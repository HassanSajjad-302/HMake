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