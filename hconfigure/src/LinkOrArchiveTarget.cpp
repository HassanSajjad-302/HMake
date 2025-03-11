
#ifdef USE_HEADER_UNITS
import "LinkOrArchiveTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Cache.hpp";
import "CppSourceTarget.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <stack>;
import <utility>;
#else
#include "LinkOrArchiveTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <stack>
#include <utility>
#endif

using std::ofstream, std::filesystem::create_directories, std::ifstream, std::stack, std::lock_guard, std::mutex;

bool operator<(const LinkOrArchiveTarget &lhs, const LinkOrArchiveTarget &rhs)
{
    return lhs.name < rhs.name;
}

void LinkOrArchiveTarget::makeBuildCacheFilesDirPathAtConfigTime(string buildCacheFilesDirPath)
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

LinkOrArchiveTarget::LinkOrArchiveTarget(Configuration &config_, const string &name_, const TargetType targetType)
    : PrebuiltLinkOrArchiveTarget(config_, getLastNameAfterSlash(name_), configureNode->filePath + slashc + name_,
                                  targetType, name_, false, false)
{
    makeBuildCacheFilesDirPathAtConfigTime("");
}

LinkOrArchiveTarget::LinkOrArchiveTarget(Configuration &config_, const bool buildExplicit, const string &name_,
                                         const TargetType targetType)
    : PrebuiltLinkOrArchiveTarget(config_, getLastNameAfterSlash(name_), configureNode->filePath + slashc + name_,
                                  targetType, name_, buildExplicit, false)
{
    makeBuildCacheFilesDirPathAtConfigTime("");
}

LinkOrArchiveTarget::LinkOrArchiveTarget(Configuration &config_, const string &buildCacheFileDirPath_,
                                         const string &name_, const TargetType targetType)
    : PrebuiltLinkOrArchiveTarget(config_, getLastNameAfterSlash(name_),
                                  configureNode->filePath + slashc + buildCacheFileDirPath_, targetType, name_, false,
                                  false)
{
    makeBuildCacheFilesDirPathAtConfigTime(configureNode->filePath + slashc + buildCacheFileDirPath_);
}

LinkOrArchiveTarget::LinkOrArchiveTarget(Configuration &config_, const string &buildCacheFileDirPath_,
                                         const bool buildExplicit, const string &name_, const TargetType targetType)
    : PrebuiltLinkOrArchiveTarget(config_, getLastNameAfterSlash(name_),
                                  configureNode->filePath + slashc + buildCacheFileDirPath_, targetType, name_,
                                  buildExplicit, false)
{
    makeBuildCacheFilesDirPathAtConfigTime(configureNode->filePath + slashc + buildCacheFileDirPath_);
}

void LinkOrArchiveTarget::setOutputName(string str)
{
#ifndef BUILD_MODE
    outputName = std::move(str);
#endif
}

BTargetType LinkOrArchiveTarget::getBTargetType() const
{
    return BTargetType::LINK_OR_ARCHIVE_TARGET;
}

void LinkOrArchiveTarget::setFileStatus()
{
    for (auto &[pre, dep] : requirementDeps)
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
            atomic_ref(fileStatus).store(false);
            return;
        }
        // TODO
        // Throw Exception. Shared Library or Executable cannot have zero object-files.
    }

    setLinkOrArchiveCommands();
    commandWithoutTargetsWithTool.setCommand(config.linkerFeatures.linker.bTPath.string() + " " +
                                             string(linkOrArchiveCommandWithoutTargets));

    namespace LinkBuild = Indices::BuildCache::LinkBuild;
    if (getBuildCache().Empty())
    {
        atomic_ref(fileStatus).store(true);
    }

    if (!atomic_ref(fileStatus).load())
    {
        outputFileNode->ensureSystemCheckCalled(true, true);
        if (outputFileNode->doesNotExist)
        {
            atomic_ref(fileStatus).store(true);
        }
        else
        {
            if (getBuildCache()[LinkBuild::commandWithoutArgumentsWithTools] == commandWithoutTargetsWithTool.getHash())
            {
                bool needsUpdate = false;
                if (!evaluate(TargetType::LIBRARY_STATIC))
                {
                    for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
                    {
                        // No need to check whether prebuiltLinkOrArchiveTarget is a static-library since it is
                        // already-checked in that target's setFileStatus.

                        if (prebuiltLinkOrArchiveTarget->getBTargetType() == BTargetType::LINK_OR_ARCHIVE_TARGET &&
                            static_cast<LinkOrArchiveTarget *>(prebuiltLinkOrArchiveTarget)->objectFiles.empty())
                        {
                            continue;
                        }
                        if (prebuiltLinkOrArchiveTarget->outputFileNode->lastWriteTime > outputFileNode->lastWriteTime)
                        {
                            needsUpdate = true;
                            break;
                        }
                    }
                }

                for (const ObjectFile *objectFile : objectFiles)
                {
                    if (!isNodeInValue(getBuildCache()[LinkBuild::objectFiles], *objectFile->objectFileOutputFilePath))
                    {
                        needsUpdate = true;
                        break;
                    }

                    if (objectFile->objectFileOutputFilePath->lastWriteTime > outputFileNode->lastWriteTime)
                    {
                        needsUpdate = true;
                        break;
                    }
                }
                if (needsUpdate)
                {
                    atomic_ref(fileStatus).store(true);
                }
            }
            else
            {
                atomic_ref(fileStatus).store(true);
            }
        }
    }

    if constexpr (os == OS::NT)
    {
        if (linkTargetType == TargetType::EXECUTABLE &&
            config.prebuiltLinkOrArchiveTargetFeatures.copyToExeDirOnNtOs == CopyDLLToExeDirOnNTOs::YES &&
            atomic_ref(fileStatus).load())
        {
            flat_hash_set<PrebuiltLinkOrArchiveTarget *> checked;
            // TODO:
            // Use vector instead and call reserve before
            stack<PrebuiltLinkOrArchiveTarget *, vector<PrebuiltLinkOrArchiveTarget *>> allDeps;
            for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
            {
                checked.emplace(prebuiltBasic);
                allDeps.emplace(prebuiltBasic);
            }
            while (!allDeps.empty())
            {
                PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget = allDeps.top();
                allDeps.pop();
                if (prebuiltLinkOrArchiveTarget->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (atomic_ref(prebuiltLinkOrArchiveTarget->fileStatus).load())
                    {
                        // latest dll will be built and copied
                        dllsToBeCopied.emplace_back(prebuiltLinkOrArchiveTarget);
                    }
                    else
                    {
                        // latest dll exists, but it might not have been copied in the previous invocation.

                        if (const Node *copiedDLLNode = Node::getNodeFromNormalizedString(
                                string(getOutputDirectoryV()) + slashc +
                                    prebuiltLinkOrArchiveTarget->getActualOutputName(),
                                true, true);
                            copiedDLLNode->doesNotExist)
                        {
                            dllsToBeCopied.emplace_back(prebuiltLinkOrArchiveTarget);
                        }
                        else
                        {
                            if (copiedDLLNode->lastWriteTime <
                                prebuiltLinkOrArchiveTarget->outputFileNode->lastWriteTime)
                            {
                                dllsToBeCopied.emplace_back(prebuiltLinkOrArchiveTarget);
                            }
                        }
                    }
                }
                for (auto &[prebuiltBasic_, prebuiltDep] : prebuiltLinkOrArchiveTarget->requirementDeps)
                {
                    if (checked.emplace(prebuiltBasic_).second)
                    {
                        allDeps.emplace(prebuiltBasic_);
                    }
                }
            }
        }
    }
}

void LinkOrArchiveTarget::updateBTarget(Builder &builder, unsigned short round)
{
    PrebuiltLinkOrArchiveTarget::updateBTarget(builder, round);
    RealBTarget &realBTarget = realBTargets[round];
    if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        setFileStatus();
        if (atomic_ref(fileStatus).load())
        {
            assignFileStatusToDependents(0);
        }

        if (atomic_ref(fileStatus).load())
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
                Value *objectFilesValue;

                namespace LinkBuild = Indices::BuildCache::LinkBuild;
                if (buildOrConfigCacheCopy.Empty())
                {
                    buildOrConfigCacheCopy.PushBack(svtogsr(name), cacheAlloc);

                    buildOrConfigCacheCopy.PushBack(commandWithoutTargetsWithTool.getHash(), cacheAlloc);
                    buildOrConfigCacheCopy.PushBack(Value(kArrayType), cacheAlloc);
                    objectFilesValue = buildOrConfigCacheCopy.End() - 1;
                }
                else
                {
                    buildOrConfigCacheCopy[LinkBuild::commandWithoutArgumentsWithTools] =
                        commandWithoutTargetsWithTool.getHash();
                    objectFilesValue = &buildOrConfigCacheCopy[LinkBuild::objectFiles];
                    objectFilesValue->Clear();
                }

                objectFilesValue->Reserve(objectFiles.size(), cacheAlloc);
                for (const ObjectFile *objectFile : objectFiles)
                {
                    objectFilesValue->PushBack(objectFile->objectFileOutputFilePath->getValue(), cacheAlloc);
                }
            }

            {
                if (linkTargetType == TargetType::LIBRARY_STATIC)
                {
                    postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor, false,
                                                                std::move(buildOrConfigCacheCopy), targetCacheIndex);
                }
                else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
                {
                    postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor, false,
                                                                std::move(buildOrConfigCacheCopy), targetCacheIndex);
                }
            }

            if constexpr (os == OS::NT)
            {
                if (linkTargetType == TargetType::EXECUTABLE &&
                    config.prebuiltLinkOrArchiveTargetFeatures.copyToExeDirOnNtOs == CopyDLLToExeDirOnNTOs::YES &&
                    realBTarget.exitStatus == EXIT_SUCCESS)
                {
                    for (const PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget : dllsToBeCopied)
                    {
                        copy_file(prebuiltLinkOrArchiveTarget->outputFileNode->filePath,
                                  string(getOutputDirectoryV()) + slashc +
                                      prebuiltLinkOrArchiveTarget->getActualOutputName(),
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
            readConfigCacheAtBuildTime();
        }
        if (!evaluate(TargetType::LIBRARY_STATIC))
        {
            for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
            {
                const PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget =
                    static_cast<PrebuiltLinkOrArchiveTarget *>(prebuiltBasic);
                requirementLinkerFlags += prebuiltLinkOrArchiveTarget->usageRequirementLinkerFlags;
            }
        }

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeTargetConfigCacheAtConfigureTime();
        }
    }
}

void LinkOrArchiveTarget::writeTargetConfigCacheAtConfigureTime()
{
    buildOrConfigCacheCopy.PushBack(buildCacheFilesDirPathNode->getValue(), cacheAlloc);
    copyBackConfigCacheMutexLocked();
}

void LinkOrArchiveTarget::readConfigCacheAtBuildTime()
{
    buildCacheFilesDirPathNode = Node::getNotSystemCheckCalledNodeFromValue(
        getConfigCache()[Indices::ConfigCache::LinkConfig::buildCacheFilesDirPath]);
}

string LinkOrArchiveTarget::getTarjanNodeName() const
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

RunCommand LinkOrArchiveTarget::Archive()
{
    return {config.linkerFeatures.archiver.bTPath, linkOrArchiveCommandWithTargets, getLinkOrArchiveCommandPrint(),
            true};
}

RunCommand LinkOrArchiveTarget::Link()
{
    return {config.linkerFeatures.linker.bTPath, linkOrArchiveCommandWithTargets, getLinkOrArchiveCommandPrint(), true};
}

void LinkOrArchiveTarget::setLinkOrArchiveCommands()
{
    const LinkerFlags &flags = config.linkerFlags;
    const Linker &linker = config.linkerFeatures.linker;
    const Archiver &archiver = config.linkerFeatures.archiver;

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
        linkOrArchiveCommandWithTargets += requirementLinkerFlags + " ";
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
        linkOrArchiveCommandWithTargets += addQuotes(objectFile->objectFileOutputFilePath->filePath) + " ";
    }

    if (linkTargetType != TargetType::LIBRARY_STATIC)
    {
        for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : sortedPrebuiltDependencies)
        {
            if (prebuiltLinkOrArchiveTarget->getBTargetType() == BTargetType::LINK_OR_ARCHIVE_TARGET &&
                static_cast<LinkOrArchiveTarget *>(prebuiltLinkOrArchiveTarget)->objectFiles.empty())
            {
                continue;
            }

            linkOrArchiveCommandWithTargets += prebuiltDep->requirementPreLF;
            linkOrArchiveCommandWithTargets += getLinkFlag(string(prebuiltLinkOrArchiveTarget->getOutputDirectoryV()),
                                                           prebuiltLinkOrArchiveTarget->getOutputName());
            linkOrArchiveCommandWithTargets += prebuiltDep->requirementPostLF;
        }

        auto getLibraryDirectoryFlag = [&] {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return "/LIBPATH:";
            }
            return "-L";
        };

        for (const LibDirNode &libDirNode : requirementLibraryDirectories)
        {
            linkOrArchiveCommandWithTargets += getLibraryDirectoryFlag() + addQuotes(libDirNode.node->filePath) + " ";
        }

        if (evaluate(BTFamily::GCC))
        {
            for (auto &[prebuiltBasic, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (prebuiltBasic->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpath)
                    {
                        auto *prebuiltLinkOrArchiveTarget = static_cast<PrebuiltLinkOrArchiveTarget *>(prebuiltBasic);
                        linkOrArchiveCommandWithTargets +=
                            "-Wl," + flags.RPATH_OPTION_LINK + " " + "-Wl," +
                            addQuotes(string(prebuiltLinkOrArchiveTarget->getOutputDirectoryV())) + " ";
                    }
                    else
                    {
                        linkOrArchiveCommandWithTargets += prebuiltDep->requirementRpath;
                    }
                }
            }
        }

        if (config.linkerFeatures.evaluate(BTFamily::GCC) && evaluate(TargetType::EXECUTABLE) && flags.isRpathOs)
        {
            for (auto &[prebuiltBasic, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (prebuiltBasic->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpathLink)
                    {
                        auto *prebuiltLinkOrArchiveTarget = static_cast<PrebuiltLinkOrArchiveTarget *>(prebuiltBasic);
                        linkOrArchiveCommandWithTargets +=
                            "-Wl,-rpath-link -Wl," +
                            addQuotes(string(prebuiltLinkOrArchiveTarget->getOutputDirectoryV())) + " ";
                    }
                    else
                    {
                        linkOrArchiveCommandWithTargets += prebuiltDep->requirementRpathLink;
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

string LinkOrArchiveTarget::getLinkOrArchiveCommandPrint()
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
            linkOrArchiveCommandPrint += requirementLinkerFlags + " ";
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
            for (auto &[prebuiltBasic, prebuiltDep] : sortedPrebuiltDependencies)
            {
                auto *prebuiltLinkOrArchiveTarget = static_cast<PrebuiltLinkOrArchiveTarget *>(prebuiltBasic);
                linkOrArchiveCommandPrint += prebuiltDep->requirementPreLF;
                linkOrArchiveCommandPrint +=
                    getLinkFlagPrint(string(prebuiltLinkOrArchiveTarget->getOutputDirectoryV()),
                                     prebuiltLinkOrArchiveTarget->getOutputName(), lcpSettings.libraryDependencies);
                linkOrArchiveCommandPrint += prebuiltDep->requirementPostLF;
            }
        }

        auto getLibraryDirectoryFlag = [&] {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return "/LIBPATH:";
            }
            return "-L";
        };

        for (const LibDirNode &libDirNode : requirementLibraryDirectories)
        {
            if (libDirNode.isStandard)
            {
                if (lcpSettings.standardLibraryDirectories.printLevel != PathPrintLevel::NO)
                {
                    linkOrArchiveCommandPrint +=
                        getLibraryDirectoryFlag() +
                        getReducedPath(libDirNode.node->filePath, lcpSettings.standardLibraryDirectories) + " ";
                }
            }
            else
            {
                if (lcpSettings.libraryDirectories.printLevel != PathPrintLevel::NO)
                {
                    linkOrArchiveCommandPrint +=
                        getLibraryDirectoryFlag() +
                        getReducedPath(libDirNode.node->filePath, lcpSettings.libraryDirectories) + " ";
                }
            }
        }

        if (evaluate(BTFamily::GCC) && lcpSettings.libraryDependencies.printLevel != PathPrintLevel::NO)
        {
            for (auto &[prebuiltBasic, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (prebuiltBasic->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpath)
                    {
                        auto *prebuiltLinkOrArchiveTarget = static_cast<PrebuiltLinkOrArchiveTarget *>(prebuiltBasic);
                        linkOrArchiveCommandPrint += "-Wl," + flags.RPATH_OPTION_LINK + " " + "-Wl," +
                                                     getReducedPath(prebuiltLinkOrArchiveTarget->getOutputDirectoryV(),
                                                                    lcpSettings.libraryDependencies) +
                                                     " ";
                    }
                    else
                    {
                        linkOrArchiveCommandPrint += prebuiltDep->requirementRpath;
                    }
                }
            }
        }

        if (config.linkerFeatures.evaluate(BTFamily::GCC) && evaluate(TargetType::EXECUTABLE) && flags.isRpathOs &&
            lcpSettings.libraryDependencies.printLevel != PathPrintLevel::NO)
        {
            for (auto &[prebuiltBasic, prebuiltDep] : sortedPrebuiltDependencies)
            {
                if (prebuiltBasic->evaluate(TargetType::LIBRARY_SHARED))
                {
                    if (prebuiltDep->defaultRpathLink)
                    {
                        auto *prebuiltLinkOrArchiveTarget = static_cast<PrebuiltLinkOrArchiveTarget *>(prebuiltBasic);
                        linkOrArchiveCommandPrint += "-Wl,-rpath-link -Wl," +
                                                     getReducedPath(prebuiltLinkOrArchiveTarget->getOutputDirectoryV(),
                                                                    lcpSettings.libraryDependencies) +
                                                     " ";
                    }
                    else
                    {
                        linkOrArchiveCommandPrint += prebuiltDep->requirementRpathLink;
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