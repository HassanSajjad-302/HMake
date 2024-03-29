
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    configuration.GetCppExeDSC("app")
        .getSourceTarget()
        .SOURCE_DIRECTORIES_RG(".", "file[1-4]\\.cpp|main\\.cpp")
        .SINGLE(LTO::ON, Optimization::SPACE);
}

void buildSpecification()
{
    if (CacheVariable("Debug", true).value)
    {
        Configuration &debug = GetConfiguration("Debug");
        debug.ASSIGN(ConfigType::DEBUG);
        configurationSpecification(debug);
    }
    if (CacheVariable("Release", false).value)
    {
        Configuration &release = GetConfiguration("Release");
        release.ASSIGN(LTO::ON);
        configurationSpecification(release);
    }
}

MAIN_FUNCTION