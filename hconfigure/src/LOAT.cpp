
#include "LOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "CacheWriteManager.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <stack>
#include <utility>

using std::ofstream, std::filesystem::create_directories, std::ifstream, std::stack, std::lock_guard, std::mutex;

bool operator<(const LOAT &lhs, const LOAT &rhs)
{
    return lhs.name < rhs.name;
}

void LOAT::makeBuildCacheFilesDirPathAtConfigTime(string buildCacheFilesDirPath)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (buildCacheFilesDirPath.empty())
        {
            buildCacheFilesDirPath = configureNode->filePath + slashc + name;
        }
        create_directories(buildCacheFilesDirPath);
        buildCacheFilesDirPathNode = Node::addHalfNodeFromNormalizedStringSingleThreaded(buildCacheFilesDirPath);
    }
}

LOAT::LOAT(Configuration &config_, const string &name_, const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), configureNode->filePath + slashc + name_, targetType, name_, false,
            false)
{
    makeBuildCacheFilesDirPathAtConfigTime("");
}

LOAT::LOAT(Configuration &config_, const bool buildExplicit, const string &name_, const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), configureNode->filePath + slashc + name_, targetType, name_,
            buildExplicit, false)
{
    makeBuildCacheFilesDirPathAtConfigTime("");
}

LOAT::LOAT(Configuration &config_, const string &buildCacheFileDirPath_, const string &name_,
           const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), configureNode->filePath + slashc + buildCacheFileDirPath_,
            targetType, name_, false, false)
{
    makeBuildCacheFilesDirPathAtConfigTime(configureNode->filePath + slashc + buildCacheFileDirPath_);
}

LOAT::LOAT(Configuration &config_, const string &buildCacheFileDirPath_, const bool buildExplicit, const string &name_,
           const TargetType targetType)
    : PLOAT(config_, getLastNameAfterSlash(name_), configureNode->filePath + slashc + buildCacheFileDirPath_,
            targetType, name_, buildExplicit, false)
{
    makeBuildCacheFilesDirPathAtConfigTime(configureNode->filePath + slashc + buildCacheFileDirPath_);
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
    for (auto &[pre, dep] : reqDeps)
    {
        sortedPrebuiltDependencies.emplace(pre, &dep);
    }

    RealBTarget &rb = realBTargets[0];
    for (const ObjectFileProducer *objectFileProducer : objectFileProducers)
    {
        objectFileProducer->getObjectFiles(&objectFiles, this);
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
                    for (auto &[ploat, prebuiltDep] : reqDeps)
                    {
                        // No need to check whether ploat is a static-library since it is
                        // already-checked in that target's setFileStatus.

                        if (ploat->getBTargetType() == BTargetType::LINK_OR_ARCHIVE_TARGET &&
                            static_cast<LOAT *>(ploat)->objectFiles.empty())
                        {
                            continue;
                        }
                        if (ploat->outputFileNode->lastWriteTime > outputFileNode->lastWriteTime)
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
            for (auto &[ploat, prebuiltDep] : reqDeps)
            {
                checked.emplace(ploat);
                allDeps.emplace(ploat);
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

                        if (const Node *copiedDLLNode = Node::getNodeFromNormalizedString(
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
                for (auto &[ploat_, prebuiltDep] : ploat->reqDeps)
                {
                    if (checked.emplace(ploat_).second)
                    {
                        allDeps.emplace(ploat_);
                    }
                }
            }
        }
    }
}

void LOAT::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    PLOAT::updateBTarget(builder, round, isComplete);
    RealBTarget &realBTarget = realBTargets[round];
    if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        setFileStatus();
        RealBTarget &rb = realBTargets[0];
        if (rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
        {
            rb.assignFileStatusToDependents();
            thrIndex = myThreadIndex;

            RunCommand r;
            r.startProcess(linkWithTargets, false);
            auto [output, exitStatus] = r.endProcess(false);
            realBTarget.exitStatus = exitStatus;
            linkOutput = std::move(output);

            // We have to pass the linkBuildCache since we can not update it in multithreaded mode.
            if (linkTargetType == TargetType::LIBRARY_STATIC)
            {
                CacheWriteManager::addNewEntry(this, nullptr);
            }
            else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
            {
                CacheWriteManager::addNewEntry(this, nullptr);
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
            for (auto &[ploat, prebuiltDep] : reqDeps)
            {
                reqLinkerFlags += ploat->useReqLinkerFlags;
            }
        }

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeCacheAtConfigureTime();
        }
    }
}

void LOAT::updateBuildCache(void *ptr, string &outputStr, string &errorStr, bool &buildCacheModified)
{
    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        buildCacheModified = true;
        linkBuildCache.commandWithoutArgumentsWithTools.hash = commandWithoutTargetsWithTool.getHash();
        linkBuildCache.objectFiles.reserve(objectFiles.size());
        linkBuildCache.objectFiles.clear();
        for (const ObjectFile *objectFile : objectFiles)
        {
            linkBuildCache.objectFiles.emplace_back(objectFile->objectNode);
        }
    }

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        if (isConsole)
        {
            outputStr += getColorCode(ColorIndex::dark_khaki);
        }
    }
    else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
    {

        if (isConsole)
        {
            outputStr += getColorCode(ColorIndex::orange);
        }
    }

    string printCommand;
    if (linkOutput.empty())
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
        printCommand = FORMAT("{} {} ", str, name);
    }
    else
    {
        printCommand += linkWithTargets;
    }

    outputStr += printCommand;
    outputStr += threadIds[thrIndex];
    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += linkOutput;
}

void LOAT::writeBuildCache(vector<char> &buffer)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        PLOAT::writeBuildCache(buffer);
        return;
    }

    linkBuildCache.commandWithoutArgumentsWithTools.serialize(buffer);
    writeUint32(buffer, linkBuildCache.objectFiles.size());
    for (const Node *node : linkBuildCache.objectFiles)
    {
        writeNode(buffer, node);
    }
}

void LOAT::writeCacheAtConfigureTime()
{
    writeNode(configCacheBuffer, buildCacheFilesDirPathNode);
    fileTargetCaches[cacheIndex].configCache = string_view(configCacheBuffer.data(), configCacheBuffer.size());
}

void LOAT::readCacheAtBuildTime()
{
    buildCacheFilesDirPathNode = readHalfNode(fileTargetCaches[cacheIndex].configCache.data(), configCacheBytesRead);
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

    linkWithTargets.reserve(1024);

    linkWithTargets = "\"";
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        linkWithTargets += config.linkerFeatures.archiver.bTPath.generic_string() + "\" ";
    }
    else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
    {
        linkWithTargets += config.linkerFeatures.linker.bTPath.generic_string() + "\" ";
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
            for (const string &str : split(flags.FINDLIBS_SA_LINK, " "))
            {
                if (str.empty())
                {
                    continue;
                }
                linkWithTargets += str + ".lib ";
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
        for (auto &[ploat, prebuiltDep] : sortedPrebuiltDependencies)
        {
            if (ploat->getBTargetType() == BTargetType::LINK_OR_ARCHIVE_TARGET &&
                static_cast<LOAT *>(ploat)->objectFiles.empty())
            {
                continue;
            }

            linkWithTargets += prebuiltDep->reqPreLF;

            if (linker.bTFamily == BTFamily::MSVC)
            {
                linkWithTargets += '\"';
                linkWithTargets += string(ploat->getOutputDirectoryV());
                linkWithTargets += slashc;
                linkWithTargets += ploat->getOutputName() + ".lib\" ";
            }
            else
            {
                linkWithTargets += "-L\"";
                linkWithTargets += string(ploat->getOutputDirectoryV());
                linkWithTargets += "\" -l\"";
                linkWithTargets += ploat->getOutputName();
                linkWithTargets += "\" ";
            }

            linkWithTargets += prebuiltDep->reqPostLF;
        }

        for (const LibDirNode &libDirNode : reqLibraryDirs)
        {
            linkWithTargets += libDirFlag;
            linkWithTargets += libDirNode.node->filePath;
            linkWithTargets += "\" ";
        }

        if (evaluate(BTFamily::GCC))
        {
            for (auto &[ploat, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (ploat->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpath)
                    {
                        linkWithTargets += "-Wl," + flags.RPATH_OPTION_LINK + " " + "-Wl,\"" +
                                           string(ploat->getOutputDirectoryV()) + "\" ";
                    }
                    else
                    {
                        linkWithTargets += prebuiltDep->reqRpath;
                    }
                }
            }
        }

        if (config.linkerFeatures.evaluate(BTFamily::GCC) && evaluate(TargetType::EXECUTABLE) && flags.isRpathOs)
        {
            for (auto &[ploat, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (ploat->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpathLink)
                    {
                        linkWithTargets += "-Wl,-rpath-link -Wl,\"" + string(ploat->getOutputDirectoryV()) + "\" ";
                    }
                    else
                    {
                        linkWithTargets += prebuiltDep->reqRpathLink;
                    }
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