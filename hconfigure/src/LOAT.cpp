
#ifdef USE_HEADER_UNITS
import "LOAT.hpp";
import "BuildSystemFunctions.hpp";
import "Cache.hpp";
import "CppSourceTarget.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <stack>;
import <utility>;
#else
#include "LOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <stack>
#include <utility>
#endif

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

    for (const ObjectFileProducer *objectFileProducer : objectFileProducers)
    {
        objectFileProducer->getObjectFiles(&objectFiles, this);
    }

    if (objectFiles.empty())
    {
        if (evaluate(TargetType::LIBRARY_STATIC))
        {
            fileStatus = false;
            return;
        }
        // TODO
        // Throw Exception. Shared Library or Executable cannot have zero object-files.
    }

    setLinkOrArchiveCommands();
    commandWithoutTargetsWithTool.setCommand(config.linkerFeatures.linker.bTPath.string() + " " +
                                             string(linkOrArchiveCommandWithoutTargets));

    if (!fileStatus)
    {
        outputFileNode->ensureSystemCheckCalled(true, true);
        if (outputFileNode->doesNotExist)
        {
            fileStatus = true;
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
                    if (std::ranges::find(linkBuildCache.objectFiles, objectFile->objectFileOutputFileNode) ==
                        linkBuildCache.objectFiles.end())
                    {
                        needsUpdate = true;
                        break;
                    }

                    if (objectFile->objectFileOutputFileNode->lastWriteTime > outputFileNode->lastWriteTime)
                    {
                        needsUpdate = true;
                        break;
                    }
                }
                if (needsUpdate)
                {
                    fileStatus = true;
                }
            }
            else
            {
                fileStatus = true;
            }
        }
    }

    if constexpr (os == OS::NT)
    {
        if (linkTargetType == TargetType::EXECUTABLE &&
            config.ploatFeatures.copyToExeDirOnNtOs == CopyDLLToExeDirOnNTOs::YES && atomic_ref(fileStatus).load())
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
                    if (atomic_ref(ploat->fileStatus).load())
                    {
                        // latest dll will be built and copied
                        dllsToBeCopied.emplace_back(ploat);
                    }
                    else
                    {
                        // latest dll exists, but it might not have been copied in the previous invocation.

                        if (const Node *copiedDLLNode = Node::getNodeFromNormalizedString(
                                string(getOutputDirectoryV()) + slashc + ploat->getActualOutputName(), true, true);
                            copiedDLLNode->doesNotExist)
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
        if (fileStatus)
        {
            assignFileStatusToDependents(0);
        }

        if (fileStatus)
        {
            shared_ptr<RunCommand> postBasicLinkOrArchive;
            if (linkTargetType == TargetType::LIBRARY_STATIC)
            {
                postBasicLinkOrArchive = std::make_shared<RunCommand>(Archive());
            }
            else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
            {
                postBasicLinkOrArchive = std::make_shared<RunCommand>(Link());
            }
            realBTarget.exitStatus = postBasicLinkOrArchive->exitStatus;

            if (postBasicLinkOrArchive->exitStatus == EXIT_SUCCESS)
            {
                updatedBuildCache.commandWithoutArgumentsWithTools.hash = commandWithoutTargetsWithTool.getHash();
                updatedBuildCache.objectFiles.reserve(objectFiles.size());
                updatedBuildCache.objectFiles.clear();
                for (const ObjectFile *objectFile : objectFiles)
                {
                    updatedBuildCache.objectFiles.emplace_back(objectFile->objectFileOutputFileNode);
                }
            }

            // We have to pass the linkBuildCache since we can not update it in multithreaded mode.
            if (linkTargetType == TargetType::LIBRARY_STATIC)
            {
                postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor, this, nullptr);
            }
            else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
            {
                postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor, this, nullptr);
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
    else if (round == 2)
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

void LOAT::updateBuildCache(void *ptr)
{
    linkBuildCache = std::move(updatedBuildCache);
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

string LOAT::getTarjanNodeName() const
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

RunCommand LOAT::Archive()
{
    return {config.linkerFeatures.archiver.bTPath, linkOrArchiveCommandWithTargets, getLinkOrArchiveCommandPrint(),
            true};
}

RunCommand LOAT::Link()
{
    return {config.linkerFeatures.linker.bTPath, linkOrArchiveCommandWithTargets, getLinkOrArchiveCommandPrint(), true};
}

void LOAT::setLinkOrArchiveCommands()
{
    const LinkerFlags &flags = config.linkerFlags;
    const Linker &linker = config.linkerFeatures.linker;
    const Archiver &archiver = config.linkerFeatures.archiver;

    linkOrArchiveCommandWithTargets.reserve(1024);

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        linkOrArchiveCommandWithTargets = archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
        auto getArchiveOutputFlag = [&]() -> string {
            if (archiver.bTFamily == BTFamily::MSVC)
            {
                return "/OUT:";
            }
            if (archiver.bTFamily == BTFamily::GCC)
            {
                return " rcs ";
            }
            return "";
        };
        linkOrArchiveCommandWithTargets += getArchiveOutputFlag();
        linkOrArchiveCommandWithTargets += addQuotes(outputFileNode->filePath) + " ";
    }
    else
    {
        linkOrArchiveCommandWithTargets = linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";

        // TODO Not catering for MSVC
        // Temporary Just for ensuring link success with clang Address-Sanitizer
        // There should be no spaces after user-provided-flags.
        // TODO shared libraries not supported.
        if (linker.bTFamily == BTFamily::GCC)
        {
            linkOrArchiveCommandWithTargets += flags.OPTIONS + " " + flags.OPTIONS_LINK + " ";
        }
        else if (linker.bTFamily == BTFamily::MSVC)
        {
            for (const string &str : split(flags.FINDLIBS_SA_LINK, " "))
            {
                if (str.empty())
                {
                    continue;
                }
                linkOrArchiveCommandWithTargets += str + ".lib ";
            }
            linkOrArchiveCommandWithTargets += flags.LINKFLAGS_LINK + flags.LINKFLAGS_MSVC;
        }
        linkOrArchiveCommandWithTargets += reqLinkerFlags + " ";
    }

    auto getLinkFlag = [&](const string &libraryPath, const string &libraryName) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return addQuotes(libraryPath + slashc + libraryName + ".lib") + " ";
        }
        return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
    };

    uint64_t commandWithoutTargetsSize = linkOrArchiveCommandWithTargets.size();

    for (const ObjectFile *objectFile : objectFiles)
    {
        linkOrArchiveCommandWithTargets += addQuotes(objectFile->objectFileOutputFileNode->filePath) + " ";
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

            linkOrArchiveCommandWithTargets += prebuiltDep->reqPreLF;
            linkOrArchiveCommandWithTargets +=
                getLinkFlag(string(ploat->getOutputDirectoryV()), ploat->getOutputName());
            linkOrArchiveCommandWithTargets += prebuiltDep->reqPostLF;
        }

        auto getLibraryDirectoryFlag = [&] {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return "/LIBPATH:";
            }
            return "-L";
        };

        for (const LibDirNode &libDirNode : reqLibraryDirs)
        {
            linkOrArchiveCommandWithTargets += getLibraryDirectoryFlag() + addQuotes(libDirNode.node->filePath) + " ";
        }

        if (evaluate(BTFamily::GCC))
        {
            for (auto &[ploat, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (ploat->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpath)
                    {
                        linkOrArchiveCommandWithTargets += "-Wl," + flags.RPATH_OPTION_LINK + " " + "-Wl," +
                                                           addQuotes(string(ploat->getOutputDirectoryV())) + " ";
                    }
                    else
                    {
                        linkOrArchiveCommandWithTargets += prebuiltDep->reqRpath;
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
                        linkOrArchiveCommandWithTargets +=
                            "-Wl,-rpath-link -Wl," + addQuotes(string(ploat->getOutputDirectoryV())) + " ";
                    }
                    else
                    {
                        linkOrArchiveCommandWithTargets += prebuiltDep->reqRpathLink;
                    }
                }
            }
        }

        // -fPIC
        if (!config.linkerFeatures.OR(TargetOS::WINDOWS, TargetOS::CYGWIN))
        {

            linkOrArchiveCommandWithTargets += "-fPIC ";
        }

        linkOrArchiveCommandWithTargets += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
        linkOrArchiveCommandWithTargets += addQuotes(outputFileNode->filePath) + " ";

        if (linkTargetType == TargetType::LIBRARY_SHARED)
        {
            linkOrArchiveCommandWithTargets += linker.bTFamily == BTFamily::MSVC ? "/DLL  " : " -shared ";
        }
    }

    linkOrArchiveCommandWithoutTargets = string_view(linkOrArchiveCommandWithTargets.data(), commandWithoutTargetsSize);
}

string LOAT::getLinkOrArchiveCommandPrint()
{
    const LinkerFlags &flags = config.linkerFlags;
    const Linker &linker = config.linkerFeatures.linker;
    const Archiver &archiver = config.linkerFeatures.archiver;

    string linkOrArchiveCommandPrint;

    const ArchiveCommandPrintSettings &acpSettings = settings.acpSettings;
    const LinkCommandPrintSettings &lcpSettings = settings.lcpSettings;

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        if (acpSettings.tool.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrint +=
                getReducedPath(archiver.bTPath.lexically_normal().string(), acpSettings.tool) + " ";
        }

        if (acpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrint += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
        }
        auto getArchiveOutputFlag = [&]() -> string {
            if (archiver.bTFamily == BTFamily::MSVC)
            {
                return "/OUT:";
            }
            if (archiver.bTFamily == BTFamily::GCC)
            {
                return " rcs ";
            }
            return "";
        };
        if (acpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrint += getArchiveOutputFlag();
        }

        if (acpSettings.archive.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrint += getReducedPath(outputFileNode->filePath, acpSettings.archive) + " ";
        }
    }
    else
    {
        if (lcpSettings.tool.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrint +=
                getReducedPath(linker.bTPath.lexically_normal().string(), lcpSettings.tool) + " ";
        }

        if (lcpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrint += linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";
        }

        if (lcpSettings.infrastructureFlags)
        {
            // TODO Not catering for MSVC
            // Temporary Just for ensuring link success with clang Address-Sanitizer
            // There should be no spaces after user-provided-flags.
            // TODO shared libraries not supported.
            if (linker.bTFamily == BTFamily::GCC)
            {
                linkOrArchiveCommandPrint += flags.OPTIONS + " " + flags.OPTIONS_LINK + " ";
            }
            else if (linker.bTFamily == BTFamily::MSVC)
            {

                for (const string &str : split(flags.FINDLIBS_SA_LINK, " "))
                {
                    if (str.empty())
                    {
                        continue;
                    }
                    linkOrArchiveCommandPrint += str + ".lib ";
                }
                linkOrArchiveCommandPrint += flags.LINKFLAGS_LINK + flags.LINKFLAGS_MSVC;
            }
        }

        if (lcpSettings.linkerFlags)
        {
            linkOrArchiveCommandPrint += reqLinkerFlags + " ";
        }
    }

    auto getLinkFlagPrint = [&](const string &libraryPath, const string &libraryName, const PathPrint &pathPrint) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return getReducedPath(libraryPath + slashc + libraryName + ".lib", pathPrint) + " ";
        }
        return "-L" + getReducedPath(libraryPath, pathPrint) + " -l" + getReducedPath(libraryName, pathPrint) + " ";
    };

    {
        const PathPrint *pathPrint;
        if (linkTargetType == TargetType::LIBRARY_STATIC)
        {
            pathPrint = &acpSettings.objectFiles;
        }
        else
        {
            pathPrint = &lcpSettings.objectFiles;
        }
        if (pathPrint->printLevel != PathPrintLevel::NO)
        {
            for (const ObjectFile *objectFile : objectFiles)
            {
                linkOrArchiveCommandPrint += objectFile->getObjectFileOutputFilePathPrint(*pathPrint) + " ";
            }
        }
    }

    if (linkTargetType != TargetType::LIBRARY_STATIC)
    {
        if (lcpSettings.libraryDependencies.printLevel != PathPrintLevel::NO)
        {
            for (auto &[ploat, prebuiltDep] : sortedPrebuiltDependencies)
            {
                linkOrArchiveCommandPrint += prebuiltDep->reqPreLF;
                linkOrArchiveCommandPrint += getLinkFlagPrint(string(ploat->getOutputDirectoryV()),
                                                              ploat->getOutputName(), lcpSettings.libraryDependencies);
                linkOrArchiveCommandPrint += prebuiltDep->reqPostLF;
            }
        }

        auto getLibraryDirectoryFlag = [&] {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return "/LIBPATH:";
            }
            return "-L";
        };

        for (const LibDirNode &libDirNode : reqLibraryDirs)
        {
            if (libDirNode.isStandard)
            {
                if (lcpSettings.standardLibraryDirs.printLevel != PathPrintLevel::NO)
                {
                    linkOrArchiveCommandPrint +=
                        getLibraryDirectoryFlag() +
                        getReducedPath(libDirNode.node->filePath, lcpSettings.standardLibraryDirs) + " ";
                }
            }
            else
            {
                if (lcpSettings.libraryDirs.printLevel != PathPrintLevel::NO)
                {
                    linkOrArchiveCommandPrint += getLibraryDirectoryFlag() +
                                                 getReducedPath(libDirNode.node->filePath, lcpSettings.libraryDirs) +
                                                 " ";
                }
            }
        }

        if (evaluate(BTFamily::GCC) && lcpSettings.libraryDependencies.printLevel != PathPrintLevel::NO)
        {
            for (auto &[ploat, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (ploat->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpath)
                    {
                        linkOrArchiveCommandPrint +=
                            "-Wl," + flags.RPATH_OPTION_LINK + " " + "-Wl," +
                            getReducedPath(ploat->getOutputDirectoryV(), lcpSettings.libraryDependencies) + " ";
                    }
                    else
                    {
                        linkOrArchiveCommandPrint += prebuiltDep->reqRpath;
                    }
                }
            }
        }

        if (config.linkerFeatures.evaluate(BTFamily::GCC) && evaluate(TargetType::EXECUTABLE) && flags.isRpathOs &&
            lcpSettings.libraryDependencies.printLevel != PathPrintLevel::NO)
        {
            for (auto &[ploat, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (ploat->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpathLink)
                    {
                        linkOrArchiveCommandPrint +=
                            "-Wl,-rpath-link -Wl," +
                            getReducedPath(ploat->getOutputDirectoryV(), lcpSettings.libraryDependencies) + " ";
                    }
                    else
                    {
                        linkOrArchiveCommandPrint += prebuiltDep->reqRpathLink;
                    }
                }
            }
        }

        if (lcpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrint += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
        }

        if (lcpSettings.binary.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrint += getReducedPath(outputFileNode->filePath, lcpSettings.binary) + " ";
        }

        if (lcpSettings.infrastructureFlags && linkTargetType == TargetType::LIBRARY_SHARED)
        {
            linkOrArchiveCommandPrint += linker.bTFamily == BTFamily::MSVC ? "/DLL" : " -shared ";
        }
    }
    return linkOrArchiveCommandPrint;
}