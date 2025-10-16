
#ifdef USE_HEADER_UNITS
import "ToolsCache.hpp";
import "BuildSystemFunctions.hpp";
import "JConsts.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <utility>;
#else
#include "ToolsCache.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <utility>
#endif

using std::ofstream, std::filesystem::remove;

VSTools::VSTools(string batchFile, path toolBinDir, const Arch hostArch_, const AddressModel hostAM_,
                 const Arch targetArch_, const AddressModel targetAM_, const bool executingFromWSL)
    : command(std::move(batchFile)), hostArch(hostArch_), hostAM(hostAM_), targetArch(targetArch_), targetAM(targetAM_)
{
    bool hostSupported = false;
    bool targetSupported = false;
    const vector<string> vec = split(toolBinDir.parent_path().filename().string(), ".");
    const Version toolVersion(atol(vec[0].c_str()), atoi(vec[1].c_str()), atoi(vec[2].c_str()));
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
        printErrorMessage("host or target not supported in VSTool\n");
    }
    compiler.bTFamily = linker.bTFamily = archiver.bTFamily = BTFamily::MSVC;
    compiler.bTVersion = linker.bTVersion = archiver.bTVersion = toolVersion;
    toolBinDir = toolBinDir.lexically_normal();
    compiler.bTPath = toolBinDir / "cl.exe";
    linker.bTPath = toolBinDir / "link.exe";
    archiver.bTPath = toolBinDir / "lib.exe";
    initializeFromVSToolBatchCommand(executingFromWSL);
}

void VSTools::initializeFromVSToolBatchCommand(const bool executingFromWSL)
{
    initializeFromVSToolBatchCommand(command + " " + commandArguments, executingFromWSL);
}

void VSTools::initializeFromVSToolBatchCommand(const string &finalCommand, bool executingFromWSL)
{
    const string temporaryIncludeFilename = "temporaryInclude.txt";
    const string temporaryLibFilename = "temporaryLib.txt";
    const string temporaryBatchFilename = "temporaryBatch.bat";
    const string cmdExe = executingFromWSL ? "cmd.exe /c " : "";
    const string batchCommand = "call " + finalCommand + "\n" + cmdExe + "echo %INCLUDE% > " +
                                temporaryIncludeFilename + "\n" + cmdExe + "echo %LIB%;%LIBPATH% > " +
                                temporaryLibFilename;
    ofstream(temporaryBatchFilename) << batchCommand;

    if (const int code = system((cmdExe + temporaryBatchFilename).c_str()); code != EXIT_SUCCESS)
    {
        printErrorMessage("Error in Initializing Environment\n");
    }
    remove(temporaryBatchFilename);

    auto splitPathsAndAssignToVector = [](string &accumulatedPaths) -> vector<string> {
        vector<string> separatedPaths{};
        size_t pos = accumulatedPaths.find(';');
        while (pos != string::npos)
        {
            string token = accumulatedPaths.substr(0, pos);
            if (token.empty())
            {
                break;
            }
            emplaceInVector(separatedPaths, std::move(token));
            accumulatedPaths.erase(0, pos + 1);
            pos = accumulatedPaths.find(';');
        }
        return separatedPaths;
    };

    auto convertPathsToWSLPaths = [executingFromWSL](vector<string> &vec) {
        if (executingFromWSL)
        {
            const string s = "\\";
            const string t = "/";

            const vector<string> vec2 = std::move(vec);
            vec.clear();
            for (const string &str : vec2)
            {
                string str2 = str;
                string::size_type n = 0;
                while ((n = str2.find(s, n)) != string::npos)
                {
                    str2.replace(n, s.size(), t);
                    n += t.size();
                }
                str2.erase(0, 2);
                string str3 = "/mnt/c" + str2;
                vec.emplace_back(std::move(str3));
            }
        }
    };

    string accumulatedPaths = fileToString(temporaryIncludeFilename);
    remove(temporaryIncludeFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    includeDirs = splitPathsAndAssignToVector(accumulatedPaths);
    convertPathsToWSLPaths(includeDirs);
    accumulatedPaths = fileToString(temporaryLibFilename);
    remove(temporaryLibFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    libraryDirs = splitPathsAndAssignToVector(accumulatedPaths);
    convertPathsToWSLPaths(libraryDirs);
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
    j[JConsts::includeDirs] = vsTool.includeDirs;
    j[JConsts::libraryDirs] = vsTool.libraryDirs;
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
    vsTool.includeDirs = j.at(JConsts::includeDirs).get<vector<string>>();
    vsTool.libraryDirs = j.at(JConsts::libraryDirs).get<vector<string>>();
}

LinuxTools::LinuxTools(Compiler compiler_) : compiler{std::move(compiler_)}
{
    const string temporaryIncludeFilename = "temporaryInclude.txt";

    const string str = std::filesystem::current_path().string();
    const string temporaryCppFile = "temporary-main.cpp";
    ofstream(temporaryCppFile) << "";
    command = compiler.bTPath.string() + " " + temporaryCppFile + " -E -v> " + temporaryIncludeFilename + " 2>&1";
    const int code = system(command.c_str());
    remove(temporaryCppFile);
    if (code != EXIT_SUCCESS)
    {
        printErrorMessage("Error in Initializing Environment\n");
    }

    string accumulatedPaths = fileToString(temporaryIncludeFilename);
    remove(temporaryIncludeFilename);
    const vector<string> lines = split(std::move(accumulatedPaths), "\n");

    size_t foundIndex = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (lines[i] == "#include <...> search starts here:")
        {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex)
    {
        size_t endIndex = 0;
        for (size_t i = foundIndex + 1; i < lines.size(); ++i)
        {
            if (lines[i] == "End of search list.")
            {
                endIndex = i;
                break;
            }
        }

        if (endIndex)
        {
            for (size_t i = foundIndex + 1; i < endIndex; ++i)
            {
                // first character is space, so substr is copied
                emplaceInVector(includeDirs, lines[i].substr(1, lines[i].size() - 1));
            }
        }
        else
        {
            printMessage("Warning! No standard include found during LinuxTools::\n");
        }
    }
    else
    {
        printMessage("Warning! No standard include found during LinuxTools::\n");
    }
}

void to_json(Json &j, const LinuxTools &linuxTools)
{
    j[JConsts::command] = linuxTools.command;
    j[JConsts::compiler] = linuxTools.compiler;
    j[JConsts::includeDirs] = linuxTools.includeDirs;
}

void from_json(const Json &j, LinuxTools &linuxTools)
{
    linuxTools.command = j.at(JConsts::command).get<string>();
    linuxTools.compiler = j.at(JConsts::compiler).get<Compiler>();
    linuxTools.includeDirs = j.at(JConsts::includeDirs).get<vector<string>>();
}

ToolsCache::ToolsCache()
{
    const string toolsCacheFile = "toolsCache.json";
    if constexpr (os == OS::LINUX)
    {
        if (const char *homedir = getenv("HOME"); homedir)
        {
            toolsCacheFilePath = path(homedir) / ".hmake" / path(toolsCacheFile);
        }
        else
        {
            printErrorMessage("Exiting in ToolsCache Constructor. HOME Environment variable not set\n");
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
    // 2022 Visual Studio Community is req. So is g++-12.2.0.

    if constexpr (os == OS::NT)
    {
        string batchFilePath =
            R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")";
        path toolBinDir = R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin)";
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_64, Arch::X86, AddressModel::A_64);
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_64, Arch::X86, AddressModel::A_32);
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_32, Arch::X86, AddressModel::A_64);
        vsTools.emplace_back(batchFilePath, toolBinDir, Arch::X86, AddressModel::A_32, Arch::X86, AddressModel::A_32);
    }
    else if constexpr (os == OS::LINUX)
    {
        linuxTools.emplace_back(Compiler(BTFamily::GCC, Version(12, 2, 0), "/usr/bin/c++"));
        linkers.emplace_back(BTFamily::GCC, Version(12, 2, 0), "/usr/bin/c++");
        archivers.emplace_back(BTFamily::GCC, Version(12, 2, 0), "/usr/bin/ar");
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
    j[JConsts::linuxTools] = toolsCacheLocal.linuxTools;
    j[JConsts::compilerArray] = toolsCacheLocal.compilers;
    j[JConsts::linkerArray] = toolsCacheLocal.linkers;
    j[JConsts::archiverArray] = toolsCacheLocal.archivers;
}

void from_json(const Json &j, ToolsCache &toolsCacheLocal)
{
    toolsCacheLocal.vsTools = j.at(JConsts::vsTools).get<vector<VSTools>>();
    toolsCacheLocal.linuxTools = j.at(JConsts::linuxTools).get<vector<LinuxTools>>();
    toolsCacheLocal.compilers = j.at(JConsts::compilerArray).get<vector<Compiler>>();
    toolsCacheLocal.linkers = j.at(JConsts::linkerArray).get<vector<Linker>>();
    toolsCacheLocal.archivers = j.at(JConsts::archiverArray).get<vector<Archiver>>();
}
