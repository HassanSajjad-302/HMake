
#ifdef USE_HEADER_UNITS
import "BoostCppTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "DSC.hpp";
import "LOAT.hpp";
#else
#include "BoostCppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "LOAT.hpp"
#include <utility>
#endif

using std::filesystem::directory_iterator;

static DSC<CppSourceTarget> &getMainTarget(const string &name, Configuration *configuration, const bool headerOnly)
{
    const string buildCacheFilesDirPath = configuration->name + slashc + name;

    DSC<CppSourceTarget> *t = nullptr;
    if (headerOnly)
    {
        t = &configuration->getCppStaticDSC(false, buildCacheFilesDirPath, name);
    }
    else
    {
        t = &configuration->getCppTargetDSC(false, buildCacheFilesDirPath, name);
    }
    t->getSourceTarget().publicHUDirs(string("boost") + slashc + name);
    if (name != "core" && name != "mpl" && name != "detail")
    {
        t->getSourceTarget().headerUnits(string("boost") + slashc + name + ".hpp");
    }
    return *t;
}

BoostCppTarget::BoostCppTarget(const string &name, Configuration *configuration_, const bool headerOnly,
                               const bool createTestsTarget, const bool createExamplesTarget)
    : TargetCache(configuration_->name + "Boost_" + name), configuration(configuration_),
      mainTarget(getMainTarget(name, configuration_, headerOnly))
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        Value &targetConfigCache = getConfigCache();
        if (createTestsTarget)
        {
            string testsLocation = configuration->name + slashc + name + slashc + "Tests";
            testTarget = &targets<BTarget>.emplace_back(std::move(testsLocation), true, false, true, false, true);
        }
        if (createExamplesTarget)
        {
            string examplesLocation = configuration->name + slashc + name + slashc + "Examples";
            testTarget = &targets<BTarget>.emplace_back(std::move(examplesLocation), true, false, true, false, true);
        }
        if (targetConfigCache.Size() < 2)
        {
            return;
        }
        for (uint64_t i = 1; i < targetConfigCache.Size(); i += 2)
        {
            auto boostExampleOrTest = static_cast<BoostExampleOrTestType>(targetConfigCache[i].GetUint());
            const string unitTestName =
                mainTarget.getPLOAT().name + slashc +
                string(targetConfigCache[i + 1].GetString(), targetConfigCache[i + 1].GetStringLength());
            bool explicitBuild = false;
            bool isExample = false;
            if (boostExampleOrTest == BoostExampleOrTestType::EXAMPLE ||
                boostExampleOrTest == BoostExampleOrTestType::RUN_EXAMPLE)
            {
                explicitBuild = configuration->evaluate(ExamplesExplicit::YES);
                isExample = true;
            }
            else
            {
                explicitBuild = configuration->evaluate(TestsExplicit::YES);
            }

            bool isCompile = false;
            if (boostExampleOrTest == BoostExampleOrTestType::COMPILE_TEST ||
                boostExampleOrTest == BoostExampleOrTestType::COMPILE_FAIL_TEST)
            {
                isCompile = true;
            }
            else
            {
                isCompile = false;
            }

            if (isCompile)
            {
                CppSourceTarget &cppTarget = configuration->getCppObjectNoName(explicitBuild, "", unitTestName)
                                                 .privateDeps(&mainTarget.getSourceTarget());
                examplesOrTests.emplace_back(BoostTestTargetType{.cppTarget = &cppTarget}, boostExampleOrTest);

                if (testTarget)
                {
                    testTarget->addDependency<0>(cppTarget);
                }
            }
            else
            {
                DSC<CppSourceTarget> &uintTest =
                    configuration->getCppExeDSCNoName(explicitBuild, "", unitTestName).privateDeps(mainTarget);
                examplesOrTests.emplace_back(BoostTestTargetType{.dscTarget = &uintTest}, boostExampleOrTest);
                if (isExample)
                {
                    if (examplesTarget)
                    {
                        examplesTarget->addDependency<0>(uintTest.getLOAT());
                    }
                }
                else
                {
                    if (testTarget)
                    {
                        testTarget->addDependency<0>(uintTest.getLOAT());
                    }
                }
            }
        }
    }
}

BoostCppTarget &BoostCppTarget::assignPrivateTestDeps()
{
    for (auto &[testTarget, targetType] : examplesOrTests)
    {
        bool isCompile = false;
        if (targetType == BoostExampleOrTestType::COMPILE_TEST ||
            targetType == BoostExampleOrTestType::COMPILE_FAIL_TEST)
        {
            isCompile = true;
        }
        else
        {
            isCompile = false;
        }

        if (isCompile)
        {
            for (CppSourceTarget *dep : cppTestDepsPrivate)
            {
                testTarget.cppTarget->privateDeps(dep);
            }
        }
        else
        {
            for (DSC<CppSourceTarget> *dep : dscTestDepsPrivate)
            {
                testTarget.dscTarget->privateDeps(*dep);
            }
        }
    }
    return *this;
}

void BoostCppTarget::copyConfigCache()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        configCache[targetCacheIndex].CopyFrom(buildOrConfigCacheCopy, ralloc);
    }
}