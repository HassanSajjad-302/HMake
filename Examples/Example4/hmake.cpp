#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease(project);

    Executable app("app", variantRelease);
    app.SOURCE_FILES("main.cpp");

    // Change the value of "FILE1" in cache.hmake to false and then run configure again.
    // Then run hbuild. Now file2.cpp will be used.
    // CacheVariable is template. So you can use any type with it. However, conversions from and to json should
    // exist for that type. See nlohmann/json for details. I guess mostly bool will be used.
    if (CacheVariable("FILE1", true).value)
    {
        app.SOURCE_FILES("file1.cpp");
    }
    else
    {
        app.SOURCE_FILES("file2.cpp");
    }

    project.configure();
    Cache::registerCacheVariables();
}