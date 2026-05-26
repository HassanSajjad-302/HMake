#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Cache.hpp"
#include "JConsts.hpp"
#include "Node.hpp"
#include "RunCommand.hpp"
#include "ToolsCache.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using std::string, std::vector, std::ifstream, std::ofstream, std::endl, std::filesystem::path,
    std::filesystem::current_path, std::filesystem::directory_iterator, std::to_string, std::runtime_error,
    std::filesystem::canonical;

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::duration<double>;

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

// https://stackoverflow.com/a/17620909/8993136
void replaceAll(string &str, const string &from, const string &to)
{
    if (from.empty())
    {
        return;
    }
    string wsRet;
    wsRet.reserve(str.length());
    size_t start_pos = 0, pos;
    while ((pos = str.find(from, start_pos)) != string::npos)
    {
        wsRet += str.substr(start_pos, pos - start_pos);
        wsRet += to;
        pos += from.length();
        start_pos = pos;
    }
    wsRet += str.substr(start_pos);
    str.swap(wsRet);
}

static string formatSeconds(Seconds s)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3fs", s.count());
    return buf;
}

#define THROW false
#ifndef HCONFIGURE_HEADER
#define THROW true
#endif
#ifndef JSON_HEADER
#define THROW true
#endif
#ifndef HCONFIGURE_C_STATIC_LIB_PATH
#define THROW true
#endif
#ifndef HCONFIGURE_B_STATIC_LIB_PATH
#define THROW true
#endif
#ifndef THIRD_PARTY_HEADER
#define THROW true
#endif
#ifndef LZ4_HEADER
#define THROW true
#endif

int main(int argc, char **argv)
{
    const auto totalStart = Clock::now();

    constructGlobals();

    if (THROW)
    {
        printErrorMessage("Macros Required for hhelper are not provided.\n");
        exit(EXIT_FAILURE);
    }

    bool onlyConfigure = false;
    if (argc == 2)
    {
        if (string argument(argv[1]); argument == "--configure")
        {
            onlyConfigure = true;
        }
        else
        {
            printErrorMessage("Unknown Argument provided to hhelper.\n");
        }
    }

    if (!std::filesystem::exists("cache.json"))
    {
        path hconfigureHeaderPath = path(HCONFIGURE_HEADER);
        path jsonHeaderPath = path(JSON_HEADER);
        path rapidjsonHeaderPath = path(RAPIDJSON_HEADER);
        path thirdPartyHeaderPath = path(THIRD_PARTY_HEADER);
        path lz4Header = path(LZ4_HEADER);
        path hconfigureCStaticLibDirectoryPath = path(HCONFIGURE_C_STATIC_LIB_DIRECTORY);
        path hconfigureBStaticLibDirectoryPath = path(HCONFIGURE_B_STATIC_LIB_DIRECTORY);
        path hconfigureCStaticLibPath = path(HCONFIGURE_C_STATIC_LIB_PATH);
        path hconfigureBStaticLibPath = path(HCONFIGURE_B_STATIC_LIB_PATH);

        if constexpr (os == OS::LINUX)
        {
#ifdef USE_FILE_COMPRESSION
            string useJsonFileCompressionDef = " -D USE_FILE_COMPRESSION ";
#else
            string useJsonFileCompressionDef = "";
#endif
#ifdef USE_THREAD_SANITIZER
            string tsan = "-fsanitize=thread -fno-omit-frame-pointer ";
#else
            string tsan = "";
#endif
            auto getCommand = [&](const bool configureExe) {
                string compileCommand =
                    "c++ -std=c++2b -O0 -fno-exceptions -fno-rtti -fvisibility=hidden "
                    "-ffunction-sections -fdata-sections " +
                    tsan + useJsonFileCompressionDef + string(configureExe ? "" : " -D BUILD_MODE -D NDEBUG ") +
                    " -I " HCONFIGURE_HEADER "  -I " THIRD_PARTY_HEADER " -I " JSON_HEADER " -I " RAPIDJSON_HEADER
                    " -I " LZ4_HEADER " {SOURCE_DIRECTORY}/hmake.cpp -Wl,--gc-sections -Wl,--whole-archive "
                    "-L " HCONFIGURE_C_STATIC_LIB_DIRECTORY " -l" +
                    string(configureExe ? "hconfigure-c" : "hconfigure-b") +
                    " -Wl,--no-whole-archive -o {CONFIGURE_DIRECTORY}/" +
                    getActualNameFromTargetName(TargetType::EXECUTABLE, os, configureExe ? "configure" : "build");
                return compileCommand;
            };
            cache.configureExeBuildScript = getCommand(true);
            cache.buildExeBuildScript = getCommand(false);
        }
        else
        {
#ifdef USE_FILE_COMPRESSION
            string useJsonFileCompressionDef = " /D USE_FILE_COMPRESSION ";
#else
            string useJsonFileCompressionDef = "";
#endif
            toolsCache.initializeToolsCacheVariableFromToolsCacheFile();
            if (toolsCache.vsTools.empty() && toolsCache.compilers.empty())
            {
                printErrorMessage(
                    "No compiler found from ToolsCache variable. Please ensure htools has been run as admin before\n");
                exit(EXIT_FAILURE);
            }

            auto getCommand = [&](const bool configureExe) {
                string command = addQuotes(toolsCache.vsTools[0].compiler.bTPath) + " ";
                for (const string &str : toolsCache.vsTools[0].includeDirs)
                {
                    command += "/I " + addQuotes(str) + " ";
                }
                command += useJsonFileCompressionDef;
                command += configureExe ? "" : " /D BUILD_MODE /D NDEBUG ";
                command += "/I " + hconfigureHeaderPath.string() + " /I " + thirdPartyHeaderPath.string() + " /I " +
                           jsonHeaderPath.string() + " /I " + rapidjsonHeaderPath.string() + " /I " +
                           lz4Header.string() +
                           " /std:c++latest /O0 /GR- /EHsc /MT /nologo {SOURCE_DIRECTORY}/hmake.cpp "
                           "/Fo{CONFIGURE_DIRECTORY}/" +
                           (configureExe ? "configure.obj" : "build.obj") + " /link /SUBSYSTEM:CONSOLE /NOLOGO ";
                for (const string &str : toolsCache.vsTools[0].libraryDirs)
                {
                    command += "/LIBPATH:" + addQuotes(str) + " ";
                }
                command += "/WHOLEARCHIVE:" +
                           addQuotes((configureExe ? hconfigureCStaticLibPath : hconfigureBStaticLibPath).string()) +
                           " kernel32.lib synchronization.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib "
                           "oleaut32.lib uuid.lib comdlg32.lib advapi32.lib" +
                           " /OUT:{CONFIGURE_DIRECTORY}/" +
                           (configureExe ? getActualNameFromTargetName(TargetType::EXECUTABLE, os, "configure")
                                         : getActualNameFromTargetName(TargetType::EXECUTABLE, os, "build"));
                return command;
            };
            cache.configureExeBuildScript = getCommand(true);
            cache.buildExeBuildScript = getCommand(false);
        }

        Json cacheJson = cache;
        ofstream("cache.json") << cacheJson.dump(4);
        return 0;
    }

    string configureExePath =
        (current_path() / getActualNameFromTargetName(TargetType::EXECUTABLE, os, "configure")).string();

    if (onlyConfigure)
    {
        if (!exists(path(configureExePath)))
        {
            printErrorMessage("configure.exe does not exists\n");
            exit(EXIT_FAILURE);
        }
        return std::system(configureExePath.c_str());
    }

    Json cacheJson;
    ifstream("cache.json") >> cacheJson;
    configureNode = Node::getNodeNonNormalized(current_path().string(), false);
    Cache cacheLocal = cacheJson;

    path sourceDirPath = cacheJson.at(JConsts::sourceDirectory).get<string>();
    if (sourceDirPath.is_relative())
    {
        sourceDirPath = (current_path() / sourceDirPath).lexically_normal();
    }
    sourceDirPath = sourceDirPath.string();

    const string srcDirString = "{SOURCE_DIRECTORY}";
    const string confDirString = "{CONFIGURE_DIRECTORY}";

    if (cacheLocal.configureExeBuildScript.empty())
    {
        printErrorMessage("No script provided for building configure executable\n");
        exit(EXIT_FAILURE);
    }
    if (cacheLocal.buildExeBuildScript.empty())
    {
        printErrorMessage("No script provided for building build executable\n");
        exit(EXIT_FAILURE);
    }

    Seconds compileConfigureTime{};
    Seconds compileBuildTime{};
    Seconds runConfigureTime{};
    int configureRunExitStatus = EXIT_SUCCESS;

    std::mutex printMutex;

    auto scriptExecution = [&](const bool configureExe) {
        string &command = configureExe ? cacheLocal.configureExeBuildScript : cacheLocal.buildExeBuildScript;
        const string label = configureExe ? "configure" : "build";
        const string objFilename = configureExe ? "configure.obj" : "build.obj";

        replaceAll(command, srcDirString, sourceDirPath.string());
        replaceAll(command, confDirString, current_path().string());

        const auto compileStart = Clock::now();
        RunCommand compileRun;
        compileRun.runProcess(command.c_str());
        const Seconds compileDuration = Clock::now() - compileStart;

        if (configureExe)
        {
            compileConfigureTime = compileDuration;
        }
        else
        {
            compileBuildTime = compileDuration;
        }

        if (std::filesystem::exists(objFilename))
        {
            std::filesystem::remove(objFilename);
        }

        {
            std::lock_guard _(printMutex);
            if (compileRun.exitStatus == EXIT_SUCCESS)
            {
                if (!compileRun.output->empty() && *compileRun.output != "hmake.cpp\r\n")
                {
                    printMessage(command);
                    printMessage(*compileRun.output);
                }
                else
                {
                    printMessage(FORMAT("Built {} executable  ({})\n", label, formatSeconds(compileDuration)));
                }
            }
            else
            {
                printMessage("Errors in Building " + label + " Executable\n");
                printMessage(command + "\n");
                printMessage(*compileRun.output + "\n");
                exit(compileRun.exitStatus);
            }
        }

        if (configureExe)
        {
            printMessage("Running configure executable\n");

            const auto runStart = Clock::now();
            RunCommand configureRun;
            configureRun.runProcess(configureExePath.c_str());
            runConfigureTime = Clock::now() - runStart;

            std::lock_guard _(printMutex);
            configureRunExitStatus = configureRun.exitStatus;
            if (configureRun.exitStatus == EXIT_SUCCESS)
            {
                printMessage(*configureRun.output);
            }
            else
            {
                printErrorMessage(*configureRun.output);
            }
        }
    };

    std::thread configureExeThread(scriptExecution, true);
    std::thread buildExeThread(scriptExecution, false);

    configureExeThread.join();
    buildExeThread.join();

    const Seconds totalTime = Clock::now() - totalStart;

    printMessage("\n");
    printMessage(FORMAT("Step 1 — Compile configure executable : {}\n", formatSeconds(compileConfigureTime)));
    printMessage(FORMAT("Step 2 — Compile build executable     : {}\n", formatSeconds(compileBuildTime)));
    printMessage(FORMAT("Step 3 — Run configure executable     : {}\n", formatSeconds(runConfigureTime)));
    printMessage("---------------------------------------------------\n");
    printMessage(FORMAT("Total                                 : {}\n", formatSeconds(totalTime)));

    return configureRunExitStatus;
}