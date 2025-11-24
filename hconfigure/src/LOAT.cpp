
#include "LOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "Configuration.hpp"
#include "CppTarget.hpp"
#include <filesystem>
#include <stack>
#include <utility>

using std::ofstream, std::filesystem::create_directories, std::ifstream, std::stack, std::lock_guard, std::mutex;

bool operator<(const LOAT &lhs, const LOAT &rhs)
{
    return lhs.name < rhs.name;
}

void LOAT::makeBuildCacheFilesDirPathAtConfigTime()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (!myBuildDir)
        {
            myBuildDir = Node::getHalfNodeST(configureNode->filePath + slashc + name);
        }
        create_directories(myBuildDir->filePath);
    }
    // set to true in dsc constructor if any of the objectFileProducers hasObjectFiles == true
    hasObjectFiles = false;
}

LOAT::LOAT(Configuration &config_, const string &name_, const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), nullptr, targetType, name_, false, false)
{
    makeBuildCacheFilesDirPathAtConfigTime();
}

LOAT::LOAT(Configuration &config_, const bool buildExplicit, const string &name_, const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), nullptr, targetType, name_, buildExplicit, false)
{
    makeBuildCacheFilesDirPathAtConfigTime();
}

LOAT::LOAT(Configuration &config_, Node *myBuildDir_, const string &name_, const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), myBuildDir_, targetType, name_, false, false),
      myBuildDir(myBuildDir_)
{
    makeBuildCacheFilesDirPathAtConfigTime();
}

LOAT::LOAT(Configuration &config_, Node *myBuildDir_, const bool buildExplicit, const string &name_,
           const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), myBuildDir_, targetType, name_, buildExplicit, false),
      myBuildDir(myBuildDir_)
{
    makeBuildCacheFilesDirPathAtConfigTime();
}

void LOAT::setOutputName(string str)
{
#ifndef BUILD_MODE
    outputName = std::move(str);
#endif
}

BTargetType LOAT::getBTargetType() const
{
    return BTargetType::LINK_OR_ARCHIVE_TARGET;
}

void LOAT::setFileStatus()
{
    RealBTarget &rb = realBTargets[0];
    for (const ObjectFileProducer *objectFileProducer : objectFileProducers)
    {
        objectFileProducer->getObjectFiles(&objectFiles);
    }

    if (objectFiles.empty())
    {
        if (evaluate(TargetType::LIBRARY_STATIC))
        {
            rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
            return;
        }
        printErrorMessage(FORMAT("Target {} has no object-files.\n", name));
        // TODO
        // Throw Exception. Shared Library or Executable cannot have zero object-files.
    }

    setLinkOrArchiveCommands();

    if (rb.updateStatus != UpdateStatus::NEEDS_UPDATE)
    {
        if (outputFileNode->fileType == file_type::not_found)
        {
            rb.updateStatus = UpdateStatus::NEEDS_UPDATE;
        }
        else
        {
            if (linkBuildCache.commandWithoutArgumentsWithTools.hash == commandWithoutTargetsWithTool.getHash())
            {
                bool needsUpdate = false;
                if (!evaluate(TargetType::LIBRARY_STATIC))
                {

                    for (const uint32_t index : reqDepsVecIndices)
                    {
                        PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);

                        // No need to check whether ploat is a static-library since it is
                        // already-checked in that target's setFileStatus.

                        if (reqDep->getBTargetType() == BTargetType::LINK_OR_ARCHIVE_TARGET &&
                            static_cast<LOAT *>(reqDep)->objectFiles.empty())
                        {
                            continue;
                        }
                        if (reqDep->outputFileNode->lastWriteTime > outputFileNode->lastWriteTime)
                        {
                            needsUpdate = true;
                            break;
                        }
                    }
                }

                for (const ObjectFile *objectFile : objectFiles)
                {
                    if (std::ranges::find(linkBuildCache.objectFiles, objectFile->objectNode) ==
                        linkBuildCache.objectFiles.end())
                    {
                        needsUpdate = true;
                        break;
                    }

                    if (objectFile->objectNode->lastWriteTime > outputFileNode->lastWriteTime)
                    {
                        needsUpdate = true;
                        break;
                    }
                }
                if (needsUpdate)
                {
                    rb.updateStatus = UpdateStatus::NEEDS_UPDATE;
                }
            }
            else
            {
                rb.updateStatus = UpdateStatus::NEEDS_UPDATE;
            }
        }
    }

    if constexpr (os == OS::NT)
    {
        if (linkTargetType == TargetType::EXECUTABLE &&
            config.ploatFeatures.copyToExeDirOnNtOs == CopyDLLToExeDirOnNTOs::YES &&
            rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
        {
            flat_hash_set<PLOAT *> checked;
            // TODO:
            // Use vector instead and call reserve before
            stack<PLOAT *, vector<PLOAT *>> allDeps;

            for (const uint32_t index : reqDepsVecIndices)
            {
                PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);
                checked.emplace(reqDep);
                allDeps.emplace(reqDep);
            }
            while (!allDeps.empty())
            {
                PLOAT *ploat = allDeps.top();
                allDeps.pop();
                if (ploat->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (rb.updateStatus == UpdateStatus::UPDATED)
                    {
                        // latest dll will be built and copied
                        dllsToBeCopied.emplace_back(ploat);
                    }
                    else
                    {
                        // latest dll exists, but it might not have been copied in the previous invocation.

                        if (const Node *copiedDLLNode = Node::getNode(
                                string(getOutputDirectoryV()) + slashc + ploat->getActualOutputName(), true, true);
                            copiedDLLNode->fileType == file_type::not_found)
                        {
                            dllsToBeCopied.emplace_back(ploat);
                        }
                        else
                        {
                            if (copiedDLLNode->lastWriteTime < ploat->outputFileNode->lastWriteTime)
                            {
                                dllsToBeCopied.emplace_back(ploat);
                            }
                        }
                    }
                }
                for (const uint32_t index : ploat->reqDepsVecIndices)
                {
                    PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);
                    if (checked.emplace(reqDep).second)
                    {
                        allDeps.emplace(reqDep);
                    }
                }
            }
        }
    }
}

void LOAT::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    PLOAT::updateBTarget(builder, round, isComplete);
    if (RealBTarget &realBTarget = realBTargets[round];
        !round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        setFileStatus();
        if (RealBTarget &rb = realBTargets[0]; rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
        {
            rb.assignNeedsUpdateToDependents();

            RunCommand r;
            r.startProcess(linkWithTargets, false);
            auto [output, exitStatus] = r.endProcess(false);
            realBTarget.exitStatus = exitStatus;

            atomic_ref(realBTargets[0].updateStatus).store(UpdateStatus::UPDATED, std::memory_order_release);
            string outputStr;
            if (isConsole)
            {
                if (linkTargetType == TargetType::LIBRARY_STATIC)
                {
                    outputStr += getColorCode(ColorIndex::dark_khaki);
                }
                else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
                {
                    outputStr += getColorCode(ColorIndex::orange);
                }
            }

            if (output.empty())
            {
                string str;
                if (linkTargetType == TargetType::LIBRARY_STATIC)
                {
                    str = "Static-Lib";
                }
                else if (linkTargetType == TargetType::LIBRARY_SHARED)
                {
                    str = "Shared-Lib";
                }
                else
                {
                    str = "Executable";
                }
                outputStr += FORMAT("{} {} ", str, name);
            }
            else
            {
                outputStr += linkWithTargets;
            }

            outputStr += threadIds[myThreadIndex];
            if (isConsole)
            {
                outputStr += getColorCode(ColorIndex::reset);
            }

            outputStr += output;
            {
                std::lock_guard _(printMutex);
                fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
            }

            if constexpr (os == OS::NT)
            {
                if (linkTargetType == TargetType::EXECUTABLE &&
                    config.ploatFeatures.copyToExeDirOnNtOs == CopyDLLToExeDirOnNTOs::YES &&
                    realBTarget.exitStatus == EXIT_SUCCESS)
                {
                    for (const PLOAT *ploat : dllsToBeCopied)
                    {
                        copy_file(ploat->outputFileNode->filePath,
                                  string(getOutputDirectoryV()) + slashc + ploat->getActualOutputName(),
                                  std::filesystem::copy_options::overwrite_existing);
                    }
                }
            }
        }
    }
    else if (round == 1)
    {
        if constexpr (bsMode == BSMode::BUILD)
        {
            readCacheAtBuildTime();
        }
        if (!evaluate(TargetType::LIBRARY_STATIC))
        {
            for (const uint32_t index : reqDepsVecIndices)
            {
                const PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);
                reqLinkerFlags += reqDep->useReqLinkerFlags;
            }
        }

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeCacheAtConfigureTime();
        }
    }
}

bool LOAT::writeBuildCache(vector<char> &buffer)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        return PLOAT::writeBuildCache(buffer);
    }

    if (atomic_ref(realBTargets[0].updateStatus).load(std::memory_order_acquire) != UpdateStatus::UPDATED ||
        realBTargets[0].exitStatus != EXIT_SUCCESS)
    {
        return PLOAT::writeBuildCache(buffer);
    }

    linkBuildCache.commandWithoutArgumentsWithTools.hash = commandWithoutTargetsWithTool.getHash();
    linkBuildCache.commandWithoutArgumentsWithTools.serialize(buffer);
    writeUint32(buffer, objectFiles.size());
    for (const ObjectFile *obj : objectFiles)
    {
        writeNode(buffer, obj->objectNode);
    }
    return true;
}

void LOAT::writeCacheAtConfigureTime()
{
    writeNode(configCacheBuffer, myBuildDir);
    fileTargetCaches[cacheIndex].configCache = string_view(configCacheBuffer.data(), configCacheBuffer.size());
}

void LOAT::readCacheAtBuildTime()
{
    myBuildDir = readHalfNode(fileTargetCaches[cacheIndex].configCache.data(), configCacheBytesRead);
    if (fileTargetCaches[cacheIndex].configCache.size() != configCacheBytesRead)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }

    const string_view buildCache = fileTargetCaches[cacheIndex].buildCache;
    if (!buildCache.empty())
    {
        uint32_t bytesRead = 0;
        linkBuildCache.commandWithoutArgumentsWithTools.deserialize(buildCache.data(), bytesRead);
        const uint32_t objCacheSize = readUint32(buildCache.data(), bytesRead);
        linkBuildCache.objectFiles.reserve(objCacheSize);
        for (uint32_t i = 0; i < objCacheSize; ++i)
        {
            linkBuildCache.objectFiles.emplace_back(readHalfNode(buildCache.data(), bytesRead));
        }
        if (bytesRead != buildCache.size())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
    }
}

string LOAT::getPrintName() const
{
    string str;
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        str = "Static Library";
    }
    else if (linkTargetType == TargetType::LIBRARY_SHARED)
    {
        str = "Shared Library";
    }
    else
    {
        str = "Executable";
    }
    return str + " " + configureNode->filePath + slashc + name;
}

void LOAT::setLinkOrArchiveCommands()
{
    const LinkerFlags &flags = config.linkerFlags;
    const Linker &linker = config.linkerFeatures.linker;
    const Archiver &archiver = config.linkerFeatures.archiver;

    linkWithTargets.reserve(4 * 1024);

    linkWithTargets = "\"";
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        linkWithTargets += config.linkerFeatures.archiver.bTPath + "\" ";
    }
    else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
    {
        linkWithTargets += config.linkerFeatures.linker.bTPath + "\" ";
    }

    // following 2 are appended with an extra "
    string archiveOutputFlag;
    string libDirFlag;

    if (archiver.bTFamily == BTFamily::MSVC)
    {
        archiveOutputFlag = "/OUT:\"";
        libDirFlag = "/LIBPATH:\"";
    }
    else if (archiver.bTFamily == BTFamily::GCC)
    {

        archiveOutputFlag = " rcs \"";
        libDirFlag = "-L\"";
    }

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        linkWithTargets += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
        linkWithTargets += archiveOutputFlag;
        linkWithTargets += outputFileNode->filePath + "\" ";
    }
    else
    {
        linkWithTargets += linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";

        // TODO Not catering for MSVC
        // Temporary Just for ensuring link success with clang Address-Sanitizer
        // There should be no spaces after user-provided-flags.
        // TODO shared libraries not supported.
        if (linker.bTFamily == BTFamily::GCC)
        {
            linkWithTargets += flags.OPTIONS + " " + flags.OPTIONS_LINK + " ";
        }
        else if (linker.bTFamily == BTFamily::MSVC)
        {
            for (const vector<string_view> sp = split(flags.FINDLIBS_SA_LINK, ' '); const string_view &s : sp)
            {
                if (s.empty())
                {
                    continue;
                }
                linkWithTargets += string(s) + ".lib ";
            }
            linkWithTargets += flags.LINKFLAGS_LINK + flags.LINKFLAGS_MSVC;
        }
        linkWithTargets += reqLinkerFlags + " ";
    }

    uint64_t commandWithoutTargetsSize = linkWithTargets.size();

    for (const ObjectFile *objectFile : objectFiles)
    {
        linkWithTargets += '\"' + objectFile->objectNode->filePath + "\" ";
    }

    if (linkTargetType != TargetType::LIBRARY_STATIC)
    {
        for (const uint32_t index : reqDepsVecIndices)
        {
            PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);
            if (reqDep->getBTargetType() == BTargetType::LINK_OR_ARCHIVE_TARGET &&
                static_cast<LOAT *>(reqDep)->objectFiles.empty())
            {
                continue;
            }

            if (linker.bTFamily == BTFamily::MSVC)
            {
                linkWithTargets += '\"';
                linkWithTargets += string(reqDep->getOutputDirectoryV());
                linkWithTargets += slashc;
                linkWithTargets += reqDep->getOutputName() + ".lib\" ";
            }
            else
            {
                linkWithTargets += "-L\"";
                linkWithTargets += string(reqDep->getOutputDirectoryV());
                linkWithTargets += "\" -l\"";
                linkWithTargets += reqDep->getOutputName();
                linkWithTargets += "\" ";
            }
        }

        for (const LibDirNode &libDirNode : reqLibraryDirs)
        {
            linkWithTargets += libDirFlag;
            linkWithTargets += libDirNode.node->filePath;
            linkWithTargets += "\" ";
        }

        if (config.linkerFeatures.evaluate(BTFamily::GCC))
        {
            for (const uint32_t index : reqDepsVecIndices)
            {
                if (const PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);
                    reqDep->evaluate(TargetType::LIBRARY_SHARED))
                {
                    linkWithTargets += "-Wl," + flags.RPATH_OPTION_LINK + " " + "-Wl,\"" +
                                       string(reqDep->getOutputDirectoryV()) + "\" ";
                }
            }
        }

        if (config.linkerFeatures.evaluate(BTFamily::GCC) && evaluate(TargetType::EXECUTABLE) && flags.isRpathOs)
        {
            for (const uint32_t index : reqDepsVecIndices)
            {
                if (const PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);
                    reqDep->evaluate(TargetType::LIBRARY_SHARED))
                {
                    linkWithTargets += "-Wl,-rpath-link -Wl,\"" + string(reqDep->getOutputDirectoryV()) + "\" ";
                }
            }
        }

        // -fPIC
        if (!config.linkerFeatures.OR(TargetOS::WINDOWS, TargetOS::CYGWIN))
        {

            linkWithTargets += "-fPIC ";
        }

        linkWithTargets += linker.bTFamily == BTFamily::MSVC ? " /OUT:\"" : " -o \"";
        linkWithTargets += outputFileNode->filePath + "\" ";

        if (linkTargetType == TargetType::LIBRARY_SHARED)
        {
            linkWithTargets += linker.bTFamily == BTFamily::MSVC ? "/DLL  " : " -shared ";
        }
    }

    commandWithoutTargetsWithTool.setCommand({linkWithTargets.data(), commandWithoutTargetsSize});
}