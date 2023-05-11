
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    configuration.GetCppExeDSC("app").getSourceTarget().SOURCE_DIRECTORIES_RG(".", "file[1-4]\\.cpp|main\\.cpp");
}

void buildSpecification()
{
    auto custom = targets<CTarget>.emplace("Custom");
    configurationSpecification(GetConfiguration("Debug"));
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(BSMode::IDE);
        buildSpecification();
        configureOrBuild();
        auto *a = getCTargetContainer(BSMode::IDE);
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