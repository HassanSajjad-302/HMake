
#include "BBuild.hpp"
#include "fstream"
#include "iostream"
using std::filesystem::current_path, std::filesystem::directory_iterator;
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
        for (auto &file : directory_iterator(current_path()))
        {
            if (file.is_regular_file())
            {
                std::string fileName = file.path().filename().string();
                if (fileName == "projectVariant.hmake" || fileName == "packageVariant.hmake")
                {
                    BVariant{file.path()};
                }
                else if (fileName == "project.hmake")
                {
                    BProject{file.path()};
                }
                else if (fileName == "cache.hmake")
                {
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
                    BTarget{file.path().string()};
                }
            }
        }
    }
}
