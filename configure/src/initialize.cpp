

#include "initialize.hpp"
#include "cxxopts.hpp"
#include "project.hpp"

void initialize(int argc, char const **argv) {
    auto opt = cxxopts::Options("HMake.proj", "HMake project description file");
    opt.add_options("")
            ("SOURCE_DIRECTORY","",cxxopts::value<std::string>())
            ("BUILD_DIRECTORY","",cxxopts::value<std::string>())
            ("CURRENT_SOURCE_DIRECTORY","",cxxopts::value<std::string>())
            ("CURRENT_BUILD_DIRECTORY","",cxxopts::value<std::string>());

    auto result = opt.parse(argc, argv);
    if(!result.count("SOURCE_DIRECTORY") || !result.count("BUILD_DIRECTORY")){
   // || !result.count("CURRENT_SOURCE_DIRECTORY") || !result.count("CURRENT_BUILD_DIRECTORY")){
        throw std::invalid_argument("An important directory missing. Not properly inoked by hmake");
    }
    fs::path sourceDirectoryPath = result["SOURCE_DIRECTORY"].as<std::string>();
    fs::path buildDirectoryPath = result["BUILD_DIRECTORY"].as<std::string>();
    auto normalize = [](fs::path p)->fs::path{
        if(p.is_absolute()){
            return p;
        }else{
            return (fs::current_path()/p).lexically_normal();
        }
    };
    sourceDirectoryPath = normalize(sourceDirectoryPath);
    buildDirectoryPath = normalize(buildDirectoryPath);
    project::SOURCE_DIRECTORY = directory(sourceDirectoryPath);
    project::BUILD_DIRECTORY = directory(buildDirectoryPath);
    project::CXX_COMPILER = "g++ ";
}
