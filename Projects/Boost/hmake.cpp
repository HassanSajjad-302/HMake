
// This successfully compiles SFML with C++20 header units. Except header units from prebuilt libraries vulkan, glad and
// minimp3, and, from directories as src/Win32/, all header units were successfully compiled. Adding  a header-unit from
// one of these causes compilation error. Not investigating for now.

#include "BoostCppTarget.hpp"
#include "arrayDeclarations.hpp"

using std::filesystem::directory_iterator;

void configurationSpecification(Configuration &config)
{
    config.stdCppTarget->getSourceTarget().publicIncludes(srcNode->filePath);

    /* 1 - callable_traits
     * 2 - compatibility
     * 3 - config
     * 4 - headers
     * 5 - hof
     * 6 - lambda2
     *
     * 7 - leaf
     * 8 - mp11
     * 9 - pfr
     * 10 - predef
     * 11 - preprocessor
     * 12 - qvm
     * 13 - ratio
     */

    // callable_traits

    config.getCppObject("aevain");
    config.assign(BuildTests::YES, BuildExamples::YES, AssignStandardCppTarget::YES);

    BoostCppTarget callableTraits("callable_Traits", &config, true, true);
    // callableTraits.addDir<BoostExampleOrTestType::RUN_EXAMPLE>("/libs/callable_traits/example")
    //     .addDir<BoostExampleOrTestType::RUN_TEST>("/libs/callable_traits/test")
    //     .addDirEndsWith<BoostExampleOrTestType::RUN_TEST>("/libs/callable_traits/test", "lazy");
    //
    // for (CppSourceTarget &cppTestTarget :
    //      callableTraits.getEndsWith<BoostExampleOrTestType::RUN_TEST, IteratorTargetType::CPP,
    //      BSMode::BUILD>("lazy"))
    // {
    //     cppTestTarget.privateCompileDefinition("USE_LAZY_TYPES");
    // }

    // compatibility

    // BoostCppTarget compatibility("compatibility", &config, true, false);
    // compatibility.getSourceTarget().publicHUIncludes("libs/");

    // config
    BoostCppTarget configTarget("config", &config, true, false);
    // Skipping test and check for now

    // headers
    // BoostCppTarget headers("headers", &config, true, false);
    // This is a fake library for installing headers

    if (config.evaluate(CxxSTD::V_11) || config.evaluate(CxxSTD::V_14))
    {
        // Not working and no one depends on it.
        /*BoostCppTarget hof("hof", &configuration, true, true);
        hof.addRunTestsDirectory("/libs/hof/test");*/
    }

    DSC<CppSourceTarget> &current = config.getCppStaticDSC("current-target");
    current.getSourceTarget().headerUnits("boost/current_function.hpp");

    BoostCppTarget core("core", &config, true, false);
    core.mainTarget.publicLibraries(&current, configTarget.mainTarget);

    BoostCppTarget lambda2("lambda2", &config, true, false);
    lambda2.mainTarget.publicLibraries(&core.mainTarget);
    lambda2.addDir<BoostExampleOrTestType::RUN_TEST>("/libs/lambda2/test");

    return;
    BoostCppTarget leaf("leaf", &config, true, false);
    leaf.add<BoostExampleOrTestType::RUN_TEST>("libs/leaf/test", leafRunTests, std::size(leafRunTests));

    // No underlying API to differentiate between run-tests, compile-tests, compile-fail-tests, run-fail-tests etc.
    // leaf.addCompileTests("/libs/leaf/test", leafCompileTests, std::size(leafCompileTests));
    // leaf.addCompileFailTests("/libs/leaf/test", leafCompileFailTests, std::size(leafCompileFailTests));
    if (config.evaluate(ExceptionHandling::ON))
    {
        leaf.add<BoostExampleOrTestType::EXAMPLE>("libs/leaf/example", leafExamples, std::size(leafExamples));
        leaf.add<BoostExampleOrTestType::EXAMPLE>("libs/leaf/example/print_file", leafExamplesPrintFile,
                                                  std::size(leafExamplesPrintFile));
    }

    BoostCppTarget mp11("mp11", &config, true, false);
    mp11.add<BoostExampleOrTestType::RUN_TEST>("libs/mp11/test", mp11RunTests, std::size(mp11RunTests));
    // mp11.addCompileTests("/libs/mp11/test", mp11CompileTests, std::size(mp11CompileTests));

    // skipping pfr tests and examples. header-only library with lots of configurations for its tests and examples

    BoostCppTarget pfr("pfr", &config, true, false);
    pfr.addDirEndsWith<BoostExampleOrTestType::RUN_TEST>("/libs/pfr/test/config", "config");
    for (CppSourceTarget &testTarget :
         pfr.getEndsWith<BoostExampleOrTestType::RUN_TEST, IteratorTargetType::CPP, BSMode::BUILD>("config"))
    {
        testTarget.privateCompileDefinition("BOOST_PFR_DETAIL_STRICT_RVALUE_TESTING");
    }

    // skipping predef tests and examples. header-only library with lots of configurations for its tests and examples
    // BoostCppTarget predef("predef", &config, true, false);

    BoostCppTarget preprocessor("preprocessor", &config, true, false);
    const char *preprocTestDir = "libs/preprocessor/test";
    preprocessor
        .add<BoostExampleOrTestType::COMPILE_TEST>(preprocTestDir, preprocessorTests, std::size(preprocessorTests))
        .addEndsWith<BoostExampleOrTestType::COMPILE_TEST>("512", preprocTestDir, preprocessorTests512,
                                                           std::size(preprocessorTests512))
        .addEndsWith<BoostExampleOrTestType::COMPILE_TEST>("1024", preprocTestDir, preprocessorTests1024,
                                                           std::size(preprocessorTests1024))
        .addEndsWith<BoostExampleOrTestType::COMPILE_TEST>("V128", preprocTestDir, preprocessorV128,
                                                           std::size(preprocessorV128))
        .addEndsWith<BoostExampleOrTestType::COMPILE_TEST>("V256", preprocTestDir, preprocessorV256,
                                                           std::size(preprocessorV256))
        .addEndsWith<BoostExampleOrTestType::COMPILE_TEST>("empty", preprocTestDir, preprocessorIsEmpty,
                                                           std::size(preprocessorIsEmpty));

    auto preprocessorMacroDefines = [&](string_view innerBuildDirName, string_view cddName, string_view cddValue) {
        for (CppSourceTarget &cppTestTarget :
             preprocessor.getEndsWith<BoostExampleOrTestType::COMPILE_TEST, IteratorTargetType::CPP, BSMode::BUILD>())
        {
            cppTestTarget.privateCompileDefinition(string(cddName), string(cddValue));
        }
    };
    preprocessorMacroDefines("512", "BOOST_PP_LIMIT_MAG", "512");
    preprocessorMacroDefines("1024", "BOOST_PP_LIMIT_MAG", "1024");

    // callable_traits
    /* DSC<CppSourceTarget> &qvm = configuration.getCppObjectDSC("qvm").privateLibraries(&stdhu);
     if (tests == TESTS::YES)
     {
         for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/qvm/test")))
         {
             // All targets are just being compiled instead of being run as well.
             // Fail tests are not being considered
             if (k.path().extension() == ".cpp" && !k.path().string().contains("fail") &&
                 !k.path().string().contains("header-test.cpp"))
             {
                 DSC<CppSourceTarget> &qvmTest =
                     configuration.getCppExeDSC("test_qvm_" + k.path().filename().string()).privateLibraries(&stdhu);
                 qvmTest.getSourceTarget().moduleFiles(k.path().string());

                 DSC<CppSourceTarget> &qvmTestHpp =
                     configuration.getCppExeDSC("test_qvm_hpp_" +
     k.path().filename().string()).privateLibraries(&stdhu); qvmTestHpp.getSourceTarget()
                     .moduleFiles(k.path().string())
                     .privateCompileDefinition("BOOST_QVM_TEST_SINGLE_HEADER",
                                               addEscapedQuotes("libs/qvm/include/boost/qvm.hpp"));

                 // Commented out because lite tests do not pass
                 /*
                 DSC<CppSourceTarget> &qvmTestLiteHpp =
                     configuration.getCppExeDSC("test_qvm_lite_hpp_" +
                 k.path().filename().string()).privateLibraries(&stdhu); qvmTestLiteHpp.getSourceTarget()
                     .moduleFiles(k.path().string())
                     .privateCompileDefinition("BOOST_QVM_TEST_SINGLE_HEADER",
                                                 addEscapedQuotes("libs/qvm/include/boost/qvm_lite.hpp"));
                                                 #1#

                 /*DSC<CppSourceTarget> &target2 =
                     configuration.getCppExeDSC("test_qvm_" + k.path().filename().string() +
                 "__lazy").privateLibraries(&stdhu);
                 target2.getSourceTarget().moduleFiles(k.path().string()).privateCompileDefinition("USE_LAZY_TYPES");#1#
             }
         }
     }

     // Level 1
     DSC<CppSourceTarget> &assert = configuration.getCppObjectDSC("assert").privateLibraries(&stdhu);
     if (tests == TESTS::YES)
     {
         for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/assert/test")))
         {
             // All targets are just being compiled instead of being run as well.
             if (k.path().extension() == ".cpp")
             {
                 DSC<CppSourceTarget> &target =
                     configuration.getCppExeDSC("test_assert_" + k.path().filename().string()).privateLibraries(&stdhu);
                 target.getSourceTarget().moduleFiles(k.path().string());
             }
         }
     }*/
}

void buildSpecification()
{
    // This tries to build SFML similar to the current CMakeLists.txt. Currently, only Windows build is supported.
    getConfiguration("conventional")
        .assign(CxxSTD::V_LATEST, TargetType::LIBRARY_SHARED, TreatModuleAsSource::NO, TranslateInclude::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
