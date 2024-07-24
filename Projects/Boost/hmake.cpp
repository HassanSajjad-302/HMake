
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
    configuration.compilerFeatures.requirementIncludes.emplace_back(
        Node::getNodeFromNonNormalizedString(path(srcDir).parent_path().string(), false));
    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToPublicHUDirectories();

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
    DSC<CppSourceTarget> &callableTraits = configuration.GetCppObjectDSC("callable_traits");
    if (tests == TESTS::YES)
    {
        for (const auto &k : directory_iterator(path(srcDir + "/libs/callable_traits/test")))
        {
            // All targets are just being compiled instead of being run as well.
            if (k.path().extension() == ".cpp")
            {
                DSC<CppSourceTarget> &target =
                    configuration.GetCppExeDSC("test_callable_traits_" + k.path().filename().string());
                target.getSourceTarget().MODULE_FILES(k.path().string());

                DSC<CppSourceTarget> &target2 =
                    configuration.GetCppExeDSC("test_callable_traits_" + k.path().filename().string() + "__lazy");
                target2.getSourceTarget().MODULE_FILES(k.path().string()).PRIVATE_COMPILE_DEFINITION("USE_LAZY_TYPES");
            }
        }
    }

    if (examples == EXAMPLES::YES)
    {
        for (const auto &k : directory_iterator(path(srcDir + "/libs/callable_traits/example")))
        {
            // All targets are being compiled instead of being run as well
            if (k.path().extension() == ".cpp")
            {
                DSC<CppSourceTarget> &target =
                    configuration.GetCppExeDSC("example_callable_traits_" + k.path().filename().string());
                target.getSourceTarget().MODULE_FILES(k.path().string());
            }
        }
    }

    // compatibility
    DSC<CppSourceTarget> &compatibility = configuration.GetCppObjectDSC("compatibility");
    // compatibility.getSourceTarget().PUBLIC_HU_INCLUDES("libs/");

    // config
    DSC<CppSourceTarget> &config = configuration.GetCppObjectDSC("config");
    // Skipping test and check for now

    // headers
    DSC<CppSourceTarget> &headers = configuration.GetCppObjectDSC("headers");
    // This is a fake library for installing headers

    DSC<CppSourceTarget> &hof = configuration.GetCppObjectDSC("hof");
    if (tests == TESTS::YES)
    {
        for (const auto &k : directory_iterator(path(srcDir + "/libs/hof/test")))
        {
            // All targets are just being compiled instead of being run as well.
            // Not all tests are passing, hence skipping
            /*if (k.path().extension() == ".cpp")
            {
                DSC<CppSourceTarget> &target =
                    configuration.GetCppExeDSC("test_hof_" + k.path().filename().string());
                target.getSourceTarget().MODULE_FILES(k.path().string());
            }*/
        }
    }
    // hof example has no Jamfile

    DSC<CppSourceTarget> &lambda2 = configuration.GetCppObjectDSC("lambda2");
    if (tests == TESTS::YES)
    {
        for (const auto &k : directory_iterator(path(srcDir + "/libs/lambda2/test")))
        {
            // All targets are just being compiled instead of being run as well.
            if (k.path().extension() == ".cpp")
            {
                DSC<CppSourceTarget> &target =
                    configuration.GetCppExeDSC("test_lambda2_" + k.path().filename().string());
                target.getSourceTarget().MODULE_FILES(k.path().string());
            }
        }
    }

    DSC<CppSourceTarget> &leaf = configuration.GetCppObjectDSC("leaf");
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
                    configuration.GetCppExeDSC("test_leaf_" + k.path().filename().string());
                target.getSourceTarget().MODULE_FILES(k.path().string());
            }
        }*/
    }

    DSC<CppSourceTarget> &mp11 = configuration.GetCppObjectDSC("mp11");
    if (tests == TESTS::YES)
    {
        // test/Jamfile.v2 has compile, run, exe and configuration based selection and target specification
        for (const auto &k : directory_iterator(path(srcDir + "/libs/mp11/test")))
        {
            // All targets are just being compiled instead of being run as well.
            // mp_compose_sf.cpp is a static lib
            if (k.path().extension() == ".cpp")
            {
                if (!k.path().string().contains("compose_sf"))
                {
                    DSC<CppSourceTarget> &target =
                        configuration.GetCppExeDSC("test_mp11_" + k.path().filename().string());
                    target.getSourceTarget().MODULE_FILES(k.path().string());
                }
                else
                {
                    DSC<CppSourceTarget> &target = configuration.GetCppObjectDSC("test_mp11_mp_compose_sf");
                    target.getSourceTarget().MODULE_FILES("libs/mp11/test/mp_compose_sf.cpp");
                }
            }
        }
    }

    // skipping pfr. header-only library with not that many tests but lots of
    // configuration.

    // skipping predef. header-only library with not that many tests but lots of
    // configurations.

    DSC<CppSourceTarget> &preprocessor = configuration.GetCppObjectDSC("preprocessor");
    if (tests == TESTS::YES)
    {
        vector<string> vec{"arithmetic.cpp", "array.cpp",     "comparison.cpp", "control.cpp", "debug.cpp",
                           "facilities.cpp", "iteration.cpp", "list.cpp",       "logical.cpp", "punctuation.cpp",
                           "repetition.cpp", "selection.cpp", "seq.cpp",        "slot.cpp",    "stringize.cpp",
                           "tuple.cpp",      "variadic.cpp"};

        for (string &str : vec)
        {
            DSC<CppSourceTarget> &preprocessorTests = configuration.GetCppExeDSC("test_preprocessor_" + str);
            preprocessorTests.getSourceTarget().MODULE_FILES(srcDir + "libs/preprocessor/test/" + str);
            DSC<CppSourceTarget> &preprocessorNumber512Tests =
                configuration.GetCppExeDSC("test_preprocessor_number_512_" + str);
            preprocessorNumber512Tests.getSourceTarget()
                .MODULE_FILES(srcDir + "libs/preprocessor/test/" + str)
                .PRIVATE_COMPILE_DEFINITION("BOOST_PP_LIMIT_MAG=512");
            DSC<CppSourceTarget> &preprocessorNumber1024Tests =
                configuration.GetCppExeDSC("test_preprocessor_number_1024_" + str);
            preprocessorNumber1024Tests.getSourceTarget()
                .MODULE_FILES(srcDir + "libs/preprocessor/test/" + str)
                .PRIVATE_COMPILE_DEFINITION("BOOST_PP_LIMIT_MAG=1024");
            // Incomplete
        }
    }

    // callable_traits
    DSC<CppSourceTarget> &qvm = configuration.GetCppObjectDSC("qvm");
    if (tests == TESTS::YES)
    {
        for (const auto &k : directory_iterator(path(srcDir + "/libs/qvm/test")))
        {
            // All targets are just being compiled instead of being run as well.
            // Fail tests are not being considered
            if (k.path().extension() == ".cpp" && !k.path().string().contains("fail") &&
                !k.path().string().contains("header-test.cpp"))
            {
                DSC<CppSourceTarget> &qvmTest = configuration.GetCppExeDSC("test_qvm_" + k.path().filename().string());
                qvmTest.getSourceTarget().MODULE_FILES(k.path().string());

                DSC<CppSourceTarget> &qvmTestHpp =
                    configuration.GetCppExeDSC("test_qvm_hpp_" + k.path().filename().string());
                qvmTestHpp.getSourceTarget()
                    .MODULE_FILES(k.path().string())
                    .PRIVATE_COMPILE_DEFINITION("BOOST_QVM_TEST_SINGLE_HEADER",
                                                addEscapedQuotes("libs/qvm/include/boost/qvm.hpp"));

                // Commented out because lite tests do not pass
                /*
                DSC<CppSourceTarget> &qvmTestLiteHpp =
                    configuration.GetCppExeDSC("test_qvm_lite_hpp_" + k.path().filename().string());
                qvmTestLiteHpp.getSourceTarget()
                    .MODULE_FILES(k.path().string())
                    .PRIVATE_COMPILE_DEFINITION("BOOST_QVM_TEST_SINGLE_HEADER",
                                                addEscapedQuotes("libs/qvm/include/boost/qvm_lite.hpp"));
                                                */

                /*DSC<CppSourceTarget> &target2 =
                    configuration.GetCppExeDSC("test_qvm_" + k.path().filename().string() + "__lazy");
                target2.getSourceTarget().MODULE_FILES(k.path().string()).PRIVATE_COMPILE_DEFINITION("USE_LAZY_TYPES");*/
            }
        }
    }

    // Level 1
    DSC<CppSourceTarget> &assert = configuration.GetCppObjectDSC("assert");
    if (tests == TESTS::YES)
    {
        for (const auto &k : directory_iterator(path(srcDir + "/libs/assert/test")))
        {
            // All targets are just being compiled instead of being run as well.
            if (k.path().extension() == ".cpp")
            {
                DSC<CppSourceTarget> &target =
                    configuration.GetCppExeDSC("test_assert_" + k.path().filename().string());
                target.getSourceTarget().MODULE_FILES(k.path().string());
            }
        }
    }
}

void buildSpecification()
{
    // This tries to build SFML similar to the current CMakeLists.txt. Currently, only Windows build is supported.
    GetConfiguration("conventional").ASSIGN(CxxSTD::V_LATEST, TargetType::LIBRARY_SHARED, TreatModuleAsSource::YES);

    selectiveConfigurationSpecification(&configurationSpecification);
}

MAIN_FUNCTION
