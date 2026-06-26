
#include "CustomCodeGenerator.hpp"

#include "Builder.hpp"

HeaderGen::HeaderGen(const string &name, LOAT *codeGenerator_, const string &macroName, const string &macroValueFile)
    : BTarget(name, rapidhash(name.data(), name.size()), true, BTargetType::UNKNOWN), codeGenerator(codeGenerator_)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        myBuildDir = Node::getHalfNode(configureNode->filePath + slashc + name);
        create_directories(myBuildDir->filePath);
        sourceNode = Node::getNodeNonNormalized(macroValueFile, true, false);
        outputHeader = Node::getHalfNode(myBuildDir->filePath + slashc + string("output.h"));
    }
    else
    {
        realBTargets[0].addDep<BTargetType::LOAT, RelationType::FULL>(&codeGenerator->realBTargets[0]);

        const string_view configCache = bTargetCaches[cacheIndex].configCache;

        {
            uint32_t bytesRead = 0;
            // reading config-cache
            myBuildDir = readHalfNode(configCache.data(), bytesRead);
            sourceNode = readHalfNode(configCache.data(), bytesRead);
            sourceNode->doStatFile = true;
            outputHeader = readHalfNode(configCache.data(), bytesRead);

            command = codeGenerator->outputFileNode->filePath;
            command += ' ';
            command += macroName;
            command += ' ';
            command += sourceNode->filePath;
            command += ' ';
            command += outputHeader->filePath;

            realBTargets[0].cumulativeHash = rapidhash(command.c_str(), command.size());
        }
    }
}

bool HeaderGen::isEventRegistered(Builder &builder)
{
    RealBTarget &rb = realBTargets[0];
    if (!selectiveBuild || rb.exitStatus != EXIT_SUCCESS)
    {
        return false;
    }

    if (rb.updateStatus == UpdateStatus::UNCHECKED)
    {
        setUpdateStatus();
    }

    if (rb.updateStatus != UpdateStatus::UPDATE_NEEDED)
    {
        if (realBTargets[0].launchTime > sourceNode->lastWriteTime)
        {
            return false;
        }
    }

    rb.launchTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    run.startAsyncProcess(command.c_str(), builder, this, false);
    return true;
}

bool HeaderGen::isEventCompleted(Builder &builder, string_view message)
{
    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        realBTargets[0].updateStatus = UpdateStatus::UPDATE_NOT_NEEDED;
        buildFooterUpdated = true;
    }

    string outputStr;
    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::cyan);
    }

    if (run.output->empty())
    {
        outputStr += FORMAT("[{}/{}]HeaderGen {} -> {} {}\n", builder.updatedCount, builder.updateBTargetsSizeGoal,
                            sourceNode->filePath, outputHeader->getFileName(), name);
    }
    else
    {
        outputStr.push_back('\n');
    }

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    if (!run.output->empty())
    {
        outputStr += *run.output;
        outputStr.push_back('\n');
    }
    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
    return false;
}

void HeaderGen::writeConfigCacheAtConfigTime(string &buffer)
{
    writeNode(buffer, myBuildDir);
    writeNode(buffer, sourceNode);
    writeNode(buffer, outputHeader);
}
