
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
    pstring buildCacheFilesDirPath = configureNode->filePath + slashc + configuration->name + slashc + name;

    if (headerOnly)
    {
        return configuration->getCppObjectDSC(false, buildCacheFilesDirPath, name);
    }
    return configuration->getCppTargetDSC(false, buildCacheFilesDirPath, name, configuration->targetType);
}

BoostCppTarget::BoostCppTarget(const pstring &name, Configuration *configuration_, const bool headerOnly)
    : TargetCache("Boost_" + name), configuration(configuration_),
      mainTarget(getMainTarget(name, configuration_, headerOnly))
{
    if (bsMode == BSMode::BUILD)
    {
        PValue &targetConfigCache = getConfigCache();
        if (targetConfigCache.Size() < 2)
        {
            return;
        }
        for (uint64_t i = 1; i < targetConfigCache.Size(); i += 2)
        {
            PValue &testName = targetConfigCache[i];
            auto boostExampleOrTest = static_cast<BoostExampleOrTestType>(targetConfigCache[i + 1].GetUint());

            if (boostExampleOrTest != BoostExampleOrTestType::EXAMPLE)
            {
                pstring buildCacheFilesDirPath = configureNode->filePath + slashc +
                                                 mainTarget.getPrebuiltBasicTarget().name + slashc + "Tests" + slashc;
                examplesOrTests.emplace_back(
                    &configuration
                         ->getCppExeDSC(configuration->evaluate(TestsExplicit::YES), buildCacheFilesDirPath,
                                        pstring(testName.GetString(), testName.GetStringLength()))
                         .privateLibraries(&mainTarget),
                    boostExampleOrTest);
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
    if (bsMode == BSMode::CONFIGURE && configuration->evaluate(BuildTests::YES))
    {
        for (const auto &k : directory_iterator(path(srcNode->filePath + dir)))
        {
            if (k.path().extension() == ".cpp")
            {
                const pstring name = k.path().stem().string();

                buildOrConfigCacheCopy.PushBack(PValue(kStringType).SetString(name.c_str(), name.size(), cacheAlloc),
                                                cacheAlloc);
                buildOrConfigCacheCopy.PushBack(static_cast<uint8_t>(BoostExampleOrTestType::RUNTEST), cacheAlloc);

                const Node *node = Node::getNodeFromNormalizedPath(k.path(), true);
                pstring buildCacheFilesDirPath = configureNode->filePath + slashc +
                                                 mainTarget.getPrebuiltBasicTarget().name + slashc + "Tests" + slashc;
                configuration->getCppExeDSC(configuration->evaluate(TestsExplicit::YES), buildCacheFilesDirPath, name)
                    .privateLibraries(&mainTarget)
                    .getSourceTarget()
                    .moduleFiles(node->filePath);
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