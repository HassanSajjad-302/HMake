#include <utility>

#include "Configure.hpp"

inline constexpr uint32_t litExeTypeEnumSize = 3;
enum class LitExeType : uint8_t
{
    LLC = 0,
    OPT = 1,
    FILE_CHECK = 2,
};
inline constexpr std::array<const char *, 3> exeNames = {"llc", "opt", "FileCheck"};
struct LitCommand
{
    LitExeType exeType;
    string command;
    string inputFile;
    string outputFile;
};

// I want the python lit to print this data in file. And then HMake will run these using IPC.
struct LitTest
{
    // path to the .ll file. If the test fails, HMake will execute this test using the lit tool.
    string testFilePath;
    // first command output will be the input for the second command and so on. last command is not expected to have any
    // output. if the test fails, the test is executed without IPC using the testFilePath. If it still fails, HMake
    // reports this as test failure, otherwise this is considered a failure due to state leakage by previous tests.
    vector<LitCommand> commands;
};

// represents an individual lit process
class LitProcess : public BTarget
{
  public:
    string exeName;
    LitExeType exeType;
    uint32_t index;
    LitProcess(string exeName_, const LitExeType exeType_, uint32_t index_)
        : exeName(std::move(exeName_)), exeType(exeType_), index(index_)
    {
    }

    bool isEventRegistered(Builder &builder) override
    {
    }
    bool isEventCompleted(Builder &builder, string_view message) override
    {
    }
};

class LitManager : public BTarget
{
  public:
    // lit python tool would output the following info on a file that HMake would read and then execute the tests using
    // IPC.
    vector<LitTest> tests;

    // list of launched-processes. process might be idling.
    array<vector<LitProcess *>, litExeTypeEnumSize> processes;
    array<vector<uint32_t *>, litExeTypeEnumSize> freedProcess;

    uint32_t currentTestIndex = 0;

    string getPrintName() const override
    {
        return "LitManager";
    }

    bool scheduleProcesses(Builder &builder)
    {
    }

    bool isEventRegistered(Builder &builder) override
    {
        uint32_t count = std::max<uint32_t>(tests.size(), cache.numberOfBuildProcesses);

        for (uint32_t litExeType = 0; litExeType < litExeTypeEnumSize; ++litExeType)
        {
            processes[litExeType].reserve(count);
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            uint8_t processTypeIndex = static_cast<uint8_t>(tests[count].commands[0].exeType);
            const uint32_t currentSize = processes[processTypeIndex].size();
            LitProcess *litProcess = processes[processTypeIndex].emplace_back(
                new LitProcess(exeNames[processTypeIndex], static_cast<LitExeType>(processTypeIndex), currentSize));

            // Now soon, LitProcess::isEventRegistered will be called for these LitProcess.
            builder.updateBTargets.emplace_back(&litProcess->realBTargets[0]);
        }
        builder.updateBTargetsSizeGoal += count;

        // We return false as we did not launch a new process. We just scheduled a few LitProcess in build-system queue.
        return false;
    }

    // This function is not used as this BTarget did not launch a new process.
    bool isEventCompleted(Builder &builder, string_view message) override
    {
    }
};

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
