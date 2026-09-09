#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    CppTarget &app = config.getCppExeDSC("app").getSourceTarget();
    app.sourceFiles("main.cpp");

    // Change FILE1=true to FILE1=false in cache.txt and run hbuild. HMake will reconfigure and use file2.cpp.
    if (CacheVariable("FILE1", true).value)
    {
        app.sourceFiles("file1.cpp");
    }
    else
    {
        app.sourceFiles("file2.cpp");
    }
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION