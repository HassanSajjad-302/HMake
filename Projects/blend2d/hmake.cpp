#include "Configure.hpp"

using std::filesystem::file_size;

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToPublicHUDirectories();

    DSC<CppSourceTarget> &asmjit = configuration.GetCppObjectDSC("asmjit").PUBLIC_LIBRARIES(&stdhu);
    asmjit.getSourceTarget()
        .PUBLIC_HU_INCLUDES("3rdparty/asmjit/src")
        .PUBLIC_COMPILE_DEFINITION("ASMJIT_STATIC")
        .PUBLIC_COMPILE_DEFINITION("ASMJIT_NO_STDCXX")
        .PUBLIC_COMPILE_DEFINITION("ASMJIT_NO_FOREIGN")
        .PUBLIC_COMPILE_DEFINITION("ASMJIT_ABI_NAMESPACE", "abi_bl")
        .R_MODULE_DIRECTORIES_RG("3rdparty/asmjit/src", ".*cpp");

    DSC<CppSourceTarget> &blend2d = configuration.GetCppSharedDSC("blend2d").PUBLIC_LIBRARIES(&asmjit);
    blend2d.getSourceTarget()
        .R_MODULE_DIRECTORIES_RG("src/blend2d", ".*cpp")
        .PRIVATE_HU_INCLUDES("src/blend2d")
        .PRIVATE_COMPILE_DEFINITION("BL_JIT_ARCH_X86");
}

void buildSpecification()
{
    GetConfiguration("debug").ASSIGN(ConfigType::DEBUG, TreatModuleAsSource::YES, TranslateInclude::YES);

    selectiveConfigurationSpecification(&configurationSpecification);
}

MAIN_FUNCTION
