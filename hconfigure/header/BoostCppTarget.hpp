
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
    // I think union should be used here for the cases where there is only CppSourceTarget and no complete
    // DSC<CppSourceTarget>.
    DSC<CppSourceTarget> *dscTarget;
    BoostExampleOrTestType targetType;
};

enum class IteratorTargetType : char
{
    DSC_CPP,
    CPP,
    LINK,
};

class BoostCppTarget : TargetCache
{

    template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType iteratorTargetType> struct GenericBase
    {
        const vector<ExampleOrTest> &examplesOrTests;
        const ExampleOrTest *exampleOrTest;

        explicit GenericBase(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        auto &operator*() const;
        bool operator!=(const GenericBase &other) const;
        GenericBase end() const;
    };

    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
    struct Get : public GenericBase<EOT, iteratorTargetType>
    {
        explicit Get(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        Get begin();
        Get operator++();
    };

    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
    struct GetEnds : public GenericBase<EOT, iteratorTargetType>
    {
        const char *endsWith;
        explicit GetEnds(const char *endsWith_, const vector<ExampleOrTest> &examplesOrTests_,
                         const ExampleOrTest *exampleOrTest_);
        GetEnds begin();
        GetEnds operator++();
    };

  public:
    Configuration *configuration = nullptr;
    BTarget *testTarget = nullptr;
    BTarget *examplesTarget = nullptr;
    DSC<CppSourceTarget> &mainTarget;
    vector<ExampleOrTest> examplesOrTests;

    BoostCppTarget(const string &name, Configuration *configuration_, bool headerOnly, bool createTestsTarget = false,
                   bool createExamplesTarget = false);
    ~BoostCppTarget();
    void addExampleOrTestGeneric(string_view sourceDir, string_view fileName, string_view innerBuildDirName,
                                 BoostExampleOrTestType exampleOrTestType, bool buildExplicit);
    void addDirectoryGeneric(string_view dir, string_view innerBuildDirName, BoostExampleOrTestType exampleOrTestType,
                             bool buildExplicit);

    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm = BSMode::BUILD> auto get();
    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm = BSMode::BUILD>
    auto getEndsWith(const char *endsWith);

    // TODO
    // Currently, not dealing with different kinds of tests and their results and not combining them either.
    void addRunTestsDirectory(const string &sourceDir);
    void addRunTestsDirectoryEndsWith(const string &sourceDir, const string &innerBuildDirEndsWith);
    void addCompileTestsDirectory(const string &sourceDir);
    void addRunExamplesDirectory(const string &sourceDir);

    void addRunTests(string_view sourceDir, const string_view *fileName, uint64_t arraySize);
    void addCompileTests(string_view sourceDir, const string_view *fileName, uint64_t arraySize);
    void addCompileFailTests(string_view sourceDir, const string_view *fileName, uint64_t arraySize);
    void addExamples(string_view sourceDir, const string_view *fileName, uint64_t arraySize);
};

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType boostTargetType>
BoostCppTarget::GenericBase<boostExampleOrTestType, boostTargetType>::GenericBase(
    const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_)
    : examplesOrTests(examplesOrTests_), exampleOrTest(exampleOrTest_)
{
}

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType iteratorTargetType>
auto &BoostCppTarget::GenericBase<boostExampleOrTestType, iteratorTargetType>::operator*() const
{
    if constexpr ((boostExampleOrTestType == BoostExampleOrTestType::COMPILE_TEST ||
                   boostExampleOrTestType == BoostExampleOrTestType::COMPILE_FAIL_TEST) &&
                  iteratorTargetType != IteratorTargetType::LINK)
    {
        static_assert(false && "IteratorTargetType::LINK is not available when the BoostExampleOrTestType is "
                               "COMPILE_TEST or COMPILE_FAIL_TEST");
    }

    if constexpr (iteratorTargetType == IteratorTargetType::DSC_CPP)
    {
        return *exampleOrTest->dscTarget;
    }
    else if constexpr (iteratorTargetType == IteratorTargetType::CPP)
    {
        return exampleOrTest->dscTarget->getSourceTarget();
    }
    else
    {
        return exampleOrTest->dscTarget->getLinkOrArchiveTarget();
    }
}

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType boostTargetType>
bool BoostCppTarget::GenericBase<boostExampleOrTestType, boostTargetType>::operator!=(const GenericBase &other) const
{
    return exampleOrTest != other.exampleOrTest;
}

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType boostTargetType>
BoostCppTarget::GenericBase<boostExampleOrTestType, boostTargetType> BoostCppTarget::GenericBase<
    boostExampleOrTestType, boostTargetType>::end() const
{
    if (!exampleOrTest)
    {
        return GenericBase(examplesOrTests, nullptr);
    }
    return GenericBase(examplesOrTests, examplesOrTests.begin().operator->() + examplesOrTests.size());
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::Get<EOT, iteratorTargetType, bsm>::Get(const vector<ExampleOrTest> &examplesOrTests_,
                                                       const ExampleOrTest *exampleOrTest_)
    : GenericBase<EOT, iteratorTargetType>(examplesOrTests_, exampleOrTest_)
{
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::Get<EOT, iteratorTargetType, bsm> BoostCppTarget::Get<EOT, iteratorTargetType, bsm>::begin()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == bsm)
    {
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if (exampleOrTest_.targetType == EOT)
            {
                exampleOrTestLocal = &exampleOrTest_;
                break;
            }
        }
        // exampleOrTest is nullptr in the constructor, so it is assigned here.
        exampleOrTest = exampleOrTestLocal;
    }
    return Get(examplesOrTests, exampleOrTestLocal);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::Get<EOT, iteratorTargetType, bsm> BoostCppTarget::Get<EOT, iteratorTargetType, bsm>::operator++()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    if constexpr (bsMode == bsm)
    {
        ++exampleOrTest;
        for (; exampleOrTest != examplesOrTests.begin().operator->() + examplesOrTests.size(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == EOT)
            {
                return Get(examplesOrTests, exampleOrTest);
            }
        }
    }
    return Get(examplesOrTests, nullptr);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm>::GetEnds(const char *endsWith_,
                                                               const vector<ExampleOrTest> &examplesOrTests_,
                                                               const ExampleOrTest *exampleOrTest_)
    : GenericBase<EOT, iteratorTargetType>(examplesOrTests_, exampleOrTest_), endsWith(endsWith_)
{
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm> BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm>::begin()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == bsm)
    {
        string finalEndString;
        if constexpr (EOT == BoostExampleOrTestType::EXAMPLE || EOT == BoostExampleOrTestType::RUN_EXAMPLE)
        {
            finalEndString = "Examples";
        }
        else
        {
            finalEndString = "Tests";
        }
        finalEndString += slashc;
        finalEndString += endsWith;
        lowerCasePStringOnWindows(finalEndString.data(), finalEndString.size());
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if (exampleOrTest_.targetType == EOT &&
                exampleOrTest_.dscTarget->getSourceTarget().name.contains(finalEndString))
            {
                exampleOrTestLocal = &exampleOrTest_;
                break;
            }
        }
        exampleOrTest = exampleOrTestLocal;
    }
    return GetEnds(std::move(endsWith), examplesOrTests, exampleOrTestLocal);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm> BoostCppTarget::GetEnds<EOT, iteratorTargetType,
                                                                              bsm>::operator++()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    if constexpr (bsMode == bsm)
    {
        ++exampleOrTest;
        string finalEndString;
        if constexpr (EOT == BoostExampleOrTestType::EXAMPLE || EOT == BoostExampleOrTestType::RUN_EXAMPLE)
        {
            finalEndString = "Examples";
        }
        else
        {
            finalEndString = "Tests";
        }
        finalEndString += slashc;
        finalEndString += endsWith;
        lowerCasePStringOnWindows(finalEndString.data(), finalEndString.size());
        for (; exampleOrTest != examplesOrTests.begin().operator->() + examplesOrTests.size(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == EOT &&
                exampleOrTest->dscTarget->getSourceTarget().name.contains(finalEndString))
            {
                return GetEnds(std::move(endsWith), examplesOrTests, exampleOrTest);
            }
        }
    }
    return GetEnds(std::move(endsWith), examplesOrTests, nullptr);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm> auto BoostCppTarget::get()
{
    return Get<EOT, iteratorTargetType, bsm>(examplesOrTests, nullptr);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
auto BoostCppTarget::getEndsWith(const char *endsWith = "")
{
    return GetEnds<EOT, iteratorTargetType, bsm>(endsWith, examplesOrTests, nullptr);
}
#endif // BOOSTCPPTARGET_HPP
