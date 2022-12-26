
#include "Configure.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>

using std::string, std::vector, std::ifstream, std::ofstream, fmt::print, std::endl, std::filesystem::path,
    std::filesystem::current_path, std::filesystem::directory_iterator, std::to_string, std::runtime_error,
    std::filesystem::canonical;
using Json = nlohmann::ordered_json;

void jsonAssignSpecialist(const string &jstr, Json &j, auto &container)
{
    if (container.empty())
    {
        return;
    }
    if (container.size() == 1)
    {
        j[jstr] = *container.begin();
        return;
    }
    j[jstr] = container;
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

#ifdef _WIN32
constexpr Platform platform = Platform::WINDOWS;
#elif defined __linux
constexpr Platform platform = Platform::LINUX;
#else
#define THROW 1
#endif

int main()
{
    if (THROW)
    {
        print(stderr, "Macros Required for hhelper are not provided.\n");
        exit(EXIT_FAILURE);
    }
    int count = 0;
    path cacheFilePath;
    path cp = current_path();
    for (const auto &i : directory_iterator(cp))
    {
        if (i.is_regular_file() && i.path().filename() == "cache.hmake")
        {
            cacheFilePath = i.path();
            ++count;
        }
    }
    if (count > 1)
    {
        print(stderr, "More than one file with cache.hmake name present\n");
        exit(EXIT_FAILURE);
    }
    if (count == 0)
    {
        // todo:
        // Here we will have the code that will detect the system we are on and compilers we do have installed.
        // And the location of those compilers. And their versions.
        Json j;
        vector<Compiler> compilersDetected;
        vector<Linker> linkersDetected;
        vector<Archiver> archiversDetected;

        if constexpr (platform == Platform::LINUX)
        {
            Version ver{10, 2, 0};
            compilersDetected.push_back(Compiler{BTFamily::GCC, ver, path("/usr/bin/g++")});
            linkersDetected.push_back(Linker{
                BTFamily::GCC,
                ver,
                path("/usr/bin/g++"),
            });
            archiversDetected.push_back(Archiver{BTFamily::GCC, ver, "/usr/bin/ar"});
        }
        else
        {
            Version ver{19, 30, 30705};
            compilersDetected.push_back(Compiler{
                BTFamily::MSVC, ver,
                R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64\cl.exe)"});
            linkersDetected.push_back(Linker{
                BTFamily::MSVC, ver,
                R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64\link.exe)"});
            archiversDetected.push_back(Archiver{
                BTFamily::MSVC, ver,
                R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64\lib.exe)"});
        }

        j[JConsts::sourceDirectory] = "../";
        j[JConsts::packageCopy] = true;
        j[JConsts::packageCopyPath] = current_path().lexically_normal().generic_string() + "/install/";
        j[JConsts::configuration] = JConsts::release;
        j[JConsts::compilerArray] = compilersDetected;
        j[JConsts::compilerSelectedArrayIndex] = 0;
        j[JConsts::linkerArray] = linkersDetected;
        j[JConsts::linkerSelectedArrayIndex] = 0;
        j[JConsts::archiverArray] = archiversDetected;
        j[JConsts::archiverSelectedArrayIndex] = 0;
        j[JConsts::libraryType] = JConsts::static_;

        j[JConsts::cacheVariables] = Json::object();

        vector<string> compileConfigureCommands;

        path hconfigureHeaderPath = path(HCONFIGURE_HEADER);
        path jsonHeaderPath = path(JSON_HEADER);
        path fmtHeaderPath = path(FMT_HEADER);
        path hconfigureStaticLibDirectoryPath = path(HCONFIGURE_STATIC_LIB_DIRECTORY);
        path fmtStaticLibDirectoryPath = path(FMT_STATIC_LIB_DIRECTORY);
        path hconfigureStaticLibPath = path(HCONFIGURE_STATIC_LIB_PATH);
        path fmtStaticLibPath = path(FMT_STATIC_LIB_PATH);
        if constexpr (platform == Platform::LINUX)
        {
            string compileCommand =
                "g++ -std=c++20"
                " -I " HCONFIGURE_HEADER " -I " JSON_HEADER "  -I " FMT_HEADER " {SOURCE_DIRECTORY}/hmake.cpp"
                " -L " HCONFIGURE_STATIC_LIB_DIRECTORY " -l hconfigure -L " FMT_STATIC_LIB_DIRECTORY " -l fmt "
                " -o "
                "{CONFIGURE_DIRECTORY}/configure";
            compileConfigureCommands.push_back(compileCommand);
        }
        else
        {
            Environment environment = Environment::initializeEnvironmentFromVSBatchCommand(
                R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64)");

            string compileCommand =
                R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64\cl.exe")";

            for (const auto &dir : environment.includeDirectories)
            {
                compileCommand += " /I " + addQuotes(dir.directoryPath.generic_string());
            }
            compileCommand += " /I " + hconfigureHeaderPath.string() + " /I " + jsonHeaderPath.string() + " /I " +
                              fmtHeaderPath.string() + " /std:c++latest" + environment.compilerFlags +
                              " {SOURCE_DIRECTORY}/hmake.cpp"
                              " /link " +
                              environment.linkerFlags;
            for (const auto &dir : environment.libraryDirectories)
            {
                compileCommand += "/LIBPATH:" + addQuotes(dir.directoryPath.generic_string()) + " ";
            }
            compileCommand += addQuotes(hconfigureStaticLibPath.string()) + " " + addQuotes(fmtStaticLibPath.string()) +
                              " /OUT:{CONFIGURE_DIRECTORY}/configure.exe";
            compileCommand = addQuotes(compileCommand);
            compileConfigureCommands.push_back(compileCommand);
        }

        j[JConsts::compileConfigureCommands] = compileConfigureCommands;
        ofstream("cache.hmake") << j.dump(4);
    }
    else
    {

        Json cacheJson;
        ifstream("cache.hmake") >> cacheJson;
        path sourceDirPath = cacheJson.at(JConsts::sourceDirectory).get<string>();
        if (sourceDirPath.is_relative())
        {
            sourceDirPath = absolute(sourceDirPath);
        }
        sourceDirPath = sourceDirPath.lexically_normal();
        sourceDirPath = canonical(sourceDirPath);

        string srcDirString = "{SOURCE_DIRECTORY}";
        string confDirString = "{CONFIGURE_DIRECTORY}";

        vector<string> compileConfigureCommands = cacheJson.at(JConsts::compileConfigureCommands).get<vector<string>>();

        if (!compileConfigureCommands.empty())
        {
            print("Executing commands as specified in cache.hmake to produce configure executable\n");
        }

        for (string &compileConfigureCommand : compileConfigureCommands)
        {
            if (const size_t position = compileConfigureCommand.find(srcDirString); position != string::npos)
            {
                compileConfigureCommand.replace(position, srcDirString.size(), sourceDirPath.string());
            }
            if (const size_t position = compileConfigureCommand.find(confDirString); position != string::npos)
            {
                compileConfigureCommand.replace(position, confDirString.size(), current_path().string());
            }
            print("{}\n", compileConfigureCommand);
            int code = system(compileConfigureCommand.c_str());
            if (code != EXIT_SUCCESS)
            {
                exit(code);
            }
        }

        string configureCommand = (current_path() / "configure").string();
        int code = system(configureCommand.c_str());
        if (code != EXIT_SUCCESS)
        {
            exit(code);
        }
    }
}
