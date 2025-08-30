// To compile this project follow the following steps. You must have the latest MSVC installed ( 14.44.35207).

// 1) Download boost_1_88_0.zip and extract it in C:\boost (important for shorter compile-commands).
// 2) Build HMake with the CMake Release configuration.
// 3) Add the build-dir to Environment Variable
// 4) Run htools as administrator. This registers the include and library dirs of the installed latest msvc tool.
// 5) Copy the Projects/boost/* to boost/
// 6) In C:\boost/boost/    Run "hwrite -r .hpp".     --> This writes header-units.json files recursively. Otherwise,
// header-units not considered by MSVC for include translation.
//
// 7) In boost/, mkdir BoostBuild, cd BoostBuild.
// 8) hhelper
// 9) hhelper
// 10) cd conventional-d && hbuild    --> This will full build the conventional-d configuration.
// 11) cd ../hu-d/ScanOnly   &&   hbuild    --> This will just scan, but not build the hu-d configuration.
// 12) cd ../ && hbuild      --> This will full build the hu-d configuration. Just sit-back and enjoy.

// Results

// hu-d scanning    43.00
// hu-d             30.35
// conventional-d   53.07

// hu-r scanning    38.12
// hu-r             56.31
// conventional-r     100.58

// Conclusion

// Scanning is extremely slow for header-units. If we exclude scanning, we get
// 1.74x and 1.78x build speed-up in Debug and Release Configuration respectively.

// I used ptime for time measurements. And tested on modern hardware with 32 threads x86-64 cpu on Windows11.

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
    BoostCppTarget &preprocessor = config.getBoostCppTarget("preprocessor");
    BoostCppTarget &predef = config.getBoostCppTarget("predef", true, false);

    DSC<CppSourceTarget> &cstdint = config.getCppObjectDSC("cstdint").publicDeps(configTarget.mainTarget);
    cstdint.getSourceTarget().headerUnits("boost/cstdint.hpp");
    BoostCppTarget &assertTarget = config.getBoostCppTarget("assert").publicDeps(cstdint);
    BoostCppTarget &exception = config.getBoostCppTarget("exception", true, false).publicDeps(assertTarget);
    DSC<CppSourceTarget> &throwExceptionHeader =
        config.getCppObjectDSC("current-target").publicDeps(exception.mainTarget, cstdint);
    BoostCppTarget &core = config.getBoostCppTarget("core", true, false).publicDeps(configTarget, throwExceptionHeader);
    BoostCppTarget &winApi = config.getBoostCppTarget("winapi", true, false).publicDeps(configTarget);
    DSC<CppSourceTarget> &staticAssert = config.getCppObjectDSC("staticAssert").publicDeps(configTarget.mainTarget);
    staticAssert.getSourceTarget().headerUnits("boost/static_assert.hpp");
    BoostCppTarget &typeTraits = config.getBoostCppTarget("type_traits").publicDeps(staticAssert);
    BoostCppTarget &mpl = config.getBoostCppTarget("mpl", true, false).publicDeps(preprocessor, typeTraits);
    BoostCppTarget &variant2 = config.getBoostCppTarget("variant2").publicDeps(mp11, assertTarget);
    BoostCppTarget &system = config.getBoostCppTarget("system").publicDeps(configTarget, variant2, assertTarget,
                                                                           winApi.mainTarget, throwExceptionHeader);
    BoostCppTarget &function = config.getBoostCppTarget("function").publicDeps(assertTarget, core);
    BoostCppTarget &move = config.getBoostCppTarget("move", true, false).publicDeps(configTarget);
    BoostCppTarget &bind = config.getBoostCppTarget("bind");
    DSC<CppSourceTarget> &getPointerHeader =
        config.getCppObjectDSC("getPointerHeader").publicDeps(configTarget.mainTarget);
    throwExceptionHeader.getSourceTarget().headerUnits("boost/throw_exception.hpp");
    bind.publicDeps(getPointerHeader);
    getPointerHeader.getSourceTarget().headerUnits("boost/get_pointer.hpp");
    BoostCppTarget &describe = config.getBoostCppTarget("describe").publicDeps(mp11);
    BoostCppTarget &containerHash =
        config.getBoostCppTarget("container_hash", true, false).publicDeps(describe, typeTraits);
    BoostCppTarget &io = config.getBoostCppTarget("io", true, false).publicDeps(configTarget);
    BoostCppTarget &utility = config.getBoostCppTarget("utility").publicDeps(preprocessor, core, io, typeTraits);
    DSC<CppSourceTarget> &operatorsHeader = config.getCppObjectDSC("operators-header").publicDeps(core.mainTarget);
    operatorsHeader.getSourceTarget().headerUnits("boost/operators.hpp");
    BoostCppTarget &detail = config.getBoostCppTarget("detail", true, false).publicDeps(configTarget, typeTraits);
    DSC<CppSourceTarget> &limits = config.getCppObjectDSC("limits").publicDeps(configTarget.mainTarget);
    limits.getSourceTarget().headerUnits("boost/limits.hpp");
    // BoostCppTarget &container = config.getBoostCppTarget("container", fals, false);
    BoostCppTarget &hash2 = config.getBoostCppTarget("hash2", true, false).publicDeps(assertTarget, containerHash);
    DSC<CppSourceTarget> &arrayHeader =
        config.getCppStaticDSC("array_header").publicDeps(assertTarget.mainTarget, staticAssert, throwExceptionHeader);
    arrayHeader.getSourceTarget().headerUnits("boost/array.hpp");

    lambda2.privateTestDeps(core.mainTarget, throwExceptionHeader)
        .addDir<BoostExampleOrTestType::RUN_TEST>("/libs/lambda2/test")
        .assignPrivateTestDeps();

    mp11.privateTestDeps(core.mainTarget, mpl.mainTarget, throwExceptionHeader)
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

    typeTraits
        .privateTestDeps(getPointerHeader, bind.mainTarget, core.mainTarget, function.mainTarget, mpl.mainTarget,
                         move.mainTarget)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/type_traits/test", typeTraitsRunTests,
                                               std::size(typeTraitsRunTests));

    describe.privateTestDeps(core.mainTarget)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/describe/test", describeRunTests, std::size(describeRunTests));

    io.privateTestDeps(core.mainTarget, typeTraits.mainTarget)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/io/test", ioRunTests, std::size(ioRunTests));

    utility.privateTestDeps(containerHash.mainTarget, detail.mainTarget, operatorsHeader)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/utility/test", utilityRunTests, std::size(utilityRunTests));

    containerHash.privateTestDeps(utility.mainTarget, limits)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/container_hash/test", containerHashRunTests,
                                               std::size(containerHashRunTests));

    hash2.privateTestDeps(core.mainTarget, arrayHeader, utility.mainTarget)
        .add<BoostExampleOrTestType::RUN_TEST>("libs/hash2/test", hash2RunTests, std::size(hash2RunTests));
}

void buildSpecification()
{
    removeTroublingHu(headerUnitsJsonDirs, std::size(headerUnitsJsonDirs), headerUnitsJsonEntry,
                      std::size(headerUnitsJsonEntry));

    getConfiguration("conventional-r").assign(TreatModuleAsSource::YES, ConfigType::RELEASE);
    // getConfiguration("hu-r").assign(TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::RELEASE);
    getConfiguration("conventional-d").assign(TreatModuleAsSource::YES, ConfigType::DEBUG);
    // getConfiguration("hu-d").assign(TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::DEBUG);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
