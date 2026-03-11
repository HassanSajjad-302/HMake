#include <utility>

#include "Configure.hpp"

// -----------------------------------------------------------------------------
// Serialization Format
// -----------------------------------------------------------------------------
// string_view is serialized as: [4-byte little-endian length][raw string bytes]
// No null terminator is included.
// vector is serialized as: [4-byte little-endian size][vector entry bytes]

// -----------------------------------------------------------------------------
// Overview: HMake + lit Integration
// -----------------------------------------------------------------------------
// This header defines the IPC (inter-process communication) protocol between
// HMake (the build system) and LLVM lit tools (llc, opt, FileCheck).
//
// The overall flow is:
//
//   1. lit (Python) discovers tests and serializes them as a list of LitTest
//      structs into a file.
//
//   2. HMake reads that file, launches the relevant tool processes (llc, opt,
//      FileCheck), and communicates with them over IPC using the
//      HMakeResponse / LitResponse protocol defined below.
//
//   3. Each tool process receives an HMakeResponse, executes the requested
//      command(s), and replies with a LitResponse.
//
//   4. If a test fails, HMake re-runs it in isolation (outside IPC) to
//      distinguish real failures from state-leakage failures caused by a
//      prior test dirtying the process state.

// -----------------------------------------------------------------------------
// Section 1: Test Specification (written by lit, read by HMake)
// -----------------------------------------------------------------------------

enum class LitExeType : uint8_t
{
    LLC = 0,
    OPT = 1,
    FILE_CHECK = 2,
};

/// Command and input for an `llc` invocation.
/// - command: the full llc command line (e.g. "llc -mtriple=arc")
/// - input:   the LLVM IR text to feed to llc via stdin. llc do not read the .ll file.
struct LitCommandLlc
{
    string_view command;
    string_view input;
};

/// Command and input for an `opt` invocation.
/// - command: the full opt command line (e.g. "opt -passes=mem2reg")
/// - input:   the LLVM IR text to feed to opt via stdin. opt do not read the .ll file
struct LitCommandOpt
{
    string_view command;
    string_view input;
};

/// Command for a `FileCheck` invocation.
///
/// FileCheck always follows a LitCommandLlc or LitCommandOpt in the same
/// LitTest. It implicitly receives two inputs:
///   - directives input: the original .ll test file (source of CHECK lines) which is the input to the preceding llc/opt
///   test.
///   - match input:      the stdout output of the preceding llc/opt command
///
/// FileCheck produces no output on success; a non-zero exit code means failure.
struct LitCommandFileCheck
{
    string_view command;
};

/// Holds either a llc or opt command (they share the same memory layout).
union LitCommandUnion {
    LitCommandLlc llc;
    LitCommandOpt opt;
};

/// A tagged union pairing a tool type with its command data.
struct LitCommand
{
    LitExeType exeType;
    LitCommandUnion litCommandUnion;
};

/// Describes a single lit test — one .ll file with one tool invocation and one
/// FileCheck verification step.
///
/// Written by the lit Python runner into a binary file; HMake reads and
/// executes these over IPC.
///
/// Failure handling:
///   If a test fails during IPC execution, HMake re-runs it in a fresh
///   process (outside IPC) using llvm-lit. If it fails again, it is a genuine test failure.
///   If it passes in isolation, it is a state-leakage failure caused by a
///   prior test. In that case, HMake dumps a replay file containing all inputs
///   the tool received up to that point, so you can reproduce and debug the
///   exact sequence that caused the leak.
struct LitTest
{
    /// relative path (to llvm-project/) to the .ll source file.
    /// Used for re-running the test in isolation and for error reporting.
    string_view testFilePath;

    /// The primary tool invocation: either llc or opt command.
    LitCommand command;

    /// The FileCheck verification step that consumes the tool's output.
    LitCommandFileCheck fileCheck;
};

// -----------------------------------------------------------------------------
// Section 2: IPC Protocol (HMake <-> tool processes)
// -----------------------------------------------------------------------------

/// Binary delimiter used to separate IPC messages from regular stdout.
///
/// Tool processes (llc, opt, FileCheck) may write ordinary stdout during
/// execution. HMake uses this delimiter to locate and extract the LitResponse
/// message appended at the end of the process's stdout stream.
///
/// Format: "DELIMITER" + 14 alternating 0x5A/0xA5 bytes + "DELIMITER"
/// Total length: 32 bytes.
inline const char *delimiter = "DELIMITER"
                               "\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A\xA5"
                               "DELIMITER";

/// Message sent from HMake to a tool process (llc, opt, or FileCheck).
///
/// HMake launches each tool process once and communicates with it over IPC,
/// sending one HMakeResponse and waiting for one LitResponse per round-trip.
/// The message is serialized and appended with the delimiter defined above.
struct HMakeResponse
{
    /// When true, signals the tool process to shut down cleanly.
    /// HMake sends this instead of killing the process directly, giving the
    /// tool a chance to flush or save any state before exiting.
    /// All other fields are ignored when this is true.
    bool testsCompleted = false;

    /// A single unit of work for the tool process to execute.
    struct Response
    {
        /// Path to the .ll test file (used for diagnostics and replay files).
        string_view testPath;

        /// The command line to execute (e.g. "llc -mtriple=arc").
        string_view command;

        /// stdin input for llc or opt (the LLVM IR file-text).
        string_view input;

        /// The original .ll file content, used as the directives input for
        /// FileCheck (i.e. the file containing the CHECK lines).
        /// Only meaningful for FileCheck responses; not serialized over IPC.
        string_view directivesInput;
    };

    /// Contains exactly one Response during normal IPC operation.
    ///
    /// If a test fails only under IPC (passes when re-run in isolation), it
    /// indicates a state-leakage bug in the tool. In that case, HMake kills the
    /// daemon process (as it is now in a bad state) and writes a binary replay
    /// file containing all Response messages that were sent to that process
    /// in order, serialized in the same format as originally used in IPC.
    ///
    /// The tool can then be pointed at this replay file directly to reproduce the
    /// failure deterministically.
    /// To support this, the tool must always parse this field as a loop regardless
    /// of whether it is running under IPC or from a replay file — the only
    /// difference is the number of entries (1 in IPC, N in the replay file).
    vector<Response> responses;
};

/// Message sent from a tool process back to HMake after executing a command.
///
/// The tool writes this struct to stdout, followed by a 4-byte little-endian
/// message length, followed by the delimiter. This lets HMake strip ordinary
/// stdout from the IPC payload when parsing the response.
///
/// HMake collects and displays all non-IPC stdout for a test together after
/// the test completes.
struct LitResponse
{
    /// True if the tool executed successfully (exit code 0).
    bool succeeded;

    /// Captured stdout from the tool. Empty for FileCheck.
    /// Only meaningful when succeeded is true;
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

// Identifies which LLVM tool a LitCommand targets.
inline constexpr uint32_t litExeTypeEnumSize = 3;

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
