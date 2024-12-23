#include "Configure.hpp"

using std::filesystem::file_size;

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.getCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToPublicHUDirectories();

    DSC<CppSourceTarget> &asmjit = configuration.getCppObjectDSC("asmjit").publicLibraries(&stdhu);
    asmjit.getSourceTarget()
        .publicHUIncludes("3rdparty/asmjit/src")
        .publicCompileDefinition("ASMJIT_STATIC")
        .publicCompileDefinition("ASMJIT_NO_STDCXX")
        .publicCompileDefinition("ASMJIT_NO_FOREIGN")
        .publicCompileDefinition("ASMJIT_ABI_NAMESPACE", "abi_bl")
        .rModuleDirectoriesRE("3rdparty/asmjit/src", ".*cpp");

    DSC<CppSourceTarget> &blend2d = configuration.getCppSharedDSC("blend2d").publicLibraries(&asmjit);
    blend2d.getSourceTarget()
        .rModuleDirectoriesRE("src/blend2d", ".*cpp")
        .privateHUIncludes("src/blend2d")
        .privateCompileDefinition("BL_JIT_ARCH_X86");
}

void buildSpecification()
{
    getConfiguration("debug").assign(ConfigType::DEBUG, TreatModuleAsSource::YES, AddressSanitizer::ON,
                                     TranslateInclude::YES);

    selectiveConfigurationSpecification(&configurationSpecification);
}

MAIN_FUNCTION
