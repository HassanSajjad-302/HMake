

#include "Initialize.hpp"
#include "Project.hpp"
#include "cxxopts.hpp"

void initialize(int argc, char const **argv) {
  auto opt = cxxopts::Options("HMake.proj", "HMake project description file");
  opt.add_options("")("SOURCE_DIRECTORY", "", cxxopts::value<std::string>())("BUILD_DIRECTORY", "", cxxopts::value<std::string>()) ("CURRENT_SOURCE_DIRECTORY", "", cxxopts::value<std::string>()) ("CURRENT_BUILD_DIRECTORY", "", cxxopts::value<std::string>());

  auto result = opt.parse(argc, argv);
  if (!result.count("SOURCE_DIRECTORY") || !result.count("BUILD_DIRECTORY")) {
    // || !result.count("CURRENT_SOURCE_DIRECTORY") || !result.count("CURRENT_BUILD_DIRECTORY")){
    throw std::invalid_argument("An important directory missing. Not properly inoked by hmake");
  }
  fs::path sourceDirectoryPath = result["SOURCE_DIRECTORY"].as<std::string>();
  fs::path buildDirectoryPath = result["BUILD_DIRECTORY"].as<std::string>();
  fs::absolute(sourceDirectoryPath);
  fs::absolute(buildDirectoryPath);
  sourceDirectoryPath /= fs::path();
  buildDirectoryPath /= fs::path();
  Project::SOURCE_DIRECTORY = Directory(sourceDirectoryPath);
  Project::BUILD_DIRECTORY = Directory(buildDirectoryPath);
  Compiler comp{CompilerFamily::GCC, "/usr/bin/g++"};
  Project::ourCompiler = comp;
  Linker link{LinkerFamily::GCC, "/usr/bin/g++"};
  Project::ourLinker = link;
  Project::flags[CompilerFamily::GCC][CONFIG_TYPE::DEBUG] = "-g";
  Project::flags[CompilerFamily::GCC][CONFIG_TYPE::RELEASE] = "-O3 -DNDEBUG";
  Project::libraryType = LibraryType::STATIC;
}
