
#include "LOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "CppTarget.hpp"
#include "rapidhash/rapidhash.h"

#include <filesystem>
#include <memory_resource>
#include <stack>
#include <utility>

#ifndef _WIN32
#include <sys/wait.h>
#endif

using std::ofstream, std::filesystem::create_directories, std::ifstream, std::stack, std::lock_guard;

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
            myBuildDir = Node::getHalfNode(configureNode->filePath + slashc + name);
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

void LOAT::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
        return;
    }

    if (outputFileNode->fileType == file_type::not_found)
    {
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }

    PLOAT::setUpdateStatus();
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

        for (const ObjectFileProducer *objectFileProducer : objectFileProducers)
        {
            objectFileProducer->getObjectFiles(&objectFiles);
        }

        RealBTarget &rb = realBTargets[0];
        if (objectFiles.empty())
        {
            if (evaluate(TargetType::LIBRARY_STATIC))
            {
                rb.updateStatus = UpdateStatus::UPDATE_NOT_NEEDED;
            }
            else
            {
                printErrorMessage(FORMAT("Target {} has no object-files.\n", name));
            }
        }
        else
        {
            string linkWithTargets;
            if (linkTargetType == TargetType::LIBRARY_STATIC)
            {
                linkWithTargets = config.archiveCommand;
            }
            else
            {
                linkWithTargets = config.linkCommand;
            }
        }

        {
            STACK_PMR_STRING(linkWithTargets, 64 * 1024)
            setLinkOrArchiveCommands(linkWithTargets, true);
            rb.cumulativeHash = rapidhash(linkWithTargets.data(), linkWithTargets.size());
        }

        if constexpr (os == OS::NT)
        {
            if (linkTargetType == TargetType::EXECUTABLE &&
                config.ploatFeatures.copyToExeDirOnNtOs == CopyDLLToExeDirOnNTOs::YES &&
                rb.updateStatus == UpdateStatus::UPDATE_NEEDED)
            {
                flat_hash_set<PLOAT *> checked;
                // TODO:
                // Use vector instead and call reserve before
                stack<PLOAT *, vector<PLOAT *>> allDeps;

                for (const uint32_t index : reqDepsVecIndices)
                {
                    PLOAT *reqDep = static_cast<PLOAT *>(bTargetCaches[index].bTarget);
                    checked.emplace(reqDep);
                    allDeps.emplace(reqDep);
                }
                while (!allDeps.empty())
                {
                    PLOAT *ploat = allDeps.top();
                    allDeps.pop();
                    if (ploat->evaluate(TargetType::LIBRARY_SHARED))
                    {
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
                    for (const uint32_t index : ploat->reqDepsVecIndices)
                    {
                        PLOAT *reqDep = static_cast<PLOAT *>(bTargetCaches[index].bTarget);
                        if (checked.emplace(reqDep).second)
                        {
                            allDeps.emplace(reqDep);
                        }
                    }
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

void LOAT::setLinkOrArchiveCommands(std::pmr::string &linkWithTargets, const bool returnWithoutTargets) const
{
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        linkWithTargets = config.archiveCommand;
    }
    else
    {
        linkWithTargets = config.linkCommand;
    }

    linkWithTargets += outputFileNode->filePath + "\" ";

    const BTFamily linkerFamily = config.linkerFeatures.linker.bTFamily;
    if (linkTargetType != TargetType::LIBRARY_STATIC)
    {
        for (const LibDirNode &libDirNode : reqLibraryDirs)
        {
            if (linkerFamily == BTFamily::MSVC)
            {
                linkWithTargets += "/LIBPATH:\"";
            }
            else if (linkerFamily == BTFamily::GCC)
            {

                linkWithTargets += "-L\"";
            }
            linkWithTargets += libDirNode.node->filePath;
            linkWithTargets += "\" ";
        }
    }

    if (returnWithoutTargets)
    {
        return;
    }

    for (const ObjectFile *objectFile : objectFiles)
    {
        linkWithTargets += '\"' + objectFile->objectNode->filePath + "\" ";
    }

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        return;
    }

    if (linkTargetType == TargetType::LIBRARY_SHARED)
    {
        linkWithTargets += linkerFamily == BTFamily::MSVC ? "/DLL  " : " -shared ";
    }

    for (const uint32_t index : reqDepsVecIndices)
    {
        PLOAT *reqDep = static_cast<PLOAT *>(bTargetCaches[index].bTarget);
        if (reqDep->bTargetType == BTargetType::LOAT && static_cast<LOAT *>(reqDep)->objectFiles.empty())
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
        for (const uint32_t index : reqDepsVecIndices)
        {
            if (const PLOAT *reqDep = static_cast<PLOAT *>(bTargetCaches[index].bTarget);
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
            for (const uint32_t index : reqDepsVecIndices)
            {
                if (const PLOAT *reqDep = static_cast<PLOAT *>(bTargetCaches[index].bTarget);
                    reqDep->evaluate(TargetType::LIBRARY_SHARED) || reqDep->evaluate(TargetType::PLIBRARY_SHARED))
                {
                    linkWithTargets += "-Wl,-rpath-link -Wl,\"" + string(reqDep->getOutputDirectoryV()) + "\" ";
                }
            }
        }
    }
}

bool LOAT::isEventRegistered(Builder &builder)
{
    if (const RealBTarget &realBTarget = realBTargets[0]; realBTarget.exitStatus == EXIT_FAILURE || !selectiveBuild)
    {
        return false;
    }
    RealBTarget &rb = realBTargets[0];

    STACK_PMR_STRING(linkWithTargets, 64 * 1024)
    setLinkOrArchiveCommands(linkWithTargets, false);
    if (rb.updateStatus == UpdateStatus::UNCHECKED)
    {
        setUpdateStatus();
    }

    if (rb.updateStatus != UpdateStatus::UPDATE_NEEDED)
    {
        return false;
    }

    if (dryRun)
    {
        printMessage(linkWithTargets + '\n');
        return false;
    }

    realBTargets[0].launchTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    run.startAsyncProcess(linkWithTargets.c_str(), builder, this, false);
    return true;
}

bool LOAT::isEventCompleted(Builder &builder, string_view)
{
    STACK_PMR_STRING(linkWithTargets, 64 * 1024)
    setLinkOrArchiveCommands(linkWithTargets, false);

    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        buildFooterUpdated = true;
    }

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

    if (run.output->empty())
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
        outputStr += FORMAT("[{}/{}]{} {} ", builder.updatedCount, builder.updateBTargetsSizeGoal, str, name);
    }
    else
    {
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
            for (const PLOAT *ploat : dllsToBeCopied)
            {
                copy_file(ploat->outputFileNode->filePath,
                          string(getOutputDirectoryV()) + slashc + ploat->getActualOutputName(),
                          std::filesystem::copy_options::overwrite_existing);
            }
        }
    }
    return false;
}

void LOAT::writeConfigCacheAtConfigTime(string &buffer)
{
    PLOAT::writeConfigCacheAtConfigTime(buffer);
    writeNode(buffer, myBuildDir);
}