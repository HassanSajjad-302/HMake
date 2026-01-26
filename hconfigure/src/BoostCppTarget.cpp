
#include "BoostCppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Configuration.hpp"
#include "CppTarget.hpp"
#include "DSC.hpp"
#include "LOAT.hpp"
#include <utility>

using std::filesystem::directory_iterator, std::filesystem::remove;

static DSC<CppTarget> &getMainTarget(const string &name, Configuration *configuration, const bool headerOnly,
                                     const bool hasBigHeader)
{
    if (name == "boost/core/noncopyable.hpp")
    {
        bool breakpoint = true;
    }

    Node *myBuildDir = nullptr;

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string buildCacheFilesDirPath = configureNode->filePath + slashc + configuration->name + slashc + name;
        myBuildDir = Node::getHalfNodeST(buildCacheFilesDirPath);
        create_directories(myBuildDir->filePath);
    }

    DSC<CppTarget> *t = nullptr;
    if (headerOnly)
    {
        t = &configuration->getCppObjectDSC(false, myBuildDir, name);
    }
    else
    {
        t = &configuration->getCppTargetDSC(false, myBuildDir, name);
        t->getSourceTarget().moduleDirs("libs" + name + "src");
    }

    CppTarget &cpp = t->getSourceTarget();
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
                CppTarget &cppTarget = configuration->getCppObjectNoName(explicitBuild, nullptr, unitTestName)
                                           .privateDeps(mainTarget.getSourceTarget());
                examplesOrTests.emplace_back(BoostTestTargetType{.cppTarget = &cppTarget}, boostExampleOrTest);

                if (testTarget)
                {
                    testTarget->addDepNow<0>(cppTarget);
                }
            }
            else
            {
                DSC<CppTarget> &uintTest =
                    configuration->getCppExeDSCNoName(explicitBuild, nullptr, unitTestName).privateDeps(mainTarget);
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
                Node *node = Node::getNodeNonNormalized(p.path().generic_string(), true);
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
            for (CppTarget *dep : cppTestDepsPrivate)
            {
                testTarget.cppTarget->privateDeps(*dep);
            }
        }
        else
        {
            for (DSC<CppTarget> *dep : dscTestDepsPrivate)
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