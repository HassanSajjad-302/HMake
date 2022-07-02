
#include "BBuild.hpp"
#include "fstream"
#include "iostream"
using std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream;
int main(int argc, char **argv)
{
    if (argc == 2)
    {
        std::string filePath = argv[1];
        path buildFilePath = filePath;
        if (buildFilePath.is_relative())
        {
            buildFilePath = (current_path() / buildFilePath).lexically_normal();
        }
        else
        {
            buildFilePath = buildFilePath.lexically_normal();
        }
        if (!is_regular_file(buildFilePath))
        {
            std::cerr << buildFilePath.string() << " is not regular file";
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        auto initializeSettings = [](const path &settingsFilePath) {
            Json outputSettingsJson;
            ifstream(settingsFilePath) >> outputSettingsJson;
            settings = outputSettingsJson;
        };

        vector<string> directoryFiles;
        for (auto &file : directory_iterator(current_path()))
        {
            if (file.is_regular_file())
            {
                directoryFiles.emplace_back(file.path().filename().string());
            }
        }
        auto directoryFilesContains = [&](const string &str) -> bool {
            return find(directoryFiles.begin(), directoryFiles.end(), str) != directoryFiles.end();
        };

        if (directoryFilesContains("project.hmake"))
        {
            initializeSettings("settings.hmake");
            Builder{Builder::getTargetFilePathsFromProjectOrPackageFile("project.hmake", false)};
        }
        else if (directoryFilesContains("projectVariant.hmake"))
        {
            initializeSettings("../settings.hmake");
            Builder{Builder::getTargetFilePathsFromVariantFile("projectVariant.hmake")};
        }
        else if (directoryFilesContains("package.hmake"))
        {
            initializeSettings("../settings.hmake");
            Builder{Builder::getTargetFilePathsFromProjectOrPackageFile("package.hmake", true)};
            Builder::copyPackage("package.hmake");
        }
        else if (directoryFilesContains("packageVariant.hmake"))
        {
            initializeSettings("../../settings.hmake");
            Builder{Builder::getTargetFilePathsFromVariantFile("packageVariant.hmake")};
        }
        else
        {
            for (const auto &i : directoryFiles)
            {
                if (i.ends_with(".hmake"))
                {
                    Json outputSettingsJson;
                    ifstream("../../settings.hmake") >> outputSettingsJson;
                    settings = outputSettingsJson;
                    set<string> vec;
                    vec.emplace(i);
                    Builder{vec};
                    break;
                }
            }
        }
    }
}
