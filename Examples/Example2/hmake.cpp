
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
    Configuration &debug = GetConfiguration("Debug");
    debug.ASSIGN(ConfigType::DEBUG);
    Configuration &release = GetConfiguration("Release");
    release.ASSIGN(LTO::ON);

    configurationSpecification(debug);
    configurationSpecification(release);
}

MAIN_FUNCTION