#include "Configure.hpp"

void buildSpecification()
{
    CppSourceTarget &app = GetCppExeDSC("app").getSourceTarget();
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
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        exportAllSymbolsAndInitializeGlobals();
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif