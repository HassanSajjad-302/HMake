
#ifdef USE_HEADER_UNITS
import "ToolsCache.hpp";
import "BuildSystemFunctions.hpp";
import "JConsts.hpp";
import "Utilities.hpp";
import "fmt/format.h";
import <filesystem>;
import <fstream>;
import <utility>;
#else
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "ToolsCache.hpp"
#include "Utilities.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>
#include <utility>
#endif

using std::ofstream, fmt::print, std::filesystem::remove;

VSTools::VSTools(string batchFile, path toolBinDir, Arch hostArch_, AddressModel hostAM_, Arch targetArch_,
                 AddressModel targetAM_, bool executingFromWSL)
    : command(std::move(batchFile)), hostArch(hostArch_), hostAM(hostAM_), targetArch(targetArch_), targetAM(targetAM_)
{
    bool hostSupported = false;
    bool targetSupported = false;
    vector<string> vec = split(toolBinDir.parent_path().filename().string(), ".");
    Version toolVersion(atol(vec[0].c_str()), atoi(vec[1].c_str()), atoi(vec[2].c_str()));
    if (hostArch_ == Arch::X86)
    {
        if (hostAM == AddressModel::A_32)
        {
            toolBinDir /= "Hostx86/";
            hostSupported = true;
        }
        else if (hostAM == AddressModel::A_64)
        {
            toolBinDir /= "Hostx64/";
            hostSupported = true;
        }
    }
    if (targetArch_ == Arch::X86)
    {
        if (targetAM == AddressModel::A_32)
        {
            toolBinDir /= "x86/";
            targetSupported = true;
            commandArguments = hostAM == AddressModel::A_32 ? "x86" : "amd64_x86";
        }
        else if (targetAM == AddressModel::A_64)
        {
            toolBinDir /= "x64/";
            targetSupported = true;
            commandArguments = hostAM == AddressModel::A_32 ? "x86_x64" : "x64";
        }
    }
    // TODO
    // Investigating batchFile reveals that other platforms like arm are supported but currently don't know where the
    // tools will be installed. Support the vcvarsall.bat file code here and provide API about it.
    if (!hostSupported || !targetSupported)
    {
        print(stderr, "host or target not supported in VSTool\n");
        exit(EXIT_FAILURE);
    }
    compiler.bTFamily = linker.bTFamily = archiver.bTFamily = BTFamily::MSVC;
    compiler.bTVersion = linker.bTVersion = archiver.bTVersion = toolVersion;
    toolBinDir = toolBinDir.lexically_normal();
    compiler.bTPath = toolBinDir / "cl.exe";
    linker.bTPath = toolBinDir / "link.exe";
    archiver.bTPath = toolBinDir / "lib.exe";
    initializeFromVSToolBatchCommand(executingFromWSL);
}

void VSTools::initializeFromVSToolBatchCommand(bool executingFromWSL)
{
    initializeFromVSToolBatchCommand(command + " " + commandArguments, executingFromWSL);
}

void VSTools::initializeFromVSToolBatchCommand(const string &finalCommand, bool executingFromWSL)
{
    string temporaryIncludeFilename = "temporaryInclude.txt";
    string temporaryLibFilename = "temporaryLib.txt";
    string temporaryBatchFilename = "temporaryBatch.bat";
    string cmdExe = executingFromWSL ? "cmd.exe /c " : "";
    string batchCommand = "call " + finalCommand + "\n" + cmdExe + "echo %INCLUDE% > " + temporaryIncludeFilename +
                          "\n" + cmdExe + "echo %LIB%;%LIBPATH% > " + temporaryLibFilename;
    ofstream(temporaryBatchFilename) << batchCommand;

    if (int code = system((cmdExe + temporaryBatchFilename).c_str()); code != EXIT_SUCCESS)
    {
        print(stderr, "Error in Initializing Environment\n");
        exit(EXIT_FAILURE);
    }
    remove(temporaryBatchFilename);

    auto splitPathsAndAssignToVector = [](string &accumulatedPaths) -> set<string> {
        set<string> separatedPaths{};
        size_t pos = accumulatedPaths.find(';');
        while (pos != std::string::npos)
        {
            std::string token = accumulatedPaths.substr(0, pos);
            if (token.empty())
            {
                break;
            }
            separatedPaths.emplace(token);
            accumulatedPaths.erase(0, pos + 1);
            pos = accumulatedPaths.find(';');
        }
        return separatedPaths;
    };

    auto convertPathsToWSLPaths = [executingFromWSL](set<string> &vec) {
        if (executingFromWSL)
        {
            const std::string s = "\\";
            const std::string t = "/";

            set<string> vec2 = std::move(vec);
            vec.clear();
            for (const string &str : vec2)
            {
                string str2 = str;
                std::string::size_type n = 0;
                while ((n = str2.find(s, n)) != std::string::npos)
                {
                    str2.replace(n, s.size(), t);
                    n += t.size();
                }
                str2.erase(0, 2);
                string str3 = "/mnt/c" + str2;
                vec.emplace(std::move(str3));
            }
        }
    };

    string accumulatedPaths = file_to_string(temporaryIncludeFilename);
    remove(temporaryIncludeFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    includeDirectories = splitPathsAndAssignToVector(accumulatedPaths);
    convertPathsToWSLPaths(includeDirectories);
    accumulatedPaths = file_to_string(temporaryLibFilename);
    remove(temporaryLibFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    libraryDirectories = splitPathsAndAssignToVector(accumulatedPaths);
    convertPathsToWSLPaths(libraryDirectories);
}

void to_json(Json &j, const VSTools &vsTool)
{
    j[JConsts::command] = vsTool.command;
    j[JConsts::commandArguments] = vsTool.commandArguments;
    j[JConsts::compiler] = vsTool.compiler;
    j[JConsts::linker] = vsTool.linker;
    j[JConsts::archiver] = vsTool.archiver;
    j[JConsts::hostArchitecture] = vsTool.hostArch;
    j[JConsts::hostArddressModel] = vsTool.hostAM;
    j[JConsts::targetArchitecture] = vsTool.targetArch;
    j[JConsts::targetAddressModel] = vsTool.targetAM;
    j[JConsts::includeDirectories] = vsTool.includeDirectories;
    j[JConsts::libraryDirectories] = vsTool.libraryDirectories;
}

void from_json(const Json &j, VSTools &vsTool)
{
    vsTool.command = j.at(JConsts::command).get<string>();
    vsTool.commandArguments = j.at(JConsts::commandArguments).get<string>();
    vsTool.compiler = j.at(JConsts::compiler).get<Compiler>();
    vsTool.linker = j.at(JConsts::linker).get<Linker>();
    vsTool.archiver = j.at(JConsts::archiver).get<Archiver>();
    vsTool.hostArch = j.at(JConsts::hostArchitecture).get<Arch>();
    vsTool.hostAM = j.at(JConsts::hostArddressModel).get<AddressModel>();
    vsTool.targetArch = j.at(JConsts::targetArchitecture).get<Arch>();
    vsTool.targetAM = j.at(JConsts::targetAddressModel).get<AddressModel>();
    vsTool.includeDirectories = j.at(JConsts::includeDirectories).get<set<string>>();
    vsTool.libraryDirectories = j.at(JConsts::libraryDirectories).get<set<string>>();
}

ToolsCache::ToolsCache()
{
    string toolsCacheFile = "toolsCache.json";
    if constexpr (os == OS::LINUX)
    {
        if (const char *homedir = getenv("HOME"); homedir)
        {
            toolsCacheFilePath = path(homedir) / ".hmake" / path(toolsCacheFile);
        }
        else
        {
            print(stderr, "Exiting in ToolsCache Constructor. HOME Environment variable not set\n");
            exit(EXIT_FAILURE);
        }
    }
    else if constexpr (os == OS::NT)
    {
        toolsCacheFilePath = R"(C:\Program Files (x86)\HMake\)" + toolsCacheFile;
    }
}

void ToolsCache::detectToolsAndInitialize()
{
    // TODO
    // HMake does not have installer yet. Otherwise, this maybe added in installer. Currently, nothing is detected yet.
    // 2022 Visual Studio Community is requirement. So is g++-12.2.0.
    if constexpr (os == OS::NT)
    {
        string batchFilePath =
            R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")";
        path toolBinDir = R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.35.32215\bin)";
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_64, Arch::X86, AddressModel::A_64);
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_64, Arch::X86, AddressModel::A_32);
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_32, Arch::X86, AddressModel::A_64);
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_32, Arch::X86, AddressModel::A_32);
    }
    else if constexpr (os == OS::LINUX)
    {
        compilers.emplace_back(BTFamily::GCC, Version(12, 2, 0), "/usr/bin/g++");
        linkers.emplace_back(BTFamily::GCC, Version(12, 2, 0), "/usr/bin/g++");
        archivers.emplace_back(BTFamily::GCC, Version(12, 2, 0), "/usr/bin/ar");
    }
    if (!exists(toolsCacheFilePath))
    {
        std::filesystem::create_directories(toolsCacheFilePath);
    }
}

void ToolsCache::initializeToolsCacheVariableFromToolsCacheFile()
{
    Json toolsCacheJson;
    std::ifstream(toolsCacheFilePath) >> toolsCacheJson;
    *this = toolsCacheJson;
}

void to_json(Json &j, const ToolsCache &toolsCacheLocal)
{
    j[JConsts::vsTools] = toolsCacheLocal.vsTools;
    j[JConsts::compilerArray] = toolsCacheLocal.compilers;
    j[JConsts::linkerArray] = toolsCacheLocal.linkers;
    j[JConsts::archiverArray] = toolsCacheLocal.archivers;
}

void from_json(const Json &j, ToolsCache &toolsCacheLocal)
{
    toolsCacheLocal.vsTools = j.at(JConsts::vsTools).get<vector<VSTools>>();
    toolsCacheLocal.compilers = j.at(JConsts::compilerArray).get<vector<Compiler>>();
    toolsCacheLocal.linkers = j.at(JConsts::linkerArray).get<vector<Linker>>();
    toolsCacheLocal.archivers = j.at(JConsts::archiverArray).get<vector<Archiver>>();
}

