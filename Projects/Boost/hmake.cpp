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
    config.getCppObject("ScanOnly");

    BoostCppTarget &callableTraits = config.getBoostCppTarget("callable_Traits");
    BoostCppTarget &configTarget = config.getBoostCppTarget("config");
    BoostCppTarget &lambda2 = config.getBoostCppTarget("lambda2");
    BoostCppTarget &variant = config.getBoostCppTarget("variant");
    BoostCppTarget &leaf = config.getBoostCppTarget("leaf");
    BoostCppTarget &mp11 = config.getBoostCppTarget("mp11");
    BoostCppTarget &pfr = config.getBoostCppTarget("pfr");
    BoostCppTarget &preprocessor = config.getBoostCppTarget("preprocessor");
    BoostCppTarget &predef = config.getBoostCppTarget("predef", true, false);

    DSC<CppSourceTarget> &cstdint = config.getCppTargetDSC("cstdint").publicDeps(configTarget.mainTarget);
    cstdint.getSourceTarget().headerUnits("boost/cstdint.hpp");
    BoostCppTarget &assertTarget = config.getBoostCppTarget("assert").publicDeps(cstdint);
    BoostCppTarget &exception = config.getBoostCppTarget("exception", true, false).publicDeps(assertTarget, cstdint);
    DSC<CppSourceTarget> &current = config.getCppStaticDSC("current-target").publicDeps(exception.mainTarget, cstdint);
    BoostCppTarget &core = config.getBoostCppTarget("core", true, false).publicDeps(configTarget, current);
    BoostCppTarget &winApi = config.getBoostCppTarget("winapi", true, false).publicDeps(configTarget);
    BoostCppTarget &detail = config.getBoostCppTarget("detail", true, false).publicDeps(configTarget);
    DSC<CppSourceTarget> &staticAssert =
        config.getCppTargetDSC("staticAssert").publicDeps(configTarget.mainTarget, detail.mainTarget);
    staticAssert.getSourceTarget().headerUnits("boost/static_assert.hpp");
    DSC<CppSourceTarget> &version = config.getCppStaticDSC("version-header");
    version.getSourceTarget().headerUnits("boost/version.hpp");
    BoostCppTarget &typeTraits =
        config.getBoostCppTarget("type_traits").publicDeps(configTarget, staticAssert, detail, version);
    BoostCppTarget &mpl = config.getBoostCppTarget("mpl", true, false).publicDeps(detail, preprocessor, typeTraits);
    BoostCppTarget &variant2 = config.getBoostCppTarget("variant2").publicDeps(mp11, assertTarget);
    BoostCppTarget &system =
        config.getBoostCppTarget("system").publicDeps(configTarget, variant2, assertTarget, winApi.mainTarget, current);
    BoostCppTarget &function = config.getBoostCppTarget("function").publicDeps(assertTarget, core);
    BoostCppTarget &move = config.getBoostCppTarget("move", true, false).publicDeps(configTarget);
    DSC<CppSourceTarget> &nonCopyable = config.getCppStaticDSC("nonCopyable").publicDeps(core.mainTarget);
    nonCopyable.getSourceTarget().headerUnits("boost/noncopyable.hpp");
    BoostCppTarget &bind = config.getBoostCppTarget("bind");
    DSC<CppSourceTarget> &getPointerHeader =
        config.getCppStaticDSC("getPointerHeader").publicDeps(configTarget.mainTarget);
    bind.publicDeps(getPointerHeader);
    getPointerHeader.getSourceTarget().headerUnits("boost/get_pointer.hpp");
    DSC<CppSourceTarget> &memFnHeader =
        config.getCppStaticDSC("memfn-header").publicDeps(bind.mainTarget, getPointerHeader);
    memFnHeader.getSourceTarget().headerUnits("boost/mem_fn.hpp");
    function.publicDeps(memFnHeader);

    callableTraits.addDir<BoostExampleOrTestType::RUN_EXAMPLE>("/libs/callable_traits/example")
        .addDir<BoostExampleOrTestType::RUN_TEST>("/libs/callable_traits/test")
        .addDirEndsWith<BoostExampleOrTestType::RUN_TEST>("/libs/callable_traits/test", "lazy");
    for (CppSourceTarget &cppTestTarget :
         callableTraits.getEndsWith<BoostExampleOrTestType::RUN_TEST, IteratorTargetType::CPP, BSMode::BUILD>("lazy"))
    {
        cppTestTarget.privateCompileDefinition("USE_LAZY_TYPES");
    }

    current.getSourceTarget().headerUnits("boost/current_function.hpp", "boost/throw_exception.hpp",
                                          "boost/function_equal.hpp");

    lambda2.privateTestDeps(core.mainTarget, current, version)
        .addDir<BoostExampleOrTestType::RUN_TEST>("/libs/lambda2/test")
        .assignPrivateTestDeps();

    mp11.privateTestDeps(core.mainTarget, mpl.mainTarget, current)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/mp11/test", mp11RunTests, std::size(mp11RunTests));

    system.privateTestDeps(core.mainTarget, exception.mainTarget)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/system/test", systemRunTests, std::size(systemRunTests));
    // skipping predef tests and examples. header-only library with lots of configurations for its tests and examples

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

    // skipping pfr tests and examples. header-only library with lots of configurations for its tests and examples

    pfr.addDirEndsWith<BoostExampleOrTestType::RUN_TEST>("/libs/pfr/test/config", "config")
        .privateTestDeps(preprocessor.mainTarget);
    for (CppSourceTarget &testTarget :
         pfr.getEndsWith<BoostExampleOrTestType::RUN_TEST, IteratorTargetType::CPP, BSMode::BUILD>("config"))
    {
        testTarget.privateCompileDefinition("BOOST_PFR_DETAIL_STRICT_RVALUE_TESTING");
    }
    pfr.assignPrivateTestDeps();

    typeTraits
        .privateTestDeps(memFnHeader, getPointerHeader, nonCopyable, bind.mainTarget, core.mainTarget,
                         function.mainTarget, mpl.mainTarget, move.mainTarget)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/type_traits/test", typeTraitsRunTests,
                                               std::size(typeTraitsRunTests));
}

void buildSpecification()
{
    removeTroublingHu(headerUnitsJsonDirs, std::size(headerUnitsJsonDirs), headerUnitsJsonEntry,
                      std::size(headerUnitsJsonEntry));
    // This tries to build SFML similar to the current CMakeLists.txt. Currently, only Windows build is supported.
    getConfiguration("conventional").assign(CxxSTD::V_LATEST, TargetType::LIBRARY_STATIC, TreatModuleAsSource::YES);
    getConfiguration("hu").assign(CxxSTD::V_LATEST, TargetType::LIBRARY_STATIC, TreatModuleAsSource::NO,
                                  TranslateInclude::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
