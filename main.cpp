#include <iostream>
#include <filesystem>
#include "cxxopts.hpp"
namespace fs = std::filesystem;

enum class hmakeMode{
    CONFIGURE, BUILD, FAIL
};
int main(int argc, char** argv)
{
    const std::string TARGET_FILE_EXTENSION = ".target.json5";
    const std::string FILE_NAME = "hmake.cpp";

    auto opt = cxxopts::Options("HMake", "A c++ build sytem");

    opt.add_options()("f,file","target or project file or file directory", cxxopts::value<std::string>())
            ("S", " <path-to-source> Explicitly specify a source directory.", cxxopts::value<std::string>())
            ("B", " <path-to-build> Explicitly specify a build directory.", cxxopts::value<std::string>());

    auto result = opt.parse(argc, argv);

    hmakeMode mode = hmakeMode::FAIL;
    fs::path fileName;
    //Note that following two are neither assigned nor used when hmake is run in build mode.
    fs::path sourceDirectory;
    fs::path buildDirectory;

    if(result.count("file")){
        fileName = result["file"].as<std::string>();
        fileName = fileName.lexically_normal();
        if(!fs::exists(fileName)){
            throw std::invalid_argument("File " + fileName.string() + " Not Found");
        }
        if(fileName == FILE_NAME){
            mode = hmakeMode::CONFIGURE;
            sourceDirectory = fileName.parent_path();
            if(sourceDirectory == ""){
                sourceDirectory = fs::current_path();
            }
            if(!is_directory(sourceDirectory)){
                throw std::runtime_error("Could not determine the source directory from the file provided. parent"
                                         "path resolved to" + sourceDirectory.string());
            }
        }else if(fileName.string().ends_with(TARGET_FILE_EXTENSION)){
            mode = hmakeMode::BUILD;
        }
    }else if(result.count("S")){
        sourceDirectory = result["S"].as<std::string>();
        sourceDirectory = sourceDirectory.lexically_normal();
        if(!fs::exists(sourceDirectory)){
            throw std::runtime_error("Source Directory " + sourceDirectory.string() + " does not exists");
        }
        if(!is_directory(sourceDirectory)){
            throw std::runtime_error("Source Directory " + sourceDirectory.string() + " is not directory");
        }
        int count = 0;
        fs::path ourFilePath;
        for (const auto & entry : fs::directory_iterator(sourceDirectory))
        {
            if(entry.path().string().ends_with(TARGET_FILE_EXTENSION) || entry.path().filename() == FILE_NAME){
                ++count;
                ourFilePath = entry;
            }
        }
        if(count == 1){
            fileName = ourFilePath;
            if(ourFilePath.filename() == FILE_NAME){
                mode = hmakeMode::CONFIGURE;
            }else if(ourFilePath.filename().string().ends_with(TARGET_FILE_EXTENSION)){
                mode = hmakeMode::BUILD;
            }
        }
    }
    if(mode == hmakeMode::FAIL){
        throw std::runtime_error("Could not determine which mode should hmake run. hmake not invoked properly.");
    }
    //At this point, we have our hmake file and source directory
    if(result.count("B")){
        if(mode == hmakeMode::BUILD){
            throw std::runtime_error("Build directory should not had been provided in build mode");
        }
        buildDirectory = result["B"].as<std::string>();
        buildDirectory.lexically_normal();
        if(!fs::exists(buildDirectory)){
            throw std::runtime_error("Source Directory" + sourceDirectory.string() + " does not exists");
        }
        if(!is_directory(buildDirectory)){
            throw std::runtime_error("Source Directory" + sourceDirectory.string() + " is not directory");
        }
    }

    //So, at this point we have the mode in which hmake is to run, we have source directory and build directory in case
    //hmake mode is configure.
    if(mode == hmakeMode::CONFIGURE){
        std::cout << "Success " << std::endl;
        std::string compileCommand = "g++ -std=c++20 -I/home/hassan/Projects/HMake/configure/header "
                                     + std::string("-L/home/hassan/Projects/HMake/cmake-build-debug/ -lconfigure ")
                                     + fileName.string() + " -o app";
        int code = system(compileCommand.c_str());
        exit(EXIT_SUCCESS);
    }else{

    }
}