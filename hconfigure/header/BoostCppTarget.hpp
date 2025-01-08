
#ifndef BOOSTCPPTARGET_HPP
#define BOOSTCPPTARGET_HPP

#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"

enum class BoostExampleOrTestType : char
{
    RUN_TEST,
    COMPILE_TEST,
    COMPILE_FAIL_TEST,
    LINK_TEST,
    LINK_FAIL_TEST,
    EXAMPLE,
    RUN_EXAMPLE,
};

struct ExampleOrTest
{
    DSC<CppSourceTarget> *dscTarget;
    BoostExampleOrTestType targetType;
};

struct BoostCppTarget : TargetCache
{
    Configuration *configuration = nullptr;
    BTarget *testTarget = nullptr;
    BTarget *examplesTarget = nullptr;
    DSC<CppSourceTarget> &mainTarget;
    vector<ExampleOrTest> examplesOrTests;

    struct RunTests
    {
        const vector<ExampleOrTest> &examplesOrTests;
        const ExampleOrTest *exampleOrTest;

        explicit RunTests(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        ExampleOrTest &operator*() const;
        bool operator!=(const RunTests &other) const;
        RunTests end() const;
        RunTests begin();
        RunTests operator++();
    };

    struct RunTestsBuildMode : RunTests
    {
        explicit RunTestsBuildMode(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        RunTestsBuildMode begin();
        RunTestsBuildMode operator++();
    };

    struct RunTestsBuildModeEndsWith : RunTests
    {
        pstring endsWith;
        explicit RunTestsBuildModeEndsWith(pstring endsWith_, const vector<ExampleOrTest> &examplesOrTests_,
                                           const ExampleOrTest *exampleOrTest_);
        RunTestsBuildModeEndsWith begin();
        RunTestsBuildModeEndsWith operator++();
    };

    struct CompileTests : RunTests
    {
        explicit CompileTests(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        CompileTests begin();
        CompileTests operator++();
    };

    struct CompileTestsBuildMode : RunTests
    {
        explicit CompileTestsBuildMode(const vector<ExampleOrTest> &exampleOrTests_,
                                       const ExampleOrTest *exampleOrTest_);
        CompileTestsBuildMode begin();
        CompileTestsBuildMode operator++();
    };

    struct CompileFailTests : RunTests
    {
        explicit CompileFailTests(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        CompileFailTests begin();
        CompileFailTests operator++();
    };

    struct CompileFailTestsBuildMode : RunTests
    {
        explicit CompileFailTestsBuildMode(const vector<ExampleOrTest> &exampleOrTests_,
                                           const ExampleOrTest *exampleOrTest_);
        CompileFailTestsBuildMode begin();
        CompileFailTestsBuildMode operator++();
    };

    struct LinkTests : RunTests
    {
        explicit LinkTests(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        LinkTests begin();
        LinkTests operator++();
    };

    struct LinkTestsBuildMode : RunTests
    {
        explicit LinkTestsBuildMode(const vector<ExampleOrTest> &exampleOrTests_, const ExampleOrTest *exampleOrTest_);
        LinkTestsBuildMode begin();
        LinkTestsBuildMode operator++();
    };

    struct LinkFailTestsBuildMode : RunTests
    {
        explicit LinkFailTestsBuildMode(const vector<ExampleOrTest> &exampleOrTests_,
                                        const ExampleOrTest *exampleOrTest_);
        LinkFailTestsBuildMode begin();
        LinkFailTestsBuildMode operator++();
    };

    struct Examples : RunTests
    {
        explicit Examples(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        Examples begin();
        Examples operator++();
    };

    struct ExamplesBuildMode : RunTests
    {
        explicit ExamplesBuildMode(const vector<ExampleOrTest> &exampleOrTests_, const ExampleOrTest *exampleOrTest_);
        ExamplesBuildMode begin();
        ExamplesBuildMode operator++();
    };

    struct RunExamples : RunTests
    {
        explicit RunExamples(const vector<ExampleOrTest> &RunExamplesOrTests_, const ExampleOrTest *exampleOrTest_);
        RunExamples begin();
        RunExamples operator++();
    };

    struct RunExamplesBuildMode : RunTests
    {
        explicit RunExamplesBuildMode(const vector<ExampleOrTest> &exampleOrTests_,
                                      const ExampleOrTest *exampleOrTest_);
        RunExamplesBuildMode begin();
        RunExamplesBuildMode operator++();
    };

    BoostCppTarget(const pstring &name, Configuration *configuration_, bool headerOnly, bool createTestsTarget = false,
                   bool createExamplesTarget = false);
    ~BoostCppTarget();
    void addDirectoryGeneric(const pstring &dir, const pstring &nameThirdPart, BoostExampleOrTestType exampleOrTestType,
                             bool buildExplicit);
    void addTestDirectory(const pstring &dir);
    void addTestDirectoryEndsWith(const pstring &dir, const pstring &endsWith);
    void addRunExamplesDirectory(const pstring &dir);
    RunTestsBuildMode getRunTestsBuildMode();
    RunTestsBuildModeEndsWith getRunTestsBuildModeEndsWith(pstring endsWith);
    /*void addTestDirectory(const pstring &dir, const pstring &prefix);
    void addExampleDirectory(const pstring &dir);*/
};

#endif // BOOSTCPPTARGET_HPP
