
// This successfully compiles SFML with C++20 header units. Except header units from prebuilt libraries vulkan, glad and
// minimp3, and, from directories as src/Win32/, all header units were successfully compiled. Adding  a header-unit from
// one of these causes compilation error. Not investigating for now.

#include "BoostCppTarget.hpp"
#include "arrayDeclarations.hpp"

using std::filesystem::directory_iterator;

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.getCppObjectDSC("stdhu");
    stdhu.getSourceTarget().makeReqInclsUseable().publicHUIncludes(srcNode->filePath);

    stdhu.getLinkOrArchiveTarget().usageRequirementLibraryDirectories =
        stdhu.getLinkOrArchiveTarget().requirementLibraryDirectories;

    configuration.compilerFeatures.reqIncls.clear();
    configuration.prebuiltBasicFeatures.requirementLibraryDirectories.clear();

    /*
     * 1 - callable_traits
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

    configuration.assign(&stdhu, BuildTests::YES, BuildExamples::YES, AssignStandardCppTarget::YES);

    BoostCppTarget callableTraits("callableTraits", &configuration, true, true);
    callableTraits.addRunTestsDirectory("/libs/callable_traits/test");
    callableTraits.addRunTestsDirectoryEndsWith("/libs/callable_traits/test", "lazy");
    for (CppSourceTarget &cppTestTarget :
         callableTraits.getEndsWith<BoostExampleOrTestType::RUN_TEST, IteratorTargetType::CPP, BSMode::BUILD>("lazy"))
    {
        cppTestTarget.privateCompileDefinition("USE_LAZY_TYPES");
    }
    callableTraits.addRunExamplesDirectory("/libs/callable_traits/example");

    /*   /*DSC<CppSourceTarget> &callableTraits =
       configuration.getCppObjectDSC("callable_traits").privateLibraries(&stdhu); if (tests == TESTS::YES)
       {
           for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/callable_traits/test")))
           {
               // All targets are just being compiled instead of being run as well.
               if (k.path().extension() == ".cpp")
               {
                   DSC<CppSourceTarget> &target =
                       configuration.getCppExeDSC("test_callable_traits_" + k.path().filename().string())
                           .privateLibraries(&stdhu);
                   target.getSourceTarget().moduleFiles(k.path().string());

                   DSC<CppSourceTarget> &target2 =
                       configuration.getCppExeDSC("test_callable_traits_" + k.path().filename().string() + "__lazy")
                           .privateLibraries(&stdhu);
                   target2.getSourceTarget().moduleFiles(k.path().string()).privateCompileDefinition("USE_LAZY_TYPES");
               }
           }
       }

       if (examples == EXAMPLES::YES)
       {
           for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/callable_traits/example")))
           {
               // All targets are being compiled instead of being run as well
               if (k.path().extension() == ".cpp")
               {
                   DSC<CppSourceTarget> &target =
                       configuration.getCppExeDSC("example_callable_traits_" + k.path().filename().string())
                           .privateLibraries(&stdhu);
                   target.getSourceTarget().moduleFiles(k.path().string());
               }
           }
       }*/

    // compatibility
    BoostCppTarget compatibility("compatibility", &configuration, true, false);
    // compatibility.getSourceTarget().publicHUIncludes("libs/");

    // config
    BoostCppTarget config("config", &configuration, true, false);
    // Skipping test and check for now

    // headers
    BoostCppTarget headers("headers", &configuration, true, false);
    // This is a fake library for installing headers

    if (configuration.evaluate(CxxSTD::V_11) || configuration.evaluate(CxxSTD::V_14))
    {
        // Not working and no one depends on it.
        /*BoostCppTarget hof("hof", &configuration, true, true);
        hof.addRunTestsDirectory("/libs/hof/test");*/
    }
    /* if (tests == TESTS::YES)
     {
         for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/hof/test")))
         {
             // All targets are just being compiled instead of being run as well.
             // Not all tests are passing, hence skipping
             /*if (k.path().extension() == ".cpp")
             {
                 DSC<CppSourceTarget> &target =
                     configuration.getCppExeDSC("test_hof_" + k.path().filename().string()).privateLibraries(&stdhu);
                 target.getSourceTarget().moduleFiles(k.path().string());
             }#1#
         }
     }
     // hof example has no Jamfile
     */

    BoostCppTarget lambda2("lambda2", &configuration, true, false);
    lambda2.addRunTestsDirectory("/libs/lambda2/test");

    /*DSC<CppSourceTarget> &lambda2 = configuration.getCppObjectDSC("lambda2").privateLibraries(&stdhu);
    if (tests == TESTS::YES)
    {
        for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/lambda2/test")))
        {
            // All targets are just being compiled instead of being run as well.
            if (k.path().extension() == ".cpp")
            {
                DSC<CppSourceTarget> &target =
                    configuration.getCppExeDSC("test_lambda2_" +
    k.path().filename().string()).privateLibraries(&stdhu); target.getSourceTarget().moduleFiles(k.path().string());
            }
        }
    }*/

    BoostCppTarget leaf("leaf", &configuration, true, false);
    leaf.addRunTests("/libs/leaf/test", leafRunTests, std::size(leafRunTests));

    // No underlying API to differentiate between run-tests, compile-tests, compile-fail-tests, run-fail-tests etc.
    // leaf.addCompileTests("/libs/leaf/test", leafCompileTests, std::size(leafCompileTests));
    // leaf.addCompileFailTests("/libs/leaf/test", leafCompileFailTests, std::size(leafCompileFailTests));
    if (configuration.evaluate(ExceptionHandling::ON))
    {
        leaf.addExamples("libs/leaf/example", leafExamples, std::size(leafExamples));
        leaf.addExamples("libs/leaf/example/print_file", leafExamplesPrintFile, std::size(leafExamplesPrintFile));
    }

    /* DSC<CppSourceTarget> &leaf = configuration.getCppObjectDSC("leaf").privateLibraries(&stdhu);
    if (tests == TESTS::YES)
    {
        // test/Jamfile.v2 has compile, run, exe and configuration based selection and target specification
        /*for (const auto &k : directory_iterator(path(srcDir + "/libs/leaf/test")))
        {
            // All targets are just being compiled instead of being run as well.
            // Those tests that should fail to compile are not being dealt yet
            if (k.path().extension() == ".cpp" && !k.path().string().contains("fail"))
            {
                DSC<CppSourceTarget> &target =
                    configuration.getCppExeDSC("test_leaf_" + k.path().filename().string()).privateLibraries(&stdhu);
                target.getSourceTarget().moduleFiles(k.path().string());
            }
        }
    }*/

    BoostCppTarget mp11("mp11", &configuration, true, false);
    mp11.addRunTests("/libs/mp11/test", mp11RunTests, std::size(mp11RunTests));
    // mp11.addCompileTests("/libs/mp11/test", mp11CompileTests, std::size(mp11CompileTests));

    /*DSC<CppSourceTarget> &mp11 = configuration.getCppObjectDSC("mp11").privateLibraries(&stdhu);
    if (tests == TESTS::YES)
    {
        // test/Jamfile.v2 has compile, run, exe and configuration based selection and target specification
        for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/mp11/test")))
        {
            // All targets are just being compiled instead of being run as well.
            // mp_compose_sf.cpp is a static lib
            if (k.path().extension() == ".cpp")
            {
                if (!k.path().string().contains("compose_sf"))
                {
                    DSC<CppSourceTarget> &target =
                        configuration.getCppExeDSC("test_mp11_" + k.path().filename().string())
                            .privateLibraries(&stdhu);
                    target.getSourceTarget().moduleFiles(k.path().string());
                }
                else
                {
                    DSC<CppSourceTarget> &target =
                        configuration.getCppObjectDSC("test_mp11_mp_compose_sf").privateLibraries(&stdhu);
                    target.getSourceTarget().moduleFiles("libs/mp11/test/mp_compose_sf.cpp");
                }
            }
        }
    }*/

    // skipping pfr. header-only library with not that many tests but lots of
    // configuration.

    BoostCppTarget pfr("pfr", &configuration, true, false);
    pfr.addRunTestsDirectory("/libs/pfr/test/config");
    // mp11.addCompileTests("/libs/mp11/test", mp11CompileTests, std::size(mp11CompileTests));

    // skipping predef. header-only library with not that many tests but lots of
    // configurations.

    /* DSC<CppSourceTarget> &preprocessor = configuration.getCppObjectDSC("preprocessor").privateLibraries(&stdhu);
     if (tests == TESTS::YES)
     {
         vector<string> vec{"arithmetic.cpp", "array.cpp",     "comparison.cpp", "control.cpp", "debug.cpp",
                            "facilities.cpp", "iteration.cpp", "list.cpp",       "logical.cpp", "punctuation.cpp",
                            "repetition.cpp", "selection.cpp", "seq.cpp",        "slot.cpp",    "stringize.cpp",
                            "tuple.cpp",      "variadic.cpp"};

         for (string &str : vec)
         {
             DSC<CppSourceTarget> &preprocessorTests =
                 configuration.getCppExeDSC("test_preprocessor_" + str).privateLibraries(&stdhu);
             preprocessorTests.getSourceTarget().moduleFiles(srcNode->filePath + "/libs/preprocessor/test/" + str);
             DSC<CppSourceTarget> &preprocessorNumber512Tests =
                 configuration.getCppExeDSC("test_preprocessor_number_512_" + str).privateLibraries(&stdhu);
             preprocessorNumber512Tests.getSourceTarget()
                 .moduleFiles(srcNode->filePath + "/libs/preprocessor/test/" + str)
                 .privateCompileDefinition("BOOST_PP_LIMIT_MAG=512");
             DSC<CppSourceTarget> &preprocessorNumber1024Tests =
                 configuration.getCppExeDSC("test_preprocessor_number_1024_" + str).privateLibraries(&stdhu);
             preprocessorNumber1024Tests.getSourceTarget()
                 .moduleFiles(srcNode->filePath + "/libs/preprocessor/test/" + str)
                 .privateCompileDefinition("BOOST_PP_LIMIT_MAG=1024");
             // Incomplete
         }
     }

     // callable_traits
     DSC<CppSourceTarget> &qvm = configuration.getCppObjectDSC("qvm").privateLibraries(&stdhu);
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
    getConfiguration("conventional").assign(CxxSTD::V_LATEST, TargetType::LIBRARY_SHARED, TreatModuleAsSource::YES);

    selectiveConfigurationSpecification(&configurationSpecification);
}

MAIN_FUNCTION
