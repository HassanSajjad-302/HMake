#include "Environment.hpp"
#include "JConsts.hpp"
#include "Utilities.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>
using std::ofstream, fmt::print, std::filesystem::remove;
Environment Environment::initializeEnvironmentFromVSBatchCommand(const string &command)
{
    string temporaryIncludeFilename = "temporaryInclude.txt";
    string temporaryLibFilename = "temporaryLib.txt";
    string temporaryBatchFilename = "temporaryBatch.bat";
    ofstream(temporaryBatchFilename) << "call " + command << "\necho %INCLUDE% > " + temporaryIncludeFilename
                                     << "\necho %LIB%;%LIBPATH% > " + temporaryLibFilename;

    if (int code = system(temporaryBatchFilename.c_str()); code == EXIT_FAILURE)
    {
        print(stderr, "Error in Initializing Environment\n");
        exit(EXIT_FAILURE);
    }
    remove(temporaryBatchFilename);

    auto splitPathsAndAssignToVector = [](string &accumulatedPaths) -> set<string> {
        set<string> separatedPaths{};
        unsigned long pos = accumulatedPaths.find(';');
        while (pos != std::string::npos)
        {
            std::string token = accumulatedPaths.substr(0, pos);
            if (token.empty())
            {
                break; // Only In Release Configuration in Visual Studio, for some reason the while loop also run with
                // empty string. Not investigating further for now
            }
            separatedPaths.emplace(token);
            accumulatedPaths.erase(0, pos + 1);
            pos = accumulatedPaths.find(';');
        }
        return separatedPaths;
    };

    Environment environment;
    string accumulatedPaths = file_to_string(temporaryIncludeFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    remove(temporaryIncludeFilename);
    environment.includeDirectories = splitPathsAndAssignToVector(accumulatedPaths);
    accumulatedPaths = file_to_string(temporaryLibFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    remove(temporaryLibFilename);
    environment.libraryDirectories = splitPathsAndAssignToVector(accumulatedPaths);
    environment.compilerFlags = " /EHsc /MD /nologo";
    environment.linkerFlags = " /SUBSYSTEM:CONSOLE /NOLOGO";
    return environment;
    // return Environment{};
}

Environment Environment::initializeEnvironmentOnLinux()
{
    // Maybe run cpp -v and then parse the output.
    // https://stackoverflow.com/questions/28688484/actual-default-linker-script-and-settings-gcc-uses
    return {};
}

void to_json(Json &j, const Environment &environment)
{
    j[JConsts::includeDirectories] = environment.includeDirectories;
    j[JConsts::libraryDirectories] = environment.libraryDirectories;
    j[JConsts::compilerFlags] = environment.compilerFlags;
    j[JConsts::linkerFlags] = environment.linkerFlags;
}

void from_json(const Json &j, Environment &environment)
{
    environment.includeDirectories = j.at(JConsts::includeDirectories).get<set<string>>();
    environment.libraryDirectories = j.at(JConsts::libraryDirectories).get<set<string>>();
    environment.compilerFlags = j.at(JConsts::compilerFlags).get<string>();
    environment.linkerFlags = j.at(JConsts::linkerFlags).get<string>();
}
