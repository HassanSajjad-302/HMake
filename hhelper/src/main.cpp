
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

struct Version {
  int majorVersion = 0;
  int minorVersion = 0;
  int patchVersion = 0;
};

namespace fs = std::filesystem;
struct Compiler {
  CompilerFamily compilerFamily;
  fs::path path;
  Version version;
};

struct Linker {
  LinkerFamily linkerFamily;
  fs::path path;
  Version version;
};

typedef nlohmann::ordered_json Json;
void to_json(Json &j, const Version &version) {
  j = std::to_string(version.majorVersion) + "." + std::to_string(version.minorVersion) + "."
      + std::to_string(version.patchVersion);
}

void to_json(Json &j, const Compiler &compiler) {
  if (compiler.compilerFamily == CompilerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (compiler.compilerFamily == CompilerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = compiler.path.string();
  j["VERSION"] = compiler.version;
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
  j["VERSION"] = p.version;
}

#define THROW false
#ifndef HCONFIGURE_HEADER
#define THROW true
#endif
#ifndef JSON_HEADER
#define THROW true
#endif
#ifndef HCONFIGURE_STATIC_LIB_PATH
#define THROW true
#endif

int main() {
  if (THROW) {
    throw std::runtime_error("Macroh Required for hhelper are not provided.");
  }
  int count = 0;
  fs::path cacheFilePath;
  fs::path cp = fs::current_path();
  for (const auto &i : fs::directory_iterator(cp)) {
    if (i.is_regular_file() && i.path().filename() == "cache.hmake") {
      cacheFilePath = i.path();
      ++count;
    }
  }
  if (count > 1) {
    throw std::runtime_error("More than one file with cache.hmake name present");
  }
  if (count == 0) {
    //todo:
    //Here we will have the code that will detect the system we are on and compilers we do have installed.
    //And the location of those compilers. And their versions.
    Json j;
    std::vector<Compiler> compilersDetected;
    Version ver{10, 2, 0};
    compilersDetected.push_back(Compiler{CompilerFamily::GCC, fs::path("/usr/bin/g++"), ver});
    std::vector<Linker> linkersDetected;
    linkersDetected.push_back(Linker{LinkerFamily::GCC, fs::path("/usr/bin/g++"), ver});
    j["SOURCE_DIRECTORY"] = "../";
    j["PACKAGE_COPY"] = true;
    j["PACKAGE_COPY_PATH"] = fs::current_path() / "install" / "";
    j["CONFIGURATION"] = "RELEASE";
    j["COMPILER_ARRAY"] = compilersDetected;
    j["COMPILER_SELECTED_ARRAY_INDEX"] = 0;
    j["LINKER_ARRAY"] = linkersDetected;
    j["LINKER_SELECTED_ARRAY_INDEX"] = 0;
    j["LIBRARY_TYPE"] = "STATIC";
    j["CACHE_VARIABLES"] = Json::object();
    j["COMPILE_COMMAND"] = "g++ -std=c++20 "
                           "-I " HCONFIGURE_HEADER
                           " -I " JSON_HEADER
                           " {SOURCE_DIRECTORY}/hmake.cpp"
                           " -L " HCONFIGURE_STATIC_LIB_PATH
                           " -l hconfigure "
                           " -o "
                           "{CONFIGURE_DIRECTORY}/configure";
    std::ofstream("cache.hmake") << j.dump(4);

  } else {

    Json cacheJson;
    std::ifstream("cache.hmake") >> cacheJson;
    fs::path sourceDirPath = cacheJson.at("SOURCE_DIRECTORY").get<std::string>();
    if (sourceDirPath.is_relative()) {
      sourceDirPath = fs::absolute(sourceDirPath);
    }
    sourceDirPath = sourceDirPath.lexically_normal();
    sourceDirPath = fs::canonical(sourceDirPath);
    std::string compileCommand = cacheJson.at("COMPILE_COMMAND").get<std::string>();
    std::string srcDirString = "{SOURCE_DIRECTORY}";
    //removes the trailing slash
    std::string confDirString = "{CONFIGURE_DIRECTORY}";
    compileCommand.replace(compileCommand.find(srcDirString), srcDirString.size(), sourceDirPath);
    compileCommand.replace(compileCommand.find(confDirString), confDirString.size(), fs::current_path());
    std::cout << compileCommand << std::endl;
    int code = std::system(compileCommand.c_str());
    if (code != EXIT_SUCCESS) {
      exit(code);
    }
    std::string configureCommand = (fs::current_path() / "configure").string();
    code = std::system(configureCommand.c_str());
    if (code != EXIT_SUCCESS) {
      exit(code);
    }
  }
}
