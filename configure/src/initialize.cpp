

#include "initialize.hpp"
#include "cxxopts.hpp"
#include "project.hpp"

void initialize(int argc, char const **argv) {
    auto opt = cxxopts::Options("HMake.proj", "HMake project description file");
    opt.add_options("")
    ("SOURCE_DIRECTORY","")
            ("BUILD_DIRECTORY","")
            ("CURRENT_SOURCE_DIRECTORY","")
            ("CURRENT_BUILD_DIRECTORY","");

    auto result = opt.parse(argc, argv);
    if(!result.count("SOURCE_DIRECTORY") || !result.count("BUILD_DIRECTORY") ||
    !result.count("CURRENT_SOURCE_DIRECTORY") || !result.count("CURRENT_BUILD_DIRECTORY")){
        throw std::invalid_argument("An important directory missing. Not properly inoked by hmake");
    }
    project::SOURCE_DIRECTORY = directory(result["SOURCE_DIRECTORY"].as<std::string>());
    project::BUILD_DIRECTORY = directory(result["BUILD_DIRECTORY"].as<std::string>());
}
