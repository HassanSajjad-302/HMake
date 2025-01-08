
#ifdef USE_HEADER_UNITS
import "BoostCppTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "DSC.hpp";
import "LinkOrArchiveTarget.hpp";
import <GetTarget.hpp>;
#else
#include "BoostCppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "LinkOrArchiveTarget.hpp"
#include <GetTarget.hpp>
#include <utility>
#endif

using std::filesystem::directory_iterator;

static DSC<CppSourceTarget> &getMainTarget(const pstring &name, Configuration *configuration, const bool headerOnly)
{
    const pstring buildCacheFilesDirPath = configureNode->filePath + slashc + configuration->name + slashc + name;

    if (headerOnly)
    {
        return configuration->getCppObjectDSC(false, buildCacheFilesDirPath, name);
    }
    return configuration->getCppTargetDSC(false, buildCacheFilesDirPath, name, configuration->targetType);
}

BoostCppTarget::RunTests::RunTests(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_)
    : examplesOrTests(examplesOrTests_), exampleOrTest(exampleOrTest_)
{
}

ExampleOrTest &BoostCppTarget::RunTests::operator*() const
{
    return const_cast<ExampleOrTest &>(*exampleOrTest);
}

bool BoostCppTarget::RunTests::operator!=(const RunTests &other) const
{
    return exampleOrTest != other.exampleOrTest;
}

BoostCppTarget::RunTests BoostCppTarget::RunTests::end() const
{
    if (!exampleOrTest)
    {
        return RunTests(examplesOrTests, nullptr);
    }
    return RunTests(examplesOrTests, examplesOrTests.begin().operator->() + examplesOrTests.size());
}

BoostCppTarget::RunTests BoostCppTarget::RunTests::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::RUN_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return RunTests(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::RunTests BoostCppTarget::RunTests::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::RUN_TEST)
        {
            return RunTests(examplesOrTests, exampleOrTest);
        }
    }
    return RunTests(examplesOrTests, nullptr);
}

BoostCppTarget::RunTestsBuildMode::RunTestsBuildMode(const vector<ExampleOrTest> &examplesOrTests_,
                                                     const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::RunTestsBuildMode BoostCppTarget::RunTestsBuildMode::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == BSMode::BUILD)
    {
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if (exampleOrTest_.targetType == BoostExampleOrTestType::RUN_TEST)
            {
                exampleOrTestLocal = &exampleOrTest_;
                break;
            }
        }
        // TODO
        // Find out why is this assigned. And write the comment here.
        exampleOrTest = exampleOrTestLocal;
    }
    return RunTestsBuildMode(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::RunTestsBuildMode BoostCppTarget::RunTestsBuildMode::operator++()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        ++exampleOrTest;
        for (; exampleOrTest != examplesOrTests.begin().operator->() + examplesOrTests.size(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == BoostExampleOrTestType::RUN_TEST)
            {
                return RunTestsBuildMode(examplesOrTests, exampleOrTest);
            }
        }
    }
    return RunTestsBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::RunTestsBuildModeEndsWith::RunTestsBuildModeEndsWith(pstring endsWith_,
                                                                     const vector<ExampleOrTest> &examplesOrTests_,
                                                                     const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_), endsWith(std::move(endsWith_))
{
}

BoostCppTarget::RunTestsBuildModeEndsWith BoostCppTarget::RunTestsBuildModeEndsWith::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == BSMode::BUILD)
    {
        pstring finalEndString = "Tests" + endsWith;
        lowerCasePStringOnWindows(finalEndString.data(), finalEndString.size());
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if (exampleOrTest_.targetType == BoostExampleOrTestType::RUN_TEST &&
                exampleOrTest_.dscTarget->getSourceTarget().name.contains(finalEndString))
            {
                exampleOrTestLocal = &exampleOrTest_;
                break;
            }
        }
        exampleOrTest = exampleOrTestLocal;
    }
    return RunTestsBuildModeEndsWith(std::move(endsWith), examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::RunTestsBuildModeEndsWith BoostCppTarget::RunTestsBuildModeEndsWith::operator++()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        ++exampleOrTest;
        pstring finalEndString = "Tests" + endsWith;
        lowerCasePStringOnWindows(finalEndString.data(), finalEndString.size());
        for (; exampleOrTest != examplesOrTests.begin().operator->() + examplesOrTests.size(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == BoostExampleOrTestType::RUN_TEST &&
                exampleOrTest->dscTarget->getSourceTarget().name.contains(finalEndString))
            {
                return RunTestsBuildModeEndsWith(std::move(endsWith), examplesOrTests, exampleOrTest);
            }
        }
    }
    return RunTestsBuildModeEndsWith(std::move(endsWith), examplesOrTests, nullptr);
}

BoostCppTarget::CompileTests::CompileTests(const vector<ExampleOrTest> &examplesOrTests_,
                                           const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::CompileTests BoostCppTarget::CompileTests::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::COMPILE_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return CompileTests(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::CompileTests BoostCppTarget::CompileTests::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::COMPILE_TEST)
        {
            return CompileTests(examplesOrTests, exampleOrTest);
        }
    }
    return CompileTests(examplesOrTests, nullptr);
}

BoostCppTarget::CompileTestsBuildMode::CompileTestsBuildMode(const vector<ExampleOrTest> &examplesOrTests_,
                                                             const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::CompileTestsBuildMode BoostCppTarget::CompileTestsBuildMode::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::COMPILE_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return CompileTestsBuildMode(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::CompileTestsBuildMode BoostCppTarget::CompileTestsBuildMode::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::COMPILE_TEST)
        {
            return CompileTestsBuildMode(examplesOrTests, exampleOrTest);
        }
    }
    return CompileTestsBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::CompileFailTests::CompileFailTests(const vector<ExampleOrTest> &examplesOrTests_,
                                                   const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::CompileFailTests BoostCppTarget::CompileFailTests::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::COMPILE_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return CompileFailTests(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::CompileFailTests BoostCppTarget::CompileFailTests::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::COMPILE_TEST)
        {
            return CompileFailTests(examplesOrTests, exampleOrTest);
        }
    }
    return CompileFailTests(examplesOrTests, nullptr);
}

BoostCppTarget::CompileFailTestsBuildMode::CompileFailTestsBuildMode(const vector<ExampleOrTest> &examplesOrTests_,
                                                                     const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::CompileFailTestsBuildMode BoostCppTarget::CompileFailTestsBuildMode::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::COMPILE_FAIL_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return CompileFailTestsBuildMode(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::CompileFailTestsBuildMode BoostCppTarget::CompileFailTestsBuildMode::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::COMPILE_FAIL_TEST)
        {
            return CompileFailTestsBuildMode(examplesOrTests, exampleOrTest);
        }
    }
    return CompileFailTestsBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::LinkTests::LinkTests(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::LinkTests BoostCppTarget::LinkTests::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::LINK_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return LinkTests(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::LinkTests BoostCppTarget::LinkTests::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::LINK_TEST)
        {
            return LinkTests(examplesOrTests, exampleOrTest);
        }
    }
    return LinkTests(examplesOrTests, nullptr);
}

BoostCppTarget::LinkTestsBuildMode::LinkTestsBuildMode(const vector<ExampleOrTest> &examplesOrTests_,
                                                       const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::LinkTestsBuildMode BoostCppTarget::LinkTestsBuildMode::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::LINK_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return LinkTestsBuildMode(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::LinkTestsBuildMode BoostCppTarget::LinkTestsBuildMode::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::LINK_TEST)
        {
            return LinkTestsBuildMode(examplesOrTests, exampleOrTest);
        }
    }
    return LinkTestsBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::LinkFailTestsBuildMode::LinkFailTestsBuildMode(const vector<ExampleOrTest> &examplesOrTests_,
                                                               const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::LinkFailTestsBuildMode BoostCppTarget::LinkFailTestsBuildMode::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::LINK_FAIL_TEST)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return LinkFailTestsBuildMode(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::LinkFailTestsBuildMode BoostCppTarget::LinkFailTestsBuildMode::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::LINK_FAIL_TEST)
        {
            return LinkFailTestsBuildMode(examplesOrTests, exampleOrTest);
        }
    }
    return LinkFailTestsBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::Examples::Examples(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::Examples BoostCppTarget::Examples::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::EXAMPLE)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return Examples(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::Examples BoostCppTarget::Examples::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::EXAMPLE)
        {
            return Examples(examplesOrTests, exampleOrTest);
        }
    }
    return Examples(examplesOrTests, nullptr);
}

BoostCppTarget::ExamplesBuildMode::ExamplesBuildMode(const vector<ExampleOrTest> &examplesOrTests_,
                                                     const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::ExamplesBuildMode BoostCppTarget::ExamplesBuildMode::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == BSMode::BUILD)
    {
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if (exampleOrTest_.targetType == BoostExampleOrTestType::EXAMPLE)
            {
                exampleOrTestLocal = &exampleOrTest_;
                break;
            }
        }
        exampleOrTest = exampleOrTestLocal;
    }
    return ExamplesBuildMode(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::ExamplesBuildMode BoostCppTarget::ExamplesBuildMode::operator++()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        ++exampleOrTest;
        for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == BoostExampleOrTestType::EXAMPLE)
            {
                return ExamplesBuildMode(examplesOrTests, exampleOrTest);
            }
        }
    }
    return ExamplesBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::RunExamples::RunExamples(const vector<ExampleOrTest> &examplesOrTests_,
                                         const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::RunExamples BoostCppTarget::RunExamples::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
    {
        if (exampleOrTest_.targetType == BoostExampleOrTestType::RUN_EXAMPLE)
        {
            exampleOrTestLocal = &exampleOrTest_;
            break;
        }
    }
    exampleOrTest = exampleOrTestLocal;
    return RunExamples(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::RunExamples BoostCppTarget::RunExamples::operator++()
{
    ++exampleOrTest;
    for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
    {
        if (exampleOrTest->targetType == BoostExampleOrTestType::RUN_EXAMPLE)
        {
            return RunExamples(examplesOrTests, exampleOrTest);
        }
    }
    return RunExamples(examplesOrTests, nullptr);
}

BoostCppTarget::RunExamplesBuildMode::RunExamplesBuildMode(const vector<ExampleOrTest> &examplesOrTests_,
                                                           const ExampleOrTest *exampleOrTest_)
    : RunTests(examplesOrTests_, exampleOrTest_)
{
}

BoostCppTarget::RunExamplesBuildMode BoostCppTarget::RunExamplesBuildMode::begin()
{
    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == BSMode::BUILD)
    {
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if (exampleOrTest_.targetType == BoostExampleOrTestType::RUN_EXAMPLE)
            {
                exampleOrTestLocal = &exampleOrTest_;
                break;
            }
        }
        exampleOrTest = exampleOrTestLocal;
    }
    return RunExamplesBuildMode(examplesOrTests, exampleOrTestLocal);
}

BoostCppTarget::RunExamplesBuildMode BoostCppTarget::RunExamplesBuildMode::operator++()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        ++exampleOrTest;
        for (; exampleOrTest != examplesOrTests.end().operator->(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == BoostExampleOrTestType::RUN_EXAMPLE)
            {
                return RunExamplesBuildMode(examplesOrTests, exampleOrTest);
            }
        }
    }
    return RunExamplesBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::BoostCppTarget(const pstring &name, Configuration *configuration_, const bool headerOnly,
                               const bool createTestsTarget, const bool createExamplesTarget)
    : TargetCache("Boost_" + name), configuration(configuration_),
      mainTarget(getMainTarget(name, configuration_, headerOnly))
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        PValue &targetConfigCache = getConfigCache();
        if (createTestsTarget)
        {
            pstring testsLocation = configuration->name + slashc + name + slashc + "Tests";
            testTarget = &targets2<BTarget>.emplace_back(std::move(testsLocation), true, false, true, false, true);
        }
        if (createExamplesTarget)
        {
            pstring examplesLocation = configuration->name + slashc + name + slashc + "Examples";
            testTarget = &targets2<BTarget>.emplace_back(std::move(examplesLocation), true, false, true, false, true);
        }
        if (targetConfigCache.Size() < 2)
        {
            return;
        }
        for (uint64_t i = 1; i < targetConfigCache.Size(); i += 2)
        {
            if (auto boostExampleOrTest = static_cast<BoostExampleOrTestType>(targetConfigCache[i].GetUint());
                boostExampleOrTest != BoostExampleOrTestType::EXAMPLE)
            {
                const pstring unitTestName =
                    mainTarget.getPrebuiltBasicTarget().name + slashc +
                    pstring(targetConfigCache[i + 1].GetString(), targetConfigCache[i + 1].GetStringLength());

                const bool uintTestExplicitBuild = configuration->evaluate(TestsExplicit::YES);
                DSC<CppSourceTarget> &uintTest =
                    configuration->getCppExeDSCNoName(uintTestExplicitBuild, "", unitTestName)
                        .privateLibraries(&mainTarget);
                examplesOrTests.emplace_back(&uintTest, boostExampleOrTest);
                if (testTarget)
                {
                    testTarget->realBTargets[0].addDependency(uintTest.getLinkOrArchiveTarget());
                }
            }
        }
    }
}

BoostCppTarget::~BoostCppTarget()
{
    configCache[targetCacheIndex].CopyFrom(buildOrConfigCacheCopy, ralloc);
}

void BoostCppTarget::addDirectoryGeneric(const pstring &dir, const pstring &nameThirdPart,
                                         BoostExampleOrTestType exampleOrTestType, bool buildExplicit)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(BuildTests::YES))
        {
            for (const auto &k : directory_iterator(path(srcNode->filePath + dir)))
            {
                if (k.path().extension() == ".cpp")
                {
                    const pstring nameFirstPart = configureNode->filePath + slashc;
                    const pstring nameSecondPart = mainTarget.getPrebuiltBasicTarget().name + slashc;
                    const pstring nameFourthPart = k.path().stem().string();

                    const pstring pushName = nameThirdPart + nameFourthPart;
                    const pstring buildCacheFilesDirPath = nameFirstPart + nameSecondPart + nameThirdPart;
                    const pstring name = nameSecondPart + nameThirdPart + nameFourthPart;

                    buildOrConfigCacheCopy.PushBack(static_cast<uint8_t>(exampleOrTestType), cacheAlloc);
                    buildOrConfigCacheCopy.PushBack(
                        PValue(kStringType).SetString(pushName.c_str(), pushName.size(), cacheAlloc), cacheAlloc);

                    const Node *node = Node::getNodeFromNormalizedPath(k.path(), true);
                    configuration->getCppExeDSCNoName(buildExplicit, buildCacheFilesDirPath, name)
                        .privateLibraries(&mainTarget)
                        .getSourceTarget()
                        .moduleFiles(node->filePath);
                }
            }
        }
    }
}

void BoostCppTarget::addTestDirectory(const pstring &dir)
{
    addDirectoryGeneric(dir, pstring("Tests") + slashc, BoostExampleOrTestType::RUN_TEST,
                        configuration->evaluate(TestsExplicit::YES));
}

void BoostCppTarget::addTestDirectoryEndsWith(const pstring &dir, const pstring &endsWith)
{
    addDirectoryGeneric(dir, "Tests" + endsWith + slashc, BoostExampleOrTestType::RUN_TEST,
                        configuration->evaluate(TestsExplicit::YES));
}

void BoostCppTarget::addRunExamplesDirectory(const pstring &dir)
{
    addDirectoryGeneric(dir, pstring("Examples") + slashc, BoostExampleOrTestType::RUN_EXAMPLE,
                        configuration->evaluate(ExamplesExplicit::YES));
}

/*void BoostCppTarget::addTestDirectory(const pstring &dir, const pstring &prefix)
{
    // TODO
    // Check that configuration has Tests defined
    namespace BoostCppTarget = Indices::ConfigCache::BoostCppTarget;
    if (bsMode == BSMode::CONFIGURE)
    {
        for (const auto &k : directory_iterator(path(srcNode->filePath + dir)))
        {
            if (k.path().extension() == ".cpp")
            {
                const Node *node = Node::getNodeFromNormalizedPath(k.path(), true);
                buildOrConfigCacheCopy[BoostCppTarget::runTests].PushBack(node->getPValue(), cacheAlloc);
                string str = prefix + path(node->filePath).filename().string();
                buildOrConfigCacheCopy[BoostCppTarget::runTests].PushBack(ptoref(str), cacheAlloc);
            }
        }
    }
}

void BoostCppTarget::addExampleDirectory(const pstring &dir)
{
    // TODO
    // Check that configuration has Examples defined
    namespace BoostCppTarget = Indices::ConfigCache::BoostCppTarget;
    if (bsMode == BSMode::CONFIGURE)
    {
        for (const auto &k : directory_iterator(path(srcNode->filePath + dir)))
        {
            if (k.path().extension() == ".cpp")
            {
                const Node *node = Node::getNodeFromNormalizedPath(k.path(), true);
                buildOrConfigCacheCopy[BoostCppTarget::runTests].PushBack(node->getPValue(), cacheAlloc);
                string str = path(node->filePath).filename().string();
                buildOrConfigCacheCopy[BoostCppTarget::runTests].PushBack(ptoref(str), cacheAlloc);
            }
        }
    }
}*/

BoostCppTarget::RunTestsBuildMode BoostCppTarget::getRunTestsBuildMode()
{
    return RunTestsBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::RunTestsBuildModeEndsWith BoostCppTarget::getRunTestsBuildModeEndsWith(pstring endsWith)
{
    return RunTestsBuildModeEndsWith(std::move(endsWith), examplesOrTests, nullptr);
}
