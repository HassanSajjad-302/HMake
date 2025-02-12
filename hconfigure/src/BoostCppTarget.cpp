
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
    const string buildCacheFilesDirPath = configuration->name + slashc + name;

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
            auto boostExampleOrTest = static_cast<BoostExampleOrTestType>(targetConfigCache[i].GetUint());
            const string unitTestName =
                mainTarget.getPrebuiltBasicTarget().name + slashc +
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
                CSourceTarget &cTarget = configuration->getCppObjectNoName(explicitBuild, "", unitTestName)
                                             .privateDeps(&mainTarget.getSourceTarget());
                auto &cppTarget = static_cast<CppSourceTarget &>(cTarget);
                examplesOrTests.emplace_back(BoostTestTargetType{.cppTarget = &cppTarget}, boostExampleOrTest);

                if (testTarget)
                {
                    testTarget->realBTargets[0].addDependency(cTarget);
                }
            }
            else
            {
                DSC<CppSourceTarget> &uintTest =
                    configuration->getCppExeDSCNoName(explicitBuild, "", unitTestName).privateLibraries(&mainTarget);
                examplesOrTests.emplace_back(BoostTestTargetType{.dscTarget = &uintTest}, boostExampleOrTest);
                if (isExample)
                {
                    if (examplesTarget)
                    {
                        examplesTarget->realBTargets[0].addDependency(uintTest.getLinkOrArchiveTarget());
                    }
                }
                else
                {

                    if (testTarget)
                    {
                        testTarget->realBTargets[0].addDependency(uintTest.getLinkOrArchiveTarget());
                    }
                }
            }
        }
    }
}

BoostCppTarget::~BoostCppTarget()
{
    configCache[targetCacheIndex].CopyFrom(buildOrConfigCacheCopy, ralloc);
}
