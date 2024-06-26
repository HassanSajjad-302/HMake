#include "Configure.hpp"

string thirdPartyPath = "Engine/Source/ThirdParty";
string win64blake3Path = thirdPartyPath + "/BLAKE3/1.3.1/lib/Win64/Release";
string runtimePath = "Engine/Source/Runtime";
string developerPath = "Engine/Source/Developer";
string corePath = runtimePath + "/core";

void addPublicPathsFrom(DSC<CppSourceTarget> &to, DSC<CppSourceTarget> &from, bool isPrivate)
{
    if (isPrivate)
    {
        for (const InclNode node : from.getSourceTarget().usageRequirementIncludes)
        {
            to.getSourceTarget().PRIVATE_INCLUDES(node.node->filePath);
        }
    }
    else
    {
        for (const InclNode node : from.getSourceTarget().usageRequirementIncludes)
        {
            to.getSourceTarget().PUBLIC_INCLUDES(node.node->filePath);
        }
    }
}

void initializeTarget(DSC<CppSourceTarget> &target, const string &dirPath)
{
    // if (exists(path(dirPath / "")))
}

void buildSpecification()
{
    DSC<CppSourceTarget> &blake3 = GetCppStaticDSC_P("blake3", win64blake3Path);
    blake3.getSourceTarget().PUBLIC_HU_INCLUDES(thirdPartyPath + "/BLAKE3/1.3.1/c");
    // Type = ModuleType.External;

    DSC<CppSourceTarget> &buildSettings = GetCppSharedDSC("buildSettings");
    // TODO

    string oodlePath = runtimePath + "/OodleDataCompression/Sdks/2.9.10";
    DSC<CppSourceTarget> &oodleDataCompression = GetCppStaticDSC_P("oo2core_win64", oodlePath + "/lib/Win64");
    string oodleVersionIncludeDirectory = oodlePath + "/include";
    oodleDataCompression.getSourceTarget().PUBLIC_HU_INCLUDES(oodleVersionIncludeDirectory);

    DSC<CppSourceTarget> &traceLog = GetCppSharedDSC("TraceLog");
    traceLog.getSourceTarget()
        .SOURCE_DIRECTORIES_RG(runtimePath + "/TraceLog/Private/Trace", ".*cpp")
        .SOURCE_DIRECTORIES_RG(runtimePath + "/TraceLog/Private/Trace/Important", ".*cpp")
        .SOURCE_DIRECTORIES_RG(runtimePath + "/TraceLog/Private/Trace/Detail/Windows", ".*cpp")
        .PRIVATE_COMPILER_FLAGS(
            R"(/FI"C:\Users\hassan\UE5\Engine\Intermediate\Build\Win64\x64\UnrealEditor\Development\TraceLog\Definitions.h" )")
        .PUBLIC_HU_INCLUDES(runtimePath + "/TraceLog/Public")
        .PRIVATE_HU_INCLUDES(runtimePath + "/TraceLog/Private")
        .ASSIGN(CxxSTD::V_20, RTTI::OFF, ExceptionHandling::OFF);

    // TODO: Change to GetCppShared
    DSC<CppSourceTarget> &derivedDataCache = GetCppObjectDSC("DerivedDataCache");
    derivedDataCache.getSourceTarget().PUBLIC_HU_INCLUDES(developerPath + "/DerivedDataCache/Public");
    // TODO: LINE1

    // TODO: Change to GetCppShared
    DSC<CppSourceTarget> &targetPlatform = GetCppObjectDSC("TargetPlatform");
    targetPlatform.getSourceTarget().PUBLIC_HU_INCLUDES(developerPath + "/TargetPlatform/Public");
    // TODO: LINE1

    // TODO: Change to GetCppShared
    DSC<CppSourceTarget> &directoryWatcher = GetCppObjectDSC("DirectroyWatcher");
    directoryWatcher.getSourceTarget().PUBLIC_HU_INCLUDES(developerPath + "/DirectoryWatcher/Public");
    // TODO: LINE1

    DSC<CppSourceTarget> &zlib =
        GetCppStaticDSC_P("zlibstatic", thirdPartyPath + "/zlib/1.2.13/lib/Win64/arm64/Release");
    zlib.getSourceTarget().PUBLIC_INCLUDES(thirdPartyPath + "/zlib/1.2.13/include");

    DSC<CppSourceTarget> &intelTBB =
        GetCppStaticDSC_P("tbb", thirdPartyPath + "/Intel/TBB/IntelTBB-2019u8/lib/Win64/vc14");
    intelTBB.getSourceTarget()
        .PUBLIC_COMPILE_DEFINITION("__TBBMALLOC_NO_IMPLICIT_LINKAGE", "1")
        .PUBLIC_COMPILE_DEFINITION("__TBB_NO_IMPLICIT_LINKAGE", "1")
        .PUBLIC_INCLUDES(thirdPartyPath + "/Intel/TBB/IntelTBB-2019u8/include");

    DSC<CppSourceTarget> &intelMallocTBB =
        GetCppStaticDSC_P("tbbmalloc", thirdPartyPath + "/Intel/TBB/IntelTBB-2019u8/lib/Win64/vc14");

    DSC<CppSourceTarget> &intelVTune =
        GetCppStaticDSC_P("libittnotify", thirdPartyPath + "/Intel/VTune/VTune-2019/lib/Win64");
    intelVTune.getSourceTarget().PUBLIC_HU_INCLUDES(thirdPartyPath + "/Intel/VTune/VTune-2019/lib/Win64");

    // Core.Build.cs: 34

    DSC<CppSourceTarget> &core = GetCppSharedDSC("core");

    core.PRIVATE_LIBRARIES(&blake3, &oodleDataCompression, &zlib, &intelTBB, &intelMallocTBB, &intelVTune);

    core.getSourceTarget()
        .PRIVATE_COMPILE_DEFINITION("YIELD_BETWEEN_TASKS", "1")
        .PUBLIC_COMPILE_DEFINITION("WITH_ADDITIONAL_CRASH_CONTEXTS", "1")
        .PUBLIC_COMPILE_DEFINITION("WITH_CONCURRENCYVIEWER_PROFILER", "0")
        .PUBLIC_COMPILE_DEFINITION("WITH_DIRECTXMATH", "0")
        .PUBLIC_COMPILE_DEFINITION("UE_WITH_IRIS", "1")
        .PRIVATE_COMPILE_DEFINITION("PLATFORM_SUPPORTS_BINARYCONFIG", "1")
        .PRIVATE_COMPILE_DEFINITION("PLATFORM_COMPILER_OPTIMIZATION_LTCG", "0")
        .PRIVATE_COMPILE_DEFINITION("PLATFORM_COMPILER_OPTIMIZATION_PG", "0")
        .PRIVATE_COMPILE_DEFINITION("PLATFORM_COMPILER_OPTIMIZATION_PG_PROFILING", "0")
        .PRIVATE_COMPILE_DEFINITION("UE_DEFINE_LEGACY_MATH_CONSTANT_MACRO_NAMES", "0");

    core.getSourceTarget()
        /*        .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/ASync", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/AutoRTFM", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Compression", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Containers", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Delegates", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Experimental", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Features", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/FileCache", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/FramePro", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/GenericPlatform", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/HAL", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Hash", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Internationalization", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/IO", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Logging", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Math", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Memory", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/MemPro", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Microsoft", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Misc", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Modules", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/ProfilingDebugging", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Serialization", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Stats", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/String", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Tasks", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Templates", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/Tests", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/ThirdParty", ".*cpp")
                .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/UObject", ".*cpp")*/
        .R_MODULE_DIRECTORIES_RG(runtimePath + "/Core/Private/UObject", ".*cpp")
        .PRIVATE_COMPILER_FLAGS(
            R"(/FI"C:\Users\hassan\UE5\Engine\Intermediate\Build\Win64\x64\UnrealEditor\Development\Core\Definitions.h" )");

    core.getSourceTarget()
        .PRIVATE_INCLUDES(runtimePath + "/Core/Private")
        .PUBLIC_INCLUDES(runtimePath + "/Core/Public");

    core.PUBLIC_LIBRARIES(&traceLog);

    addPublicPathsFrom(traceLog, core, false);
    addPublicPathsFrom(core, derivedDataCache, true);
    addPublicPathsFrom(core, targetPlatform, true);
    addPublicPathsFrom(core, directoryWatcher, true);

    // Core.Build.cs: 58
}

MAIN_FUNCTION
