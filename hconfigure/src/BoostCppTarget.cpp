
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

static DSC<CppSourceTarget> &getMainTarget(const string &name, Configuration *configuration, const bool headerOnly)
{
    const string buildCacheFilesDirPath = configureNode->filePath + slashc + configuration->name + slashc + name;

    if (headerOnly)
    {
        return configuration->getCppObjectDSC(false, buildCacheFilesDirPath, name);
    }
    return configuration->getCppTargetDSC(false, buildCacheFilesDirPath, name, configuration->targetType);
}

BoostCppTarget::BoostCppTarget(const string &name, Configuration *configuration_, const bool headerOnly,
                               const bool createTestsTarget, const bool createExamplesTarget)
    : TargetCache("Boost_" + name), configuration(configuration_),
      mainTarget(getMainTarget(name, configuration_, headerOnly))
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        Value &targetConfigCache = getConfigCache();
        if (createTestsTarget)
        {
            string testsLocation = configuration->name + slashc + name + slashc + "Tests";
            testTarget = &targets2<BTarget>.emplace_back(std::move(testsLocation), true, false, true, false, true);
        }
        if (createExamplesTarget)
        {
            string examplesLocation = configuration->name + slashc + name + slashc + "Examples";
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
                const string unitTestName =
                    mainTarget.getPrebuiltBasicTarget().name + slashc +
                    string(targetConfigCache[i + 1].GetString(), targetConfigCache[i + 1].GetStringLength());

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

// TODO
// Adds every test as run-tests instead of compile-tests, run-fail tests or compile-fail tests.
void BoostCppTarget::addExampleOrTestGeneric(const string_view sourceDir, const string_view fileName,
                                             const string_view innerBuildDirName,
                                             BoostExampleOrTestType exampleOrTestType, const bool buildExplicit)
{
    const string configurationNamePlusTargetName = mainTarget.getPrebuiltBasicTarget().name + slashc;
    const string fileStemName = path(fileName).stem().string();

    const string pushName = string(innerBuildDirName) + fileStemName;
    const string buildCacheFilesDirPath =
        configureNode->filePath + slashc + configurationNamePlusTargetName + string(innerBuildDirName);
    const string name = configurationNamePlusTargetName + string(innerBuildDirName) + fileStemName;

    buildOrConfigCacheCopy.PushBack(static_cast<uint8_t>(exampleOrTestType), cacheAlloc);
    buildOrConfigCacheCopy.PushBack(Value(kStringType).SetString(pushName.c_str(), pushName.size(), cacheAlloc),
                                    cacheAlloc);

    const Node *node = Node::getNodeFromNonNormalizedString(string(sourceDir) + slashc + string(fileName), true);
    configuration->getCppExeDSCNoName(buildExplicit, buildCacheFilesDirPath, name)
        .privateLibraries(&mainTarget)
        .getSourceTarget()
        .moduleFiles(node->filePath);
}

void BoostCppTarget::addDirectoryGeneric(string_view dir, string_view innerBuildDirName,
                                         BoostExampleOrTestType exampleOrTestType, bool buildExplicit)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(BuildTests::YES))
        {
            for (const auto &k : directory_iterator(path(srcNode->filePath + string(dir))))
            {
                if (k.path().extension() == ".cpp")
                {
                    addExampleOrTestGeneric(k.path().parent_path().string(), k.path().filename().string(),
                                            innerBuildDirName, exampleOrTestType, buildExplicit);
                }
            }
        }
    }
}

// TODO
// Maybe, provide different definitions of these functions in build-mode and configure-mode, so that in build-mode these
// functions are inline and because of being empty, the call to these functions is completely eliminated.
void BoostCppTarget::addRunTestsDirectory(const string &sourceDir)
{
    addDirectoryGeneric(sourceDir, string("Tests") + slashc, BoostExampleOrTestType::RUN_TEST,
                        configuration->evaluate(TestsExplicit::YES));
}

void BoostCppTarget::addRunTestsDirectoryEndsWith(const string &sourceDir, const string &innerBuildDirEndsWith)
{
    addDirectoryGeneric(sourceDir, string("Tests") + slashc + innerBuildDirEndsWith + slashc,
                        BoostExampleOrTestType::RUN_TEST, configuration->evaluate(TestsExplicit::YES));
}

void BoostCppTarget::addCompileTestsDirectory(const string &sourceDir)
{
    addDirectoryGeneric(sourceDir, string("Tests") + slashc, BoostExampleOrTestType::COMPILE_TEST,
                        configuration->evaluate(TestsExplicit::YES));
}

void BoostCppTarget::addRunExamplesDirectory(const string &sourceDir)
{
    addDirectoryGeneric(sourceDir, string("Examples") + slashc, BoostExampleOrTestType::RUN_EXAMPLE,
                        configuration->evaluate(ExamplesExplicit::YES));
}

/*BoostCppTarget::RunTestsBuildMode BoostCppTarget::getRunTestsBuildMode()
{
    return RunTestsBuildMode(examplesOrTests, nullptr);
}

BoostCppTarget::RunTestsBuildModeEndsWith BoostCppTarget::getRunTestsBuildModeEndsWith(string endsWith)
{
    return RunTestsBuildModeEndsWith(std::move(endsWith), examplesOrTests, nullptr);
}*/

void BoostCppTarget::addRunTests(const string_view sourceDir, const string_view *fileName, const uint64_t arraySize)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string innerBuildDirName = string("Tests") + slashc;
        for (uint64_t i = 0; i < arraySize; ++i)
        {
            addExampleOrTestGeneric(srcNode->filePath + slashc + string(sourceDir), fileName[i], innerBuildDirName,
                                    BoostExampleOrTestType::RUN_TEST, configuration->evaluate(TestsExplicit::YES));
        }
    }
}

void BoostCppTarget::addCompileTests(string_view sourceDir, const string_view *fileName, uint64_t arraySize)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string innerBuildDirName = string("Tests") + slashc;
        for (uint64_t i = 0; i < arraySize; ++i)
        {
            addExampleOrTestGeneric(srcNode->filePath + slashc + string(sourceDir), fileName[i], innerBuildDirName,
                                    BoostExampleOrTestType::COMPILE_TEST, configuration->evaluate(TestsExplicit::YES));
        }
    }
}

void BoostCppTarget::addCompileFailTests(string_view sourceDir, const string_view *fileName, uint64_t arraySize)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string innerBuildDirName = string("Tests") + slashc;
        for (uint64_t i = 0; i < arraySize; ++i)
        {
            addExampleOrTestGeneric(srcNode->filePath + slashc + string(sourceDir), fileName[i], innerBuildDirName,
                                    BoostExampleOrTestType::COMPILE_FAIL_TEST,
                                    configuration->evaluate(TestsExplicit::YES));
        }
    }
}

void BoostCppTarget::addExamples(string_view sourceDir, const string_view *fileName, uint64_t arraySize)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string innerBuildDirName = string("Examples") + slashc;
        for (uint64_t i = 0; i < arraySize; ++i)
        {
            addExampleOrTestGeneric(srcNode->filePath + slashc + string(sourceDir), fileName[i], innerBuildDirName,
                                    BoostExampleOrTestType::EXAMPLE, configuration->evaluate(ExamplesExplicit::YES));
        }
    }
}
