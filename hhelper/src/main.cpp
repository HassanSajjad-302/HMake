
#include "filesystem"
#include "fstream"
#include "nlohmann/json.hpp"
#include <iostream>

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

void jsonAssignSpecialist(const std::string &jstr, Json &j, auto &container) {
  if (container.empty()) {
    return;
  }
  if (container.size() == 1) {
    j[jstr] = *container.begin();
    return;
  }
  j[jstr] = container;
}

enum class CompilerFamily {
  ANY,
  GCC,
  MSVC,
  CLANG
};

enum class LinkerFamily {
  ANY,
  GCC,
  MSVC,
  CLANG
};

namespace fs = std::filesystem;
struct Compiler {
  CompilerFamily compilerFamily;
  fs::path path;
};

struct Linker {
  LinkerFamily linkerFamily;
  fs::path path;
};

typedef nlohmann::ordered_json Json;
void to_json(Json &j, const Compiler &compiler);
void to_json(Json &j, const Linker &linker);

void to_json(Json &j, const Compiler &compiler) {
  if (compiler.compilerFamily == CompilerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (compiler.compilerFamily == CompilerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = compiler.path.string();
}

void to_json(Json &j, const Linker &p) {
  if (p.linkerFamily == LinkerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (p.linkerFamily == LinkerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = p.path.string();
}

int main(){
  int count = 0;
  fs::path cacheFilePath;
  fs::path cp = fs::current_path();
  for(const auto& i: fs::directory_iterator(cp)){
    if(i.is_regular_file() && i.path().filename() == "cache.hmake"){
      cacheFilePath = i.path();
      ++count;
    }
  }
  if(count > 1){
    throw std::runtime_error("More than one file with cache.hmake name present");
  }
  if(count == 0) {
    //todo:
    //Here we will have the code that will detect the system we are on and compilers we do have installed.
    //And the location of those compilers.
    Json j;
    std::vector<Compiler> compilersDetected;
    compilersDetected.push_back(Compiler{CompilerFamily::GCC, fs::path("/usr/bin/g++")});
    std::vector<Linker> linkersDetected;
    linkersDetected.push_back(Linker{LinkerFamily::GCC, fs::path("/usr/bin/g++")});
    j["SOURCE_DIRECTORY"] = "../";
    j["PACKAGE_COPY"] = true;
    j["PACKAGE_COPY_PATH"] = fs::current_path() / "install" / "";
    j["CONFIGURATION"] = "RELEASE";
    j["COMPILER_ARRAY"] = compilersDetected;
    j["COMPILER_SELECTED_ARRAY_INDEX"] = 0;
    j["LINKER_ARRAY"] = linkersDetected;
    j["LINKER_SELECTED_ARRAY_INDEX"] = 0;
    j["LIBRARY_TYPE"] = "STATIC";
    j["HAS_PARENT"] = false;
    j["CACHE_VARIABLES"] = Json::object();

    std::ofstream("cache.hmake") << j.dump(4);
  }else{

    Json j;
    std::ifstream("cache.hmake") >> j;
    fs::path sourceDirPath = fs::path(std::string(j["SOURCE_DIRECTORY"]));
    fs::path filePath = sourceDirPath/fs::path("hmake.cpp");
    if (!is_regular_file(filePath)) {
      throw std::runtime_error("hmake.cpp not found. This path is  not regular file " + filePath.string());
    }

    std::string compileCommand = "g++ -std=c++20 "
                                 "-I /home/hassan/Projects/HMake/hconfigure/header/ "
                                 "-I /home/hassan/Projects/HMake/json/include/ "
        + filePath.string() + std::string(" -L /home/hassan/Projects/HMake/cmake-build-debug/ -l hconfigure ")
        + " -o " + (fs::current_path() / "configure").string();
    std::cout << compileCommand << std::endl;
    int code = system(compileCommand.c_str());
    if (code != EXIT_SUCCESS) {
      exit(code);
    }
    std::string configureCommand = (fs::current_path() / "configure").string();
    code = system(configureCommand.c_str());
    if (code != EXIT_SUCCESS) {
      exit(code);
    }
  }
}
