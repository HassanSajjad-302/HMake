
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    configuration.getCppExeDSC("app")
        .getSourceTarget()
        .sourceDirectoriesRE(".", "file[1-4]\\.cpp|main\\.cpp");
       // .SINGLE(LTO::ON, Optimization::SPACE);
}

void buildSpecification()
{
    Configuration &debug = getConfiguration("Debug");
    debug.assign(ConfigType::DEBUG);
    Configuration &release = getConfiguration("Release");
    release.assign(LTO::ON);

    callConfigurationSpecification();
}

MAIN_FUNCTION