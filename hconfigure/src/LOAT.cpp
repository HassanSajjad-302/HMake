
#include "LOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "ObjectFileProducer.hpp"
#include "rapidhash/rapidhash.h"

#include <filesystem>
#include <memory_resource>
#include <utility>

void LOAT::makeBuildCacheFilesDirPathAtConfigTime()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (!myBuildDir)
        {
            myBuildDir = Node::getHalfNode(configureNode->filePath + slashc + name);
        }
        std::filesystem::create_directories(myBuildDir->filePath);
    }
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

void LOAT::copyRuntimeDlls() const
{
    const uint64_t previousCompletionTime = realBTargets[0].completionTime;
    const bool copyAll = previousCompletionTime == -1 || outputFileNode->fileType != file_type::regular;
    const string_view outputDirectory = getOutputDirectoryV();
    STACK_PMR_STRING(copiedDllPath, 1024)
    copiedDllPath.reserve(outputDirectory.size() + 64);
    for (const uint32_t packedDependency : cachedReqDeps)
    {
        const PLOAT *dependency =
            static_cast<PLOAT *>(bTargetCaches[PloatDepInfo::getCacheIndex(packedDependency)].bTarget);
        if (!dependency->evaluate(TargetType::LIBRARY_SHARED) &&
            !dependency->evaluate(TargetType::PLIBRARY_SHARED))
        {
            continue;
        }
        if (!copyAll && dependency->realBTargets[0].completionTime <= previousCompletionTime)
        {
            continue;
        }

        copiedDllPath.assign(outputDirectory);
        copiedDllPath += slashc;
        copiedDllPath += dependency->getActualOutputName();
        if (string_view(copiedDllPath.data(), copiedDllPath.size()) !=
            string_view(dependency->outputFileNode->filePath))
        {
            copy_file(dependency->outputFileNode->filePath, copiedDllPath,
                      std::filesystem::copy_options::overwrite_existing);
        }
    }
}

void LOAT::completeRoundOne()
{
    PLOAT::completeRoundOne();
    if constexpr (bsMode == BSMode::BUILD)
    {
        myBuildDir = readHalfNode(bTargetCaches[cacheIndex].configCache.data(), configCacheBytesRead);
        if (bTargetCaches[cacheIndex].configCache.size() != configCacheBytesRead)
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }

        STACK_PMR_STRING(linkWithoutTargets, 64 * 1024)
        setLinkOrArchiveCommands(linkWithoutTargets, true);
        linkWithoutTargets += config.linkDependenciesPrefix;
        linkWithoutTargets += config.linkCommandSuffix;
        realBTargets[0].cumulativeHash = rapidhash(linkWithoutTargets.data(), linkWithoutTargets.size());

        if constexpr (os == OS::NT)
        {
            if (linkTargetType != TargetType::EXECUTABLE ||
                config.ploatFeatures.copyToExeDirOnNtOs != CopyDLLToExeDirOnNTOs::YES ||
                realBTargets[0].updateStatus != UpdateStatus::UPDATE_NEEDED)
            {
                return;
            }

            // cachedReqDeps is already the unique, flattened PLOAT closure; another graph traversal is redundant.
            dllsToBeCopied.reserve(cachedReqDeps.size());
            const string_view outputDirectory = getOutputDirectoryV();
            STACK_PMR_STRING(copiedDllPath, 1024)
            copiedDllPath.reserve(outputDirectory.size() + 64);
            for (const uint32_t packedDependency : cachedReqDeps)
            {
                PLOAT *dependency = static_cast<PLOAT *>(
                    bTargetCaches[PloatDepInfo::getCacheIndex(packedDependency)].bTarget);
                if (!dependency->evaluate(TargetType::LIBRARY_SHARED))
                {
                    continue;
                }

                copiedDllPath.assign(outputDirectory);
                copiedDllPath += slashc;
                copiedDllPath += dependency->getActualOutputName();
                const Node *copiedDll = Node::getNode(copiedDllPath, true, true);
                if (copiedDll->fileType == file_type::not_found ||
                    copiedDll->lastWriteTime < dependency->outputFileNode->lastWriteTime)
                {
                    dllsToBeCopied.emplace_back(dependency);
                }
            }
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

void LOAT::populateObjectNodes(std::pmr::vector<Node *> &objectNodes) const
{
    STACK_PMR_VECTOR(const ObjectFileProducer *, producers, 4 * 1024);
    for (const ObjectFileProducer *root : rootObjectFileProducers)
    {
        producers.emplace_back(root);
        FOR_REQ_OBJECT_FILE_PRODUCERS(root, producer, dependency)
        {
            if (dependency.isLinkDependency())
            {
                producers.emplace_back(producer);
            }
        }
    }

    std::ranges::sort(producers);
    for (uint64_t index = 1; index < producers.size(); ++index)
    {
        if (producers[index - 1] == producers[index])
        {
            printErrorMessage(FORMAT("An object-file producer reaches a link target more than once.\n"
                                     "Link target: {}\nProducer: {}",
                                     getPrintName(), producers[index]->getPrintName()));
        }
    }

    for (const ObjectFileProducer *producer : producers)
    {
        for (const Node *prebuiltObject : producer->prebuiltObjects)
        {
            if (!prebuiltObject->statCompleted)
            {
                printErrorMessage(FORMAT("A prebuilt object was not statted during round one.\n"
                                         "Producer: {}\nObject: {}",
                                         producer->getPrintName(), prebuiltObject->filePath));
            }
            if (prebuiltObject->fileType == file_type::not_found)
            {
                printErrorMessage(FORMAT("A prebuilt object does not exist.\nProducer: {}\nObject: {}",
                                         producer->getPrintName(), prebuiltObject->filePath));
            }
        }
    }

    for (const ObjectFileProducer *root : rootObjectFileProducers)
    {
        root->getObjectFiles(objectNodes, true);
    }

    std::ranges::sort(objectNodes, {}, &Node::myId);
    for (uint64_t index = 1; index < objectNodes.size(); ++index)
    {
        if (objectNodes[index - 1]->myId == objectNodes[index]->myId)
        {
            printErrorMessage(FORMAT("A linker-input Node is supplied more than once.\n"
                                     "Link target: {}\nObject: {}",
                                     getPrintName(), objectNodes[index]->filePath));
        }
    }
}

void LOAT::setLinkOrArchiveCommands(std::pmr::string &linkWithTargets, const bool returnWithoutTargets,
                                    const span<Node *> objectNodes) const
{
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        linkWithTargets = config.archiveCommand;
    }
    else
    {
        linkWithTargets = config.linkCommand;
    }

    linkWithTargets += outputFileNode->filePath;
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        // Always build a fresh archive. The completed action atomically replaces the final output on POSIX.
        linkWithTargets += ".tmp";
    }
    linkWithTargets += "\" ";

    const BTFamily linkerFamily = config.linkerFeatures.linker.bTFamily;
    if (linkTargetType != TargetType::LIBRARY_STATIC)
    {
        for (const Node *libraryDirectory : config.toolchainLibraryDirs)
        {
            if (linkerFamily == BTFamily::MSVC)
            {
                linkWithTargets += "/LIBPATH:\"";
            }
            else if (linkerFamily == BTFamily::GCC)
            {
                linkWithTargets += "-L\"";
            }
            linkWithTargets += libraryDirectory->filePath;
            linkWithTargets += "\" ";
        }
    }

    if (returnWithoutTargets)
    {
        return;
    }

    for (const Node *objectNode : objectNodes)
    {
        linkWithTargets += '\"' + objectNode->filePath + "\" ";
    }

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        return;
    }

    linkWithTargets += config.linkDependenciesPrefix;

    if (linkTargetType == TargetType::LIBRARY_SHARED)
    {
        linkWithTargets += linkerFamily == BTFamily::MSVC ? "/DLL  " : " -shared ";
    }

    for (const uint32_t packedDependency : cachedReqDeps)
    {
        PLOAT *reqDep = static_cast<PLOAT *>(bTargetCaches[PloatDepInfo::getCacheIndex(packedDependency)].bTarget);
        if (reqDep->bTargetType == BTargetType::LOAT && !reqDep->hasObjectFiles)
        {
            continue;
        }

        if (linkerFamily == BTFamily::MSVC)
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

    if (linkerFamily == BTFamily::GCC)
    {
        for (const uint32_t packedDependency : cachedReqDeps)
        {
            if (const PLOAT *reqDep =
                    static_cast<PLOAT *>(bTargetCaches[PloatDepInfo::getCacheIndex(packedDependency)].bTarget);
                reqDep->evaluate(TargetType::LIBRARY_SHARED) || reqDep->evaluate(TargetType::PLIBRARY_SHARED))
            {
                if (os != OS::NT)
                {
                    linkWithTargets += "-Wl,-rpath -Wl,\"" + string(reqDep->getOutputDirectoryV()) + "\" ";
                }
                else
                {
                    linkWithTargets += "-Wl, -Wl,\"" + string(reqDep->getOutputDirectoryV()) + "\" ";
                }
            }
        }

        if (os != OS::NT && evaluate(TargetType::EXECUTABLE))
        {
            for (const uint32_t packedDependency : cachedReqDeps)
            {
                if (const PLOAT *reqDep =
                        static_cast<PLOAT *>(bTargetCaches[PloatDepInfo::getCacheIndex(packedDependency)].bTarget);
                    reqDep->evaluate(TargetType::LIBRARY_SHARED) || reqDep->evaluate(TargetType::PLIBRARY_SHARED))
                {
                    linkWithTargets += "-Wl,-rpath-link -Wl,\"" + string(reqDep->getOutputDirectoryV()) + "\" ";
                }
            }
        }
    }

    linkWithTargets += config.linkCommandSuffix;
}

bool LOAT::isEventRegistered(Builder &builder)
{
    if (const RealBTarget &realBTarget = realBTargets[0]; realBTarget.exitStatus == EXIT_FAILURE || !selectiveBuild)
    {
        return false;
    }

    if (!refreshUpdateStatus())
    {
        return false;
    }

    STACK_PMR_VECTOR(Node *, objectNodes, 64)
    populateObjectNodes(objectNodes);

    if (objectNodes.empty())
    {
        if (evaluate(TargetType::LIBRARY_STATIC))
        {
            realBTargets[0].updateStatus = UpdateStatus::UPDATE_NOT_NEEDED;
            return false;
        }
        // An executable/shared library may intentionally obtain every object through required static archives.
        // PLOAT::completeRoundOne() has already folded that closure into hasObjectFiles.
        if (!hasObjectFiles)
        {
            printErrorMessage(FORMAT("Link target has no object files.\nTarget: {}\n"
                                     "Hint: add sources or object-producing dependencies before linking.",
                                     name));
        }
    }

    STACK_PMR_STRING(linkWithTargets, 64 * 1024)
    setLinkOrArchiveCommands(linkWithTargets, false, objectNodes);

    if (dryRun)
    {
        printMessage(linkWithTargets + '\n');
        return false;
    }

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        // Archivers update existing archives instead of removing omitted members. Remove only a failed action's
        // temporary output; the final archive remains valid until the new one has been created successfully.
        std::error_code removeError;
        std::filesystem::remove(outputFileNode->filePath + ".tmp", removeError);
        if (removeError)
        {
            printErrorMessage(FORMAT("Could not remove a stale temporary static library.\n"
                                     "Library: {}.tmp\nError: {}",
                                     outputFileNode->filePath, removeError.message()));
        }
    }

    if (config.responseFileThreshold != 0 && linkWithTargets.size() > config.responseFileThreshold)
    {
        string responseFile = myBuildDir->filePath;
        responseFile += slashc;
        responseFile += outputFileNode->getFileName();
        responseFile += ".rsp";
        commandWithResponseFile(linkWithTargets, responseFile, config.responseFileThreshold);
    }
    run.startAsyncProcess(linkWithTargets.data(), builder, this, false);
    return true;
}

bool LOAT::isEventCompleted(Builder &builder, string_view)
{
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        const string temporaryArchive = outputFileNode->filePath + ".tmp";
        if (realBTargets[0].exitStatus == EXIT_SUCCESS)
        {
            std::error_code replaceError;
            if constexpr (os == OS::NT)
            {
                // std::filesystem::rename does not replace an existing file on Windows.
                std::filesystem::remove(outputFileNode->filePath, replaceError);
            }
            if (!replaceError)
            {
                std::filesystem::rename(temporaryArchive, outputFileNode->filePath, replaceError);
            }
            if (replaceError)
            {
                printErrorMessage(FORMAT("Could not install the newly created static library.\n"
                                         "Temporary library: {}\nLibrary: {}\nError: {}",
                                         temporaryArchive, outputFileNode->filePath, replaceError.message()));
            }
        }
        else
        {
            std::error_code ignored;
            std::filesystem::remove(temporaryArchive, ignored);
        }
    }

    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        buildFooterUpdated = true;
    }

    STACK_PMR_STRING(outputStr, 4 * 1024)
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

    if (run.output->empty())
    {
        string_view action;
        if (linkTargetType == TargetType::LIBRARY_STATIC)
        {
            action = "Static-Lib";
        }
        else if (linkTargetType == TargetType::LIBRARY_SHARED)
        {
            action = "Shared-Lib";
        }
        else
        {
            action = "Executable";
        }
        outputStr += FORMAT("[{}/{}]{} {} ", builder.updatedCount, builder.readyBTargetsSizeGoal, action, name);
    }
    else
    {
        STACK_PMR_VECTOR(Node *, objectNodes, 64)
        populateObjectNodes(objectNodes);
        STACK_PMR_STRING(linkWithTargets, 64 * 1024)
        setLinkOrArchiveCommands(linkWithTargets, false, objectNodes);

        outputStr += linkWithTargets;
    }

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += *run.output;
    outputStr.push_back('\n');
    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);

    if constexpr (os == OS::NT)
    {
        if (linkTargetType == TargetType::EXECUTABLE &&
            config.ploatFeatures.copyToExeDirOnNtOs == CopyDLLToExeDirOnNTOs::YES &&
            realBTargets[0].exitStatus == EXIT_SUCCESS)
        {
            copyRuntimeDlls();
        }
    }

    return false;
}

void LOAT::writeConfigCacheAtConfigTime(string &buffer)
{
    PLOAT::writeConfigCacheAtConfigTime(buffer);
    writeNode(buffer, myBuildDir);
}
