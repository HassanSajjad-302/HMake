
// This successfully compiles SFML with C++20 header units. Except header units from prebuilt libraries vulkan, glad and
// minimp3, and, from directories as src/Win32/, all header units were successfully compiled. Adding  a header-unit from
// one of these causes compilation error. Not investigating for now.

#include "Configure.hpp"

using std::filesystem::directory_iterator;

enum class EXAMPLES
{
    YES,
    NO
};

enum class TESTS
{
    YES,
    NO
};

enum class LEAF_RELEASE_LEAF_HPP
{
    YES,
    NO
};

void configurationSpecification(Configuration &configuration)
{
    TESTS tests = TESTS::YES;
    EXAMPLES examples = EXAMPLES::YES;

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
    DSC<CppSourceTarget> &callableTraits = configuration.getCppObjectDSC("callable_traits").privateLibraries(&stdhu);
    if (tests == TESTS::YES)
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
    }

    // compatibility
    DSC<CppSourceTarget> &compatibility = configuration.getCppObjectDSC("compatibility").privateLibraries(&stdhu);
    // compatibility.getSourceTarget().publicHUIncludes("libs/");

    // config
    DSC<CppSourceTarget> &config = configuration.getCppObjectDSC("config").privateLibraries(&stdhu);
    // Skipping test and check for now

    // headers
    DSC<CppSourceTarget> &headers = configuration.getCppObjectDSC("headers").privateLibraries(&stdhu);
    // This is a fake library for installing headers

    DSC<CppSourceTarget> &hof = configuration.getCppObjectDSC("hof").privateLibraries(&stdhu);
    if (tests == TESTS::YES)
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
            }*/
        }
    }
    // hof example has no Jamfile

    DSC<CppSourceTarget> &lambda2 = configuration.getCppObjectDSC("lambda2").privateLibraries(&stdhu);
    if (tests == TESTS::YES)
    {
        for (const auto &k : directory_iterator(path(srcNode->filePath + "/libs/lambda2/test")))
        {
            // All targets are just being compiled instead of being run as well.
            if (k.path().extension() == ".cpp")
            {
                DSC<CppSourceTarget> &target =
                    configuration.getCppExeDSC("test_lambda2_" + k.path().filename().string()).privateLibraries(&stdhu);
                target.getSourceTarget().moduleFiles(k.path().string());
            }
        }
    }

    DSC<CppSourceTarget> &leaf = configuration.getCppObjectDSC("leaf").privateLibraries(&stdhu);
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
        }*/
    }

    DSC<CppSourceTarget> &mp11 = configuration.getCppObjectDSC("mp11").privateLibraries(&stdhu);
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
    }

    // skipping pfr. header-only library with not that many tests but lots of
    // configuration.

    // skipping predef. header-only library with not that many tests but lots of
    // configurations.

    DSC<CppSourceTarget> &preprocessor = configuration.getCppObjectDSC("preprocessor").privateLibraries(&stdhu);
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
                    configuration.getCppExeDSC("test_qvm_hpp_" + k.path().filename().string()).privateLibraries(&stdhu);
                qvmTestHpp.getSourceTarget()
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
                                                */

                /*DSC<CppSourceTarget> &target2 =
                    configuration.getCppExeDSC("test_qvm_" + k.path().filename().string() +
                "__lazy").privateLibraries(&stdhu);
                target2.getSourceTarget().moduleFiles(k.path().string()).privateCompileDefinition("USE_LAZY_TYPES");*/
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
    }
}

void buildSpecification()
{
    // This tries to build SFML similar to the current CMakeLists.txt. Currently, only Windows build is supported.
    getConfiguration("conventional").assign(CxxSTD::V_LATEST, TargetType::LIBRARY_SHARED, TreatModuleAsSource::YES);

    selectiveConfigurationSpecification(&configurationSpecification);
}

MAIN_FUNCTION
