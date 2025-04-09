// To compile this project follow the following steps. You must have the latest MSVC installed.

// 1)
// 2) Build HMake with the CMake Release configuration.
// 3) Add the build-dir to Environment Variable
// 4) Run htools as administrator. This registers the include and library dirs of the installed latest msvc tool.
// 5) Copy the Projects/boost/* to boost/
// 6) In boost/boost/    Run "hwrite -r ".*hpp".     --> This writes header-units.json files recursively. Otherwise,
// header-units not considered by MSVC for include translation.
//
// 7) In boost/ mkdir BoostBuild cd BoostBuild/hu/ScanOnly
// hbuild        --> This will just scan, but not build the hu configuration. This takes 43.355s on my 32 thread system.
// cd ..
// hbuild       --> This will full build the hu configuration. This takes 37.610s
// cd ../conventional
// hbuild       --> This will full build the conventional configuration. This takes 55.9s
// i.e.    header-units build is 1.48x faster if we exclude scanning.

#include "BoostCppTarget.hpp"
#include "arrayDeclarations.hpp"

using std::filesystem::directory_iterator;

void configurationSpecification(Configuration &config)
{
    config.stdCppTarget->getSourceTarget().publicIncludes(srcNode->filePath);
    config.assign(BuildTests::YES, BuildExamples::YES);

    // callable_traits

    config.getCppObject("ScanOnly");
    BoostCppTarget &callableTraits =
        config.getBoostCppTarget("callable_Traits")
            .addDir<BoostExampleOrTestType::RUN_EXAMPLE>("/libs/callable_traits/example")
            .addDir<BoostExampleOrTestType::RUN_TEST>("/libs/callable_traits/test")
            .addDirEndsWith<BoostExampleOrTestType::RUN_TEST>("/libs/callable_traits/test", "lazy");

    for (CppSourceTarget &cppTestTarget :
         callableTraits.getEndsWith<BoostExampleOrTestType::RUN_TEST, IteratorTargetType::CPP, BSMode::BUILD>("lazy"))
    {
        cppTestTarget.privateCompileDefinition("USE_LAZY_TYPES");
    }

    // config
    BoostCppTarget &configTarget = config.getBoostCppTarget("config");
    // Skipping test and check for now

    DSC<CppSourceTarget> &current = config.getCppStaticDSC("current-target");
    current.getSourceTarget().headerUnits("boost/current_function.hpp", "boost/version.hpp");

    BoostCppTarget &core = config.getBoostCppTarget("core", true, false).publicDeps(configTarget, current);

    BoostCppTarget &lambda2 = config.getBoostCppTarget("lambda2")
                                  .privateTestDeps(core.mainTarget, current)
                                  .addDir<BoostExampleOrTestType::RUN_TEST>("/libs/lambda2/test")
                                  .assignPrivateTestDeps();

    BoostCppTarget &leaf = config.getBoostCppTarget("leaf").add<BoostExampleOrTestType::RUN_TEST>(
        "libs/leaf/test", leafRunTests, std::size(leafRunTests));

    // No underlying API to differentiate between run-tests, compile-tests, compile-fail-tests, run-fail-tests etc.
    // leaf.addCompileTests("/libs/leaf/test", leafCompileTests, std::size(leafCompileTests));
    // leaf.addCompileFailTests("/libs/leaf/test", leafCompileFailTests, std::size(leafCompileFailTests));
    if (config.evaluate(ExceptionHandling::ON))
    {
        leaf.add<BoostExampleOrTestType::EXAMPLE>("libs/leaf/example", leafExamples, std::size(leafExamples));
    }
    BoostCppTarget &detail = config.getBoostCppTarget("detail", true, false).publicDeps(configTarget);
    BoostCppTarget &preprocessor = config.getBoostCppTarget("preprocessor");
    BoostCppTarget &typeTraits = config.getBoostCppTarget("type_traits").publicDeps(detail);
    BoostCppTarget &mpl = config.getBoostCppTarget("mpl", true, false).publicDeps(detail, preprocessor, typeTraits);
    BoostCppTarget &mp11 =
        config.getBoostCppTarget("mp11")
            .privateTestDeps(core.mainTarget, mpl.mainTarget, current)
            .add<BoostExampleOrTestType::RUN_TEST>("libs/mp11/test", mp11RunTests, std::size(mp11RunTests));

    // skipping predef tests and examples. header-only library with lots of configurations for its tests and examples
    BoostCppTarget &predef = config.getBoostCppTarget("predef", true, false);

    /*const char *preprocTestDir = "libs/preprocessor/test";
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

    // skipping pfr tests and examples. header-only library with lots of configurations for its tests and examples

    BoostCppTarget &pfr = config.getBoostCppTarget("pfr");
    pfr.addDirEndsWith<BoostExampleOrTestType::RUN_TEST>("/libs/pfr/test/config", "config")
        .privateTestDeps(preprocessor.mainTarget);
    for (CppSourceTarget &testTarget :
         pfr.getEndsWith<BoostExampleOrTestType::RUN_TEST, IteratorTargetType::CPP, BSMode::BUILD>("config"))
    {
        testTarget.privateCompileDefinition("BOOST_PFR_DETAIL_STRICT_RVALUE_TESTING");
    }
    pfr.assignPrivateTestDeps();*/
}

void buildSpecification()
{
    // This tries to build SFML similar to the current CMakeLists.txt. Currently, only Windows build is supported.
    // getConfiguration("conventional").assign(CxxSTD::V_LATEST, TargetType::LIBRARY_SHARED, TreatModuleAsSource::YES);
    getConfiguration("hu").assign(CxxSTD::V_LATEST, TargetType::LIBRARY_SHARED, TreatModuleAsSource::NO,
                                  TranslateInclude::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
