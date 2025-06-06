#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppStaticDSC("app")
        .getSourceTarget()
        .moduleFiles( "B.cpp")
        .privateHUDirsBigHu(srcNode->filePath, "BigHu.hpp", "BigHu");
}

void buildSpecification()
{
    getConfiguration().assign(TranslateInclude::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION