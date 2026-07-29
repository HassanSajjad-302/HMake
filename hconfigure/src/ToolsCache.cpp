#include "ToolsCache.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "RunCommand.hpp"

#include <filesystem>
#include <fstream>
#include <utility>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <sstream>
#include <format>

using std::ofstream, std::filesystem::remove;

VSTools::VSTools(string batchFile, path toolBinDir, const Arch hostArch_, const AddressModel hostAM_,
                 const Arch targetArch_, const AddressModel targetAM_, const bool executingFromWSL)
    : command(std::move(batchFile)), hostArch(hostArch_), hostAM(hostAM_), targetArch(targetArch_), targetAM(targetAM_)
{
    bool hostSupported = false;
    bool targetSupported = false;
    const string str = toolBinDir.parent_path().filename().string();
    const vector<string_view> vec = split(str, '.');
    const Version toolVersion(atol(vec[0].data()), atoi(vec[1].data()), atoi(vec[2].data()));
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
        printErrorMessage(FORMAT("Visual Studio toolchain does not support the requested architecture combination.\n"
                                 "Host address model: {}\nTarget address model: {}",
                                 static_cast<uint8_t>(hostAM), static_cast<uint8_t>(targetAM)));
    }
    compiler.bTFamily = linker.bTFamily = archiver.bTFamily = BTFamily::MSVC;
    compiler.bTVersion = linker.bTVersion = archiver.bTVersion = toolVersion;
    toolBinDir = toolBinDir.lexically_normal();
    compiler.bTPath = path(toolBinDir / "cl.exe").lexically_normal().string();
    linker.bTPath = path(toolBinDir / "link.exe").lexically_normal().string();
    archiver.bTPath = path(toolBinDir / "lib.exe").lexically_normal().string();
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
        printErrorMessage(FORMAT("Could not initialize the Visual Studio build environment.\nCommand: {}\n"
                                 "Exit code: {}",
                                 finalCommand, code));
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
    accumulatedPaths.pop_back(); // Remove trailing newline/space from echo output.
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    includeDirs = splitPathsAndAssignToVector(accumulatedPaths);
    convertPathsToWSLPaths(includeDirs);
    accumulatedPaths = fileToString(temporaryLibFilename);
    remove(temporaryLibFilename);
    accumulatedPaths.pop_back(); // Remove trailing newline/space from echo output.
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    libraryDirs = splitPathsAndAssignToVector(accumulatedPaths);
    convertPathsToWSLPaths(libraryDirs);
}

LinuxTools::LinuxTools(Compiler compiler_) : compiler{std::move(compiler_)}
{
    const string str = std::filesystem::current_path().string();
    const string temporaryCppFile = "temporary-main.cpp";
    ofstream(temporaryCppFile) << "";
    command = compiler.bTPath + " " + temporaryCppFile + " -E -v";
    RunCommand r;
    r.runProcess(command.c_str());
    remove(temporaryCppFile);
    if (r.exitStatus != EXIT_SUCCESS)
    {
        printErrorMessage(FORMAT("Could not query the compiler environment.\nCompiler: {}\nCommand: {}\n"
                                 "Exit code: {}\nCompiler output:\n{}",
                                 compiler.bTPath, command, r.exitStatus, *r.output));
    }

    const vector<string_view> lines = split(*r.output, '\n');
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
                // Each line starts with a leading space; trim it before storing.
                emplaceInVector(includeDirs, string(lines[i].substr(1, lines[i].size() - 1)));
                printMessage(FORMAT("Found standard include-dir {}\n", includeDirs[includeDirs.size() - 1]));
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

static string archToString(Arch a) {
    switch (a) {
        case Arch::X86: return "X86";
        case Arch::ARM: return "ARM";
        case Arch::S390X: return "S390X";
        case Arch::POWER: return "POWER";
        case Arch::LOONGARCH: return "LOONGARCH";
        default: return "NONE";
    }
}
static Arch stringToArch(const string& s) {
    if (s == "X86") return Arch::X86;
    if (s == "ARM") return Arch::ARM;
    if (s == "S390X") return Arch::S390X;
    if (s == "POWER") return Arch::POWER;
    if (s == "LOONGARCH") return Arch::LOONGARCH;
    return Arch::NONE;
}

static string amToString(AddressModel am) {
    switch (am) {
        case AddressModel::A_32: return "A_32";
        case AddressModel::A_64: return "A_64";
        default: return "NONE";
    }
}
static AddressModel stringToAm(const string& s) {
    if (s == "A_32") return AddressModel::A_32;
    if (s == "A_64") return AddressModel::A_64;
    return AddressModel::NONE;
}

static string familyToString(BTFamily f) {
    if (f == BTFamily::GCC) return "gcc";
    return "msvc";
}
static BTFamily stringToFamily(const string& s) {
    if (s == "gcc") return BTFamily::GCC;
    return BTFamily::MSVC;
}

static string subFamilyToString(BTSubFamily f) {
    if (f == BTSubFamily::CLANG) return "clang";
    return "";
}
static BTSubFamily stringToSubFamily(const string& s) {
    if (s == "clang") return BTSubFamily::CLANG;
    return BTSubFamily::NONE;
}

static void writeVersion(rapidjson::Value& val, const Version& v, rapidjson::Document::AllocatorType& alloc) {
    string s = std::format("{}.{}.{}", v.majorVersion, v.minorVersion, v.patchVersion);
    val.SetString(s.c_str(), s.length(), alloc);
}
static Version readVersion(const rapidjson::Value& val) {
    Version v;
    string s = val.GetString();
    std::stringstream ss(s);
    string item;
    int count = 0;
    while (std::getline(ss, item, '.')) {
        if (count == 0) v.majorVersion = std::stoi(item);
        else if (count == 1) v.minorVersion = std::stoi(item);
        else v.patchVersion = std::stoi(item);
        ++count;
    }
    return v;
}

static void writeBuildTool(rapidjson::Value& val, const BuildTool& bt, rapidjson::Document::AllocatorType& alloc) {
    val.SetObject();
    rapidjson::Value family(familyToString(bt.bTFamily).c_str(), alloc);
    val.AddMember("family", family, alloc);
    rapidjson::Value subFamily(subFamilyToString(bt.btSubFamily).c_str(), alloc);
    val.AddMember("sub-family", subFamily, alloc);
    rapidjson::Value version;
    writeVersion(version, bt.bTVersion, alloc);
    val.AddMember("version", version, alloc);
    rapidjson::Value path(bt.bTPath.c_str(), alloc);
    val.AddMember("path", path, alloc);
}
static void readBuildTool(const rapidjson::Value& val, BuildTool& bt) {
    if (val.HasMember("family")) bt.bTFamily = stringToFamily(val["family"].GetString());
    if (val.HasMember("sub-family")) bt.btSubFamily = stringToSubFamily(val["sub-family"].GetString());
    if (val.HasMember("version")) bt.bTVersion = readVersion(val["version"]);
    if (val.HasMember("path")) bt.bTPath = val["path"].GetString();
}

static void writeVSTools(rapidjson::Value& val, const VSTools& vt, rapidjson::Document::AllocatorType& alloc) {
    val.SetObject();
    rapidjson::Value command(vt.command.c_str(), alloc);
    val.AddMember("command", command, alloc);
    rapidjson::Value commandArguments(vt.commandArguments.c_str(), alloc);
    val.AddMember("commandArguments", commandArguments, alloc);
    
    rapidjson::Value compilerVal;
    writeBuildTool(compilerVal, vt.compiler, alloc);
    val.AddMember("compiler", compilerVal, alloc);
    
    rapidjson::Value linkerVal;
    writeBuildTool(linkerVal, vt.linker, alloc);
    val.AddMember("linker", linkerVal, alloc);
    
    rapidjson::Value archiverVal;
    writeBuildTool(archiverVal, vt.archiver, alloc);
    val.AddMember("archiver", archiverVal, alloc);
    
    rapidjson::Value hostArch(archToString(vt.hostArch).c_str(), alloc);
    val.AddMember("host-architecture", hostArch, alloc);
    
    rapidjson::Value hostAM(amToString(vt.hostAM).c_str(), alloc);
    val.AddMember("host-address-model", hostAM, alloc);
    
    rapidjson::Value targetArch(archToString(vt.targetArch).c_str(), alloc);
    val.AddMember("target-architecture", targetArch, alloc);
    
    rapidjson::Value targetAM(amToString(vt.targetAM).c_str(), alloc);
    val.AddMember("target-address-model", targetAM, alloc);
    
    rapidjson::Value includeDirsVal(rapidjson::kArrayType);
    for (const auto& dir : vt.includeDirs) {
        rapidjson::Value d(dir.c_str(), alloc);
        includeDirsVal.PushBack(d, alloc);
    }
    val.AddMember("include-dirs", includeDirsVal, alloc);
    
    rapidjson::Value libraryDirsVal(rapidjson::kArrayType);
    for (const auto& dir : vt.libraryDirs) {
        rapidjson::Value d(dir.c_str(), alloc);
        libraryDirsVal.PushBack(d, alloc);
    }
    val.AddMember("library-dirs", libraryDirsVal, alloc);
}
static VSTools readVSTools(const rapidjson::Value& val) {
    VSTools vt;
    if (val.HasMember("command")) vt.command = val["command"].GetString();
    if (val.HasMember("commandArguments")) vt.commandArguments = val["commandArguments"].GetString();
    if (val.HasMember("compiler")) readBuildTool(val["compiler"], vt.compiler);
    if (val.HasMember("linker")) readBuildTool(val["linker"], vt.linker);
    if (val.HasMember("archiver")) readBuildTool(val["archiver"], vt.archiver);
    if (val.HasMember("host-architecture")) vt.hostArch = stringToArch(val["host-architecture"].GetString());
    if (val.HasMember("host-address-model")) vt.hostAM = stringToAm(val["host-address-model"].GetString());
    if (val.HasMember("target-architecture")) vt.targetArch = stringToArch(val["target-architecture"].GetString());
    if (val.HasMember("target-address-model")) vt.targetAM = stringToAm(val["target-address-model"].GetString());
    
    if (val.HasMember("include-dirs")) {
        for (const auto& d : val["include-dirs"].GetArray()) {
            vt.includeDirs.push_back(d.GetString());
        }
    }
    if (val.HasMember("library-dirs")) {
        for (const auto& d : val["library-dirs"].GetArray()) {
            vt.libraryDirs.push_back(d.GetString());
        }
    }
    return vt;
}

static void writeLinuxTools(rapidjson::Value& val, const LinuxTools& lt, rapidjson::Document::AllocatorType& alloc) {
    val.SetObject();
    rapidjson::Value command(lt.command.c_str(), alloc);
    val.AddMember("command", command, alloc);
    
    rapidjson::Value compilerVal;
    writeBuildTool(compilerVal, lt.compiler, alloc);
    val.AddMember("compiler", compilerVal, alloc);
    
    rapidjson::Value includeDirsVal(rapidjson::kArrayType);
    for (const auto& dir : lt.includeDirs) {
        rapidjson::Value d(dir.c_str(), alloc);
        includeDirsVal.PushBack(d, alloc);
    }
    val.AddMember("include-dirs", includeDirsVal, alloc);
}
static LinuxTools readLinuxTools(const rapidjson::Value& val) {
    LinuxTools lt;
    if (val.HasMember("command")) lt.command = val["command"].GetString();
    if (val.HasMember("compiler")) readBuildTool(val["compiler"], lt.compiler);
    if (val.HasMember("include-dirs")) {
        for (const auto& d : val["include-dirs"].GetArray()) {
            lt.includeDirs.push_back(d.GetString());
        }
    }
    return lt;
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
            printErrorMessage("Cannot locate the tool cache because HOME is not set.\nEnvironment variable: HOME\n"
                              "Hint: set HOME to the current user's home directory.");
        }
    }
    else if constexpr (os == OS::NT)
    {
        toolsCacheFilePath = R"(C:\Program Files (x86)\HMake\)" + toolsCacheFile;
    }
}

void ToolsCache::detectToolsAndInitialize()
{
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
        linuxTools.emplace_back(Compiler(BTFamily::GCC, BTSubFamily::CLANG, Version(12, 2, 0),
                                         R"(/home/hassan/Projects/llvm-project/llvm/my-fork/bin/clang)"));
        linkers.emplace_back(BTFamily::GCC, BTSubFamily::CLANG, Version(12, 2, 0),
                             R"(/home/hassan/Projects/llvm-project/llvm/my-fork/bin/clang++)");
        archivers.emplace_back(BTFamily::GCC, BTSubFamily::CLANG, Version(12, 2, 0), "/usr/bin/ar");
    }
}

void ToolsCache::initializeToolsCacheVariableFromToolsCacheFile()
{
    if (!std::filesystem::exists(toolsCacheFilePath))
    {
        return;
    }
    std::ifstream ifs(toolsCacheFilePath);
    string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError())
    {
        return;
    }

    if (doc.HasMember(JConsts::vsTools.c_str()))
    {
        for (const auto& item : doc[JConsts::vsTools.c_str()].GetArray())
        {
            vsTools.push_back(readVSTools(item));
        }
    }
    if (doc.HasMember(JConsts::linuxTools.c_str()))
    {
        for (const auto& item : doc[JConsts::linuxTools.c_str()].GetArray())
        {
            linuxTools.push_back(readLinuxTools(item));
        }
    }
    if (doc.HasMember(JConsts::compilerArray.c_str()))
    {
        for (const auto& item : doc[JConsts::compilerArray.c_str()].GetArray())
        {
            Compiler c;
            readBuildTool(item, c);
            compilers.push_back(c);
        }
    }
    if (doc.HasMember(JConsts::linkerArray.c_str()))
    {
        for (const auto& item : doc[JConsts::linkerArray.c_str()].GetArray())
        {
            Linker l;
            readBuildTool(item, l);
            linkers.push_back(l);
        }
    }
    if (doc.HasMember(JConsts::archiverArray.c_str()))
    {
        for (const auto& item : doc[JConsts::archiverArray.c_str()].GetArray())
        {
            Archiver a;
            readBuildTool(item, a);
            archivers.push_back(a);
        }
    }
}

void ToolsCache::writeToolsCacheFile()
{
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    rapidjson::Value vsToolsVal(rapidjson::kArrayType);
    for (const auto& vt : vsTools)
    {
        rapidjson::Value v;
        writeVSTools(v, vt, alloc);
        vsToolsVal.PushBack(v, alloc);
    }
    doc.AddMember(rapidjson::Value(JConsts::vsTools.c_str(), alloc), vsToolsVal, alloc);

    rapidjson::Value linuxToolsVal(rapidjson::kArrayType);
    for (const auto& lt : linuxTools)
    {
        rapidjson::Value v;
        writeLinuxTools(v, lt, alloc);
        linuxToolsVal.PushBack(v, alloc);
    }
    doc.AddMember(rapidjson::Value(JConsts::linuxTools.c_str(), alloc), linuxToolsVal, alloc);

    rapidjson::Value compilersVal(rapidjson::kArrayType);
    for (const auto& c : compilers)
    {
        rapidjson::Value v;
        writeBuildTool(v, c, alloc);
        compilersVal.PushBack(v, alloc);
    }
    doc.AddMember(rapidjson::Value(JConsts::compilerArray.c_str(), alloc), compilersVal, alloc);

    rapidjson::Value linkersVal(rapidjson::kArrayType);
    for (const auto& l : linkers)
    {
        rapidjson::Value v;
        writeBuildTool(v, l, alloc);
        linkersVal.PushBack(v, alloc);
    }
    doc.AddMember(rapidjson::Value(JConsts::linkerArray.c_str(), alloc), linkersVal, alloc);

    rapidjson::Value archiversVal(rapidjson::kArrayType);
    for (const auto& a : archivers)
    {
        rapidjson::Value v;
        writeBuildTool(v, a, alloc);
        archiversVal.PushBack(v, alloc);
    }
    doc.AddMember(rapidjson::Value(JConsts::archiverArray.c_str(), alloc), archiversVal, alloc);

    ofstream ofs(toolsCacheFilePath);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    ofs << buffer.GetString();
}
