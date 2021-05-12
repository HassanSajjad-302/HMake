#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;
int main()
{
    const std::string TARGET_FILE_EXTENSION = ".target.json5";
    const std::string FILE_EXTENSION = "hmake.cpp";
    int count = 0;
    fs::path ourFilePath;
    for (const auto & entry : fs::directory_iterator(fs::current_path()))
    {
        if(entry.path().string().ends_with(TARGET_FILE_EXTENSION) || entry.path().filename() == FILE_EXTENSION){
            ++count;
            ourFilePath = entry;
        }
    }
    if(count != 1){
        exit(-1);
    }
    if(ourFilePath.filename() == FILE_EXTENSION){
        std::cout << ourFilePath.string() << std::endl;
        std::string compileCommand = "g++ -std=c++20 -I/home/hassan/Projects/HMake/configure/header "
                + std::string("-L/home/hassan/Projects/HMake/cmake-build-debug/ -lconfigure ")
                + ourFilePath.string() + " -o app";
        int code = system(compileCommand.c_str());
        exit(code);
    }else{

    }
}