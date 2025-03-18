#ifndef HMAKE_TOOLSCACHE_HPP
#define HMAKE_TOOLSCACHE_HPP

#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "Features.hpp";
#include "nlohmann/json.hpp";
import <vector>;
#else
#include "BuildTools.hpp"
#include "Features.hpp"
#include "nlohmann/json.hpp"
#include <vector>
#endif

using std::vector;

using Json = nlohmann::json;

// On Windows standard libraries and includes are not provided by default. And tools used are different based on
// Architecture and Address-Model.
struct VSTools
{
    string command;
    string commandArguments;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    ScannerTool scanner;
    Arch hostArch;
    AddressModel hostAM;
    Arch targetArch;
    AddressModel targetAM;
    vector<string> includeDirs;
    vector<string> libraryDirs;
    VSTools(string batchFile, path toolBinDir, Arch hostArch_, AddressModel hostAM_, Arch targetArch_,
            AddressModel targetAM_, bool executingFromWSL = false);
    VSTools() = default;
    void initializeFromVSToolBatchCommand(bool executingFromWSL = false);
    void initializeFromVSToolBatchCommand(const string &command, bool executingFromWSL = false);
};
void to_json(Json &j, const VSTools &vsTool);
void from_json(const Json &j, VSTools &vsTool);

// On Windows standard libraries and includes are not provided by default. And tools used are different based on
// Architecture and Address-Model.
struct LinuxTools
{
    string command;
    Compiler compiler;
    vector<string> includeDirs;
    LinuxTools(Compiler compiler_);
    LinuxTools() = default;
};
void to_json(Json &j, const LinuxTools &linuxTools);
void from_json(const Json &j, LinuxTools &linuxTools);

struct ToolsCache
{
    path toolsCacheFilePath;
    vector<VSTools> vsTools;
    vector<LinuxTools> linuxTools;
    // Following are tools besides vsTools
    vector<Compiler> compilers;
    vector<Linker> linkers;
    vector<Archiver> archivers;
    vector<ScannerTool> scanners;
    ToolsCache();
    void detectToolsAndInitialize();
    void initializeToolsCacheVariableFromToolsCacheFile();
};
void to_json(Json &j, const ToolsCache &toolsCacheLocal);
void from_json(const Json &j, ToolsCache &toolsCacheLocal);
inline ToolsCache toolsCache;
#endif // HMAKE_TOOLSCACHE_HPP

