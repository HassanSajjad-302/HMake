#ifndef HMAKE_TOOLSCACHE_HPP
#define HMAKE_TOOLSCACHE_HPP
#include "BuildTools.hpp"
#include "Features.hpp"
#include "nlohmann/json.hpp"
#include <set>
#include <string>
#include <vector>

using std::vector;

using std::string, std::set;
using Json = nlohmann::ordered_json;

// On Windows standard libraries and includes are not provided by default. And tools used are different based on
// Architecture and Address-Model.
struct VSTools
{
    string command;
    string commandArguments;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    Arch hostArch;
    AddressModel hostAM;
    Arch targetArch;
    AddressModel targetAM;
    set<string> includeDirectories;
    set<string> libraryDirectories;
    VSTools(string batchFile, path toolBinDir, Arch hostArch_, AddressModel hostAM_, Arch targetArch_,
            AddressModel targetAM_, bool executingFromWSL = false);
    VSTools() = default;
    void initializeFromVSToolBatchCommand(bool executingFromWSL = false);
    void initializeFromVSToolBatchCommand(const string &command, bool executingFromWSL = false);
};
void to_json(Json &j, const VSTools &vsTool);
void from_json(const Json &j, VSTools &vsTool);

struct ToolsCache
{
    path toolsCacheFilePath;
    vector<VSTools> vsTools;
    // Following are tools besides vsTools
    vector<Compiler> compilers;
    vector<Linker> linkers;
    vector<Archiver> archivers;
    ToolsCache();
    void detectToolsAndInitialize();
    void initializeToolsCacheVariableFromToolsCacheFile();
};
void to_json(Json &j, const ToolsCache &toolsCacheLocal);
void from_json(const Json &j, ToolsCache &toolsCacheLocal);
inline ToolsCache toolsCache;
#endif // HMAKE_TOOLSCACHE_HPP