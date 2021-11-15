#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease;

    Executable fun("Fun", variantRelease);
    fun.sourceFiles.emplace_back("main.cpp");

    // Change the value of "FILE1" in cache.hmake to false and then run configure again.
    // Then run hbuild. Now file2.cpp will be used.
    // CacheVariable is template. So you can use any type with it. However conversions from and to json should
    // exist for that type. See nlohmann/json for details. I guess mostly bool will be used.
    if (CacheVariable("FILE1", true).value)
    {
        fun.sourceFiles.emplace_back("file1.cpp");
    }
    else
    {
        fun.sourceFiles.emplace_back("file2.cpp");
    }

    variantRelease.executables.push_back(fun);
    project.projectVariants.push_back(variantRelease);
    project.configure();
    Cache::registerCacheVariables();
}