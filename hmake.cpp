#include "Configure.hpp"

bool smallFile = true;
void configurationSpecification(Configuration &config)
{
    config.stdCppTarget->getSourceTarget().publicIncludesSource("3rdParty");

    DSC<CppTarget> &fmt = config.getCppObjectDSC("fmt");
    fmt.getSourceTarget().moduleDirs("3rdParty/fmt/src").publicHUDirs("3rdParty/fmt/include", "fmt/");

    DSC<CppTarget> &rapidJson = config.getCppObjectDSC("rapidjson");
    rapidJson.getSourceTarget().publicHUDirs("3rdParty/rapidjson/include", "rapidjson/");

    DSC<CppTarget> &phmap = config.getCppObjectDSC("phmap");
    phmap.getSourceTarget().publicHUDirs("3rdParty/parallel-hashmap/parallel_hashmap",
                                         "parallel-hashmap/parallel_hashmap");

    DSC<CppTarget> &hconfigure = config.getCppObjectDSC("hconfigure").publicDeps(rapidJson, phmap, fmt);
    hconfigure.getSourceTarget().moduleDirs("hconfigure/src").publicHUIncludes("hconfigure/header");

    if (smallFile)
    {
        hconfigure.getSourceTarget().publicCompileDefines("USE_COMMAND_HASH", "", "USE_NODES_CACHE_INDICES_IN_CACHE",
                                                          "", "USE_JSON_FILE_COMPRESSION", "");
    }
}

void buildSpecification()
{
    getConfiguration("debug").assign(ConfigType::DEBUG, ExceptionHandling::OFF);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION