#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().sourceDirsRE(".", "file[1-4]\\.cpp|main\\.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(ConfigType::DEBUG);
    getConfiguration("Release").assign(LTO::ON); // LTO is OFF in ConfigType::RELEASE which is the default
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION