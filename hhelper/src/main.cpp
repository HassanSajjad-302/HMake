
#include "filesystem"
#include "fstream"
#include "nlohmann/json.hpp"
#include "rang.hpp"
#include <Windows.h>
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

enum class Platform {
    WINDOWS,
    LINUX
};

#ifdef _WIN64 || _WIN32
constexpr Platform platform = Platform::WINDOWS;
#elif defined __linux
constexpr Platform platform = Platform::LINUX;
#else
#define THROW 1
#endif

int main() {

    std::cout << rang::fg::red;
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
    std::vector<Linker> linkersDetected;

    if constexpr (platform == Platform::WINDOWS) {
        Version ver{10, 2, 0};
        compilersDetected.push_back(Compiler{CompilerFamily::GCC, fs::path("/usr/bin/g++"), ver});
        linkersDetected.push_back(Linker{LinkerFamily::GCC, fs::path("/usr/bin/g++"), ver});
    }
    else {
        Version ver{ 19,30,30705 };
        compilersDetected.push_back(Compiler{ CompilerFamily::MSVC, fs::path("\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.30.30705\\bin\\Hostx64\\x64\\cl.exe\""), ver});
    }
   
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

    std::vector<std::string> compileCommands;
    std::vector<std::string> linkCommands;

    if constexpr (platform == Platform::LINUX) {
        std::string compileCommand = "g++ -std=c++20"
            " -I " HCONFIGURE_HEADER
            " -I " JSON_HEADER
            " {SOURCE_DIRECTORY}/hmake.cpp"
            " -L " HCONFIGURE_STATIC_LIB_DIRECTORY
            " -l hconfigure "
            " -o "
            "{CONFIGURE_DIRECTORY}/configure";
        compileCommands.push_back(compileCommand);
    }
    else {
        compileCommands.push_back(R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" x86_amd64)");
        std::string compileCommand = "\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.30.30705\\bin\\Hostx64\\x64\\cl.exe\""
            " /I " HCONFIGURE_HEADER
            " /I " JSON_HEADER
            " /std:c++latest"
            " /EHs /MD {SOURCE_DIRECTORY}/hmake.cpp"
            " /link " HCONFIGURE_STATIC_LIB_PATH
            " /OUT:{CONFIGURE_DIRECTORY}/configure.exe";
        compileCommands.push_back(compileCommand);
    }


    j["COMPILE_COMMANDS"] = compileCommands;
    j["LINK_COMMANDS"] = linkCommands;
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

    std::string srcDirString = "{SOURCE_DIRECTORY}";
    std::string confDirString = "{CONFIGURE_DIRECTORY}";

    auto replaceSourceAndConfigureDirectoryPlaceholders = [&](std::string& command) {
        if (const int position = command.find(srcDirString); position != std::string::npos) {
            command.replace(position, srcDirString.size(), sourceDirPath.string());
        }
        if (const int position = command.find(confDirString); position != std::string::npos) {
            command.replace(position, confDirString.size(), fs::current_path().string());
        }
    };

    std::vector<std::string> compileCommands = cacheJson.at("COMPILE_COMMANDS").get<std::vector<std::string>>();
    std::vector<std::string> linkCommands = cacheJson.at("LINK_COMMANDS").get<std::vector<std::string>>();

    if (!compileCommands.empty()) {
        std::cout << "Executing compile commands as specified in cache.hmake to produce configure executable" << std::endl;
    }

    for (std::string& compileCommand : compileCommands) {
        replaceSourceAndConfigureDirectoryPlaceholders(compileCommand);
        std::cout << compileCommand << std::endl << rang::style::reset;
        int code = std::system(compileCommand.c_str());
        if (code != EXIT_SUCCESS) {
            exit(code);
        }
        std::cout << rang::fg::red;
    }

    if (!linkCommands.empty()) {
        std::cout << "Executing link commands as specified in cache.hmake to produce configure executable" << std::endl;
    }

    for (std::string& linkCommand : linkCommands) {
        replaceSourceAndConfigureDirectoryPlaceholders(linkCommand);
        std::cout << linkCommand << std::endl << rang::style::reset;
        int code = std::system(linkCommand.c_str());
        if (code != EXIT_SUCCESS) {
            exit(code);
        }
        std::cout << rang::fg::red;
    }
   
    std::string configureCommand = (fs::current_path() / "configure").string();
    int code = std::system(configureCommand.c_str());
    if (code != EXIT_SUCCESS) {
      exit(code);
    }
  }
  std::cout << rang::fg::reset;
}
