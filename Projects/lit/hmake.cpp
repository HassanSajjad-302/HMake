#include <utility>

#include "Configure.hpp"

// string_view is 4-byte of the size of the string followed by the string itself.

// Following has data structures for specifying the tests with their steps by lit to be consumed by HMake. lit can write
// a file that HMake can read and then run these tests. This is specified by LitTests struct.

// Following also has structs for communication between HMake and lit tools like opt and llc. HMakeResponse is what
// HMake sends to these lit tools while it expects to receive LitResponse.
inline constexpr uint32_t litExeTypeEnumSize = 3;
enum class LitExeType : uint8_t
{
    LLC = 0,
    OPT = 1,
    FILE_CHECK = 2,
};

struct LitCommandLlc
{
    string_view command;
    string_view input;
};

struct LitCommandOpt
{
    string_view command;
    string_view input;
};

// LitCommandFileCheck must be preceded by LitCommandLlc or LitCommandOpt. Input to these is considered as input to the
// following as well(directives instructions) while their output will be its second input. It does not generate any
// output.
struct LitCommandFileCheck
{
    string_view command;
};

union LitCommandUnion {
    LitCommandLlc llc;
    LitCommandOpt opt;
};

struct LitCommand
{
    LitExeType exeType;
    LitCommandUnion litCommandUnion;
};

// I want the python lit to print this data in file. And then HMake will run these using IPC.
struct LitTest
{
    // path to the .ll file. If the test fails, HMake will execute this test without IPC using the lit tool. If it still
    // fails, HMake reports this as test failure, otherwise this is considered a failure due to state leakage by
    // previous tests. In that case HMake will prepare a file with write a file with all the inputs that this tools has
    // received until this point. Now, you can easily debug opt or llc passing it this file.
    string_view testFilePath;

    // opt or llc command.
    LitCommand command;

    LitCommandFileCheck fileCheck;

    // Not to be sent by the lit. It is an HMake specific variable.
    bool commandCompleted = false;
};

// 32-byte delimiter
inline const char *delimiter = "DELIMITER"
                               "\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5"
                               "DELIMITER";

// After HMake has received the above info, HMake will launch llc, opt and FileCheck binaries and will send them the
// HMakeResponse and will wait for LitResponse. HMake will append the following with the above delimiter.

struct HMakeResponse
{
    // if true the data following it does not matter. HMake sends this message to let the process save state (if any for
    // any reason) instead of killing it itself.
    bool testsCompleted = false;

    struct Response
    {
        string_view testPath;
        string_view command;
        string_view input;
        // Only valid for FileCheck. It is not even sent / serialized.
        string_view directivesInput;
    };

    // Build-system will mostly send one response. But it might send many if in future we implement batching to reduce
    // IPC cost (probably). But, importantly the lit tool like llc, opt might need to be debugged to test for the state
    // leakage bugs, so they need to have the ability to run more than one tests. In that case HMake can create a
    // response file which can be passed to the tool to debug for such errors. This will be a binary file containing the
    // following data serialized.
    vector<Response> responses;
};

// HMake after launching opt, llc and FileCheck will send the above message and then wait for the following. The
// following should be followed by the 4-byte size of the following message plus the above delimiter. So HMake can
// prune-out the normal stdout from the IPC message. HMake will display the aggregated stdout for the test after it
// finishes.
struct LitResponse
{
    // if false, the output field does not matter. There should be no output for FileCheck.
    bool succeeded;
    string_view output;
};

// The following is HMake specific. Under development / testing.
class LitManager;
inline LitManager *manager = nullptr;

struct FailedTest : BTarget
{
    string command;
    uint32_t index;
    explicit FailedTest(string command_, const uint32_t index_)
        : BTarget(true, false), command(std::move(command_)), index(index_)
    {
    }
};

// represents an individual lit process
class LitProcess : public BTarget
{
  public:
    string exeName;
    LitExeType exeType;
    uint32_t index;
    uint32_t currentIndex = 0;

    struct ExecutedTests
    {
        uint32_t testIndex;
        // The index of the step in the test
        uint32_t currentIndex;
    };

    LitProcess(string exeName_, const LitExeType exeType_, const uint32_t index_)
        : BTarget(true, false), exeName(std::move(exeName_)), exeType(exeType_), index(index_)
    {
    }

    bool isEventRegistered(Builder &builder) override
    {
        const string command = exeName + "-useIPC";
        run.startAsyncProcess(command.data(), builder, this, true);
        return true;
    }

    bool isEventCompleted(Builder &builder, const string_view message) override
    {
        LitResponse litResponse;

        uint32_t bytesRead = 0;
        litResponse.output = readStringView(message.data(), bytesRead);
        litResponse.status = static_cast<LitSuccessStatus>(readUint8(message.data(), bytesRead));

        if (bytesRead != message.size())
        {
            // should be neatly handled instead of exiting.
            printErrorMessage(FORMAT("bytesRead != message.size() for {}", exeName));
        }

        litResponse.output = *new string(litResponse.output);

        if (litResponse.status == LitSuccessStatus::SUCCESS)
        {
            // Let's execute the next step in the test
        }
        else
        {
            // builder.updateBTargets.emplace()
        }
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
    array<vector<uint32_t>, litExeTypeEnumSize> freedProcessIndices;

    // list of processes for executing failed tests.
    vector<FailedTest *> failedTests;
    vector<uint32_t> freedFailedTestIndices;

    uint32_t currentTestIndex = 0;

    string getPrintName() const override
    {
        return "LitManager";
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
            processes[processTypeIndex].emplace_back(litProcess);

            // Now soon, LitProcess::isEventRegistered will be called for these LitProcess.
            builder.updateBTargets.emplace_back(&litProcess->realBTargets[0]);
        }
        builder.updateBTargetsSizeGoal += count;

        // We return false as we did not launch a new process. We just scheduled a few LitProcess in build-system queue.
        return false;
    }

    LitProcess *getLitProcess(LitExeType type)
    {
        uint8_t processTypeIndex = static_cast<uint8_t>(type);

        if (freedProcessIndices[processTypeIndex].empty())
        {
            const uint32_t currentSize = processes[processTypeIndex].size();
            LitProcess *litProcess = processes[processTypeIndex].emplace_back(
                new LitProcess(exeNames[processTypeIndex], static_cast<LitExeType>(processTypeIndex), currentSize));
            processes[processTypeIndex].emplace_back(litProcess);
            return litProcess;
        }
        const uint32_t processIndex = freedProcessIndices[processTypeIndex].back();
        freedProcessIndices[processTypeIndex].pop_back();
        return processes[processTypeIndex][processIndex];
    }

    FailedTest *getProcess(string command)
    {
        if (freedFailedTestIndices.empty())
        {
            const uint32_t currentSize = failedTests.size();
            FailedTest *failedTest = failedTests.emplace_back(new FailedTest(std::move(command), currentSize));
            failedTests.emplace_back(failedTest);
            return failedTest;
        }

        const uint32_t failedIndex = freedFailedTestIndices.back();
        freedFailedTestIndices.pop_back();
        return failedTests[failedIndex];
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
