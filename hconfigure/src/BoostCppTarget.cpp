
#include "BoostCppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "LOAT.hpp"
#include <utility>

using std::filesystem::directory_iterator, std::filesystem::remove;

void removeTroublingHu(const string_view *headerUnitsJsonDirs, uint64_t headerUnitsJsonDirsSize,
                       const string_view *headerUnitsJsonEntry, uint64_t headerUnitsJsonEntrySize)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        string str = "header-units.json";
        string boostDir = srcNode->filePath + slashc + string("boost") + slashc;
        for (uint64_t i = 0; i < headerUnitsJsonDirsSize; ++i)
        {
            path p{boostDir + string(headerUnitsJsonDirs[i]) + slashc + str};
            if (exists(p))
            {
                remove(p);
            }
        }

        for (uint64_t i = 0; i < headerUnitsJsonEntrySize; i += 2)
        {
            /*Document d;
            string fileName = boostDir + string(headerUnitsJsonEntry[i]) + slashc + str;
            auto a = readValueFromFile(fileName, d);
            Value &m = d.FindMember("BuildAsHeaderUnits")->value.GetArray();
            uint64_t index = UINT64_MAX;
            for (uint64_t j = 0; j < m.Size(); ++j)
            {
                if (compareStringsFromEnd(vtosv(m[j]), headerUnitsJsonEntry[i + 1]))
                {
                    m.Erase(&m[j]);
                    prettyWriteValueToFile(fileName, d);
                }
            }*/
        }
    }
}

static DSC<CppSourceTarget> &getMainTarget(const string &name, Configuration *configuration, const bool headerOnly,
                                           const bool hasBigHeader)
{
    if (name == "boost/core/noncopyable.hpp")
    {
        bool breakpoint = true;
    }
    const string buildCacheFilesDirPath = configuration->name + slashc + name;

    DSC<CppSourceTarget> *t = nullptr;
    if (headerOnly)
    {
        t = &configuration->getCppObjectDSC(false, buildCacheFilesDirPath, name);
    }
    else
    {
        t = &configuration->getCppTargetDSC(false, buildCacheFilesDirPath, name);
        t->getSourceTarget().moduleDirs("libs" + name + "src");
    }

    CppSourceTarget &cpp = t->getSourceTarget();
    const string s = "boost/" + name;
    if (name == "hash2" || name == "container_hash" || name == "describe" || name == "mp11" || name == "io" ||
        name == "system")
    {
        cpp.publicHUDirsRE(s, s + '/', ".*hpp");
        cpp.publicHUDirsRE(s, s + '/', ".*h");
        if (hasBigHeader)
        {
            cpp.publicHeaderUnits(s + ".hpp", s + ".hpp");
        }
    }
    else
    {
        cpp.publicIncDirsRE(s, s + '/', ".*hpp");
        cpp.publicIncDirsRE(s, s + '/', ".*h");
        if (hasBigHeader)
        {
            cpp.publicHeaderFiles(s + ".hpp", s + ".hpp");
        }
    }
    return *t;
}

BoostCppTarget::BoostCppTarget(const string &name, Configuration *configuration_, const bool headerOnly,
                               const bool hasBigHeader, const bool createTestsTarget, const bool createExamplesTarget)
    : TargetCache(*new string(configuration_->name + "Boost_" + name)), configuration(configuration_),
      mainTarget(getMainTarget(name, configuration_, headerOnly, hasBigHeader))
{
    // Reads config-cache at build-time and
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (createTestsTarget)
        {
            string testsLocation = configuration->name + slashc + name + slashc + "Tests";
            testTarget = &targets<BTarget>.emplace_back(std::move(testsLocation), true, false, true, false);
        }
        if (createExamplesTarget)
        {
            string examplesLocation = configuration->name + slashc + name + slashc + "Examples";
            testTarget = &targets<BTarget>.emplace_back(std::move(examplesLocation), true, false, true, false);
        }

        string_view configCache = fileTargetCaches[cacheIndex].configCache;
        uint32_t bytesRead = 0;
        uint32_t count = readUint32(configCache.data(), bytesRead);
        for (uint64_t i = 0; i < count; ++i)
        {
            auto boostExampleOrTest = static_cast<BoostExampleOrTestType>(readBool(configCache.data(), bytesRead));
            string unitTestName = removeDashCppFromName(mainTarget.getSourceTarget().name) + slashc;
            unitTestName += readStringView(configCache.data(), bytesRead);
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
                    testTarget->addDepNow<0>(cppTarget);
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
                        examplesTarget->addDepNow<0>(uintTest.getLOAT());
                    }
                }
                else
                {
                    if (testTarget)
                    {
                        testTarget->addDepNow<0>(uintTest.getLOAT());
                    }
                }
            }
        }
    }
    else
    {
        string str = srcNode->filePath + "/libs/" + name + "/test";
        for (const auto &p : std::filesystem::recursive_directory_iterator(str))
        {
            if (p.path().extension() == ".ipp" || p.path().extension() == ".hpp")
            {
                Node *node = Node::getNodeFromNonNormalizedString(p.path().generic_string(), true);
                testReqHeaderFiles.emplace(string(node->getFileName()), node);
            }
        }
        writeUint32(configBuffer, 0);
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
        const auto ptr = reinterpret_cast<const char *>(&testsOrExamplesCount);
        configBuffer.insert(configBuffer.begin(), ptr, ptr + sizeof(testsOrExamplesCount));
        fileTargetCaches[cacheIndex].configCache = string_view(configBuffer.data(), configBuffer.size());
    }
}