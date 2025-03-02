
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    configuration.getCppExeDSC("app")
        .getSourceTarget().sourceDirectoriesRE(".", "file[1-4]\\.cpp|main\\.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(ConfigType::DEBUG);
    getConfiguration("Release").assign(LTO::ON); // LTO is OFF in ConfigType::RELEASE which is the default
    callConfigurationSpecification();
}

MAIN_FUNCTION