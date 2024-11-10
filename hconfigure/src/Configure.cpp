#include "Configure.hpp"

bool selectiveConfigurationSpecification(void (*ptr)(Configuration &configuration))
{
    if (equivalent(path(configureNode->filePath), std::filesystem::current_path()))
    {
        for (const Configuration &configuration : targets<Configuration>)
        {
            (*ptr)(const_cast<Configuration &>(configuration));
        }
        return true;
    }
    for (const Configuration &configuration : targets<Configuration>)
    {
        if (const_cast<Configuration &>(configuration).getSelectiveBuildChildDir())
        {
            (*ptr)(const_cast<Configuration &>(configuration));
        }
    }
    return false;
}

int main2(int argc, char **argv)
{
    try
    {
        setBuildSystemModeFromArguments(argc, argv);
        constructGlobals();
        initializeCache(bsMode);
        (*buildSpecificationFuncPtr)();
        const bool errorHappened = configureOrBuild();
        destructGlobals();
        if (errorHappened)
        {
            exit(EXIT_FAILURE);
        }
    }
    catch (std::exception &ec)
    {
        if (const string str(ec.what()); !str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
