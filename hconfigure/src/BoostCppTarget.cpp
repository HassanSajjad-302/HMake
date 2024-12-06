
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
                pstring buildCacheFilesDirPath = configureNode->filePath + slashc +
                                                 mainTarget.getPrebuiltBasicTarget().name + slashc + "Tests" + slashc;

                const bool uintTestExplicitBuild = configuration->evaluate(TestsExplicit::YES);
                pstring uintTestName =
                    pstring(targetConfigCache[i + 1].GetString(), targetConfigCache[i + 1].GetStringLength());
                DSC<CppSourceTarget> &uintTest =
                    configuration->getCppExeDSC(uintTestExplicitBuild, buildCacheFilesDirPath, uintTestName)
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

void BoostCppTarget::addTestDirectory(const pstring &dir)
{
    // TODO
    // Check that configuration has Tests defined. For now assuming it has. Also, these tests are not run.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(BuildTests::YES))
        {
            for (const auto &k : directory_iterator(path(srcNode->filePath + dir)))
            {
                if (k.path().extension() == ".cpp")
                {
                    // not adding configuration name because configuration add its own name
                    const pstring name = getLastNameAfterSlash(mainTarget.getPrebuiltBasicTarget().name) + slashc +
                                         "Tests" + slashc + k.path().stem().string();

                    buildOrConfigCacheCopy.PushBack(static_cast<uint8_t>(BoostExampleOrTestType::RUNTEST), cacheAlloc);
                    buildOrConfigCacheCopy.PushBack(
                        PValue(kStringType).SetString(name.c_str(), name.size(), cacheAlloc), cacheAlloc);

                    const Node *node = Node::getNodeFromNormalizedPath(k.path(), true);
                    pstring buildCacheFilesDirPath = configureNode->filePath + slashc +
                                                     mainTarget.getPrebuiltBasicTarget().name + slashc + "Tests" +
                                                     slashc;
                    configuration
                        ->getCppExeDSC(configuration->evaluate(TestsExplicit::YES), buildCacheFilesDirPath, name)
                        .privateLibraries(&mainTarget)
                        .getSourceTarget()
                        .moduleFiles(node->filePath);
                }
            }
        }
    }
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