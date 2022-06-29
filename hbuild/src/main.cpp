
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

        mutex m;
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
            Builder{Builder::getTargetFilePathsFromProjectOrPackageFile("project.hmake", false), m};
        }
        else if (directoryFilesContains("projectVariant.hmake"))
        {
            initializeSettings("../settings.hmake");
            Builder{Builder::getTargetFilePathsFromVariantFile("projectVariant.hmake"), m};
        }
        else if (directoryFilesContains("package.hmake"))
        {
            initializeSettings("../settings.hmake");
            Builder{Builder::getTargetFilePathsFromProjectOrPackageFile("package.hmake", true), m};
            Builder::copyPackage("package.hmake");
        }
        else if (directoryFilesContains("packageVariant.hmake"))
        {
            initializeSettings("../../settings.hmake");
            Builder{Builder::getTargetFilePathsFromVariantFile("packageVariant.hmake"), m};
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
                    Builder{vec, m};
                    break;
                }
            }
        }

        /*if (file.is_regular_file())
        {
            std::string fileName = file.path().filename().string();
            if (fileName == "projectVariant.hmake" || fileName == "packageVariant.hmake")
            {
                BVariant{file.path(), m};
            }
            else if (fileName == "project.hmake")
            {
                BProject{file.path()};
            }
            else if (fileName == "cache.hmake")
            {
                for (auto &cacheDirectoryIterator : directory_iterator(current_path()))
                {
                    if (cacheDirectoryIterator.is_directory())
                    {
                        for (auto &maybeVariantFile : directory_iterator(cacheDirectoryIterator))
                        {
                            if (maybeVariantFile.is_regular_file())
                            {
                                std::string fileName = maybeVariantFile.path().filename().string();
                                if (fileName == "projectVariant.hmake" || fileName == "packageVariant.hmake")
                                {
                                    BVariant{maybeVariantFile.path(), m};
                                }
                            }
                        }
                    }
                }
            }
            else if (fileName == "package.hmake")
            {
                BPackage{file.path()};
            }
            else if (fileName == "Common.hmake")
            {
                BPackage{canonical(file.path()).parent_path() / "package.hmake"};
            }
            else if (fileName.ends_with(".hmake"))
            {
                vector<string> target{file.path().string()};
                Builder{target, m};
            }
        }*/
    }
}
