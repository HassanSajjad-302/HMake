#include "Configure.hpp"
#include "HashValues.hpp"
using std::filesystem::current_path;

bool selectiveConfigurationSpecification(void (*ptr)(Configuration &configuration))
{
    if (equivalent(path(configureNode->filePath), current_path()))
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

static void parseCmdArgumentsAndSetConfigureNode(const int argc, char **argv)
{
    if (argc > 1)
    {
        if (const pstring_view str(argv[1]); str == "--build")
        {
            bsMode = BSMode::BUILD;
        }
        else
        {
            printErrorMessage("Unknown cmd arguments in configure mode\n");
            for (int i = 1; i < argc; ++i)
            {
                printErrorMessage(argv[i]);
            }
        }
    }

    pstring configurePathString;
    if (bsMode != BSMode::CONFIGURE)
    {
        path configurePath;
        bool configureExists = false;
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            configurePath = p / getActualNameFromTargetName(TargetType::EXECUTABLE, os, "configure");
            if (exists(configurePath))
            {
                configureExists = true;
                break;
            }
        }

        if (configureExists)
        {
            configurePathString = (configurePath.parent_path().*toPStr)();
        }
        else
        {
            throw std::exception("Configure could not be found in current directory and directories above\n");
        }
    }
    else
    {
        configurePathString = (current_path().*toPStr)();
    }

    lowerCasePStringOnWindows(configurePathString.data(), configurePathString.size());
    configureNode = Node::getNodeFromNormalizedString(configurePathString, false);

    if (bsMode == BSMode::BUILD)
    {
        for (int i = 2; i < argc; ++i)
        {
            pstring targetArgFullPath = (current_path() / argv[i]).lexically_normal().string();
            lowerCasePStringOnWindows(targetArgFullPath.data(), targetArgFullPath.size());
            if (targetArgFullPath.size() <= configureNode->filePath.size())
            {
                throw std::invalid_argument(fmt::format("Invalid Command-Line Argument {}\n", argv[i]));
            }
            if (targetArgFullPath.ends_with(slashc))
            {
                cmdTargets.emplace(targetArgFullPath.begin() + configureNode->filePath.size() + 1,
                                   targetArgFullPath.end() - 1);
            }
            else
            {
                cmdTargets.emplace(targetArgFullPath.begin() + configureNode->filePath.size() + 1,
                                   targetArgFullPath.end());
            }
        }
    }
}

int main2(const int argc, char **argv)
{
    try
    {
        parseCmdArgumentsAndSetConfigureNode(argc, argv);
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
