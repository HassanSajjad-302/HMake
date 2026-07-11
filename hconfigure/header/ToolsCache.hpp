#ifndef HMAKE_TOOLSCACHE_HPP
#define HMAKE_TOOLSCACHE_HPP

#include "BuildTools.hpp"
#include "Features.hpp"
#include <vector>
using std::vector;

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
    vector<string> includeDirs;
    vector<string> libraryDirs;
    VSTools(string batchFile, path toolBinDir, Arch hostArch_, AddressModel hostAM_, Arch targetArch_,
            AddressModel targetAM_, bool executingFromWSL = false);
    VSTools() = default;
    void initializeFromVSToolBatchCommand(bool executingFromWSL = false);
    void initializeFromVSToolBatchCommand(const string &command, bool executingFromWSL = false);
};

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

struct ToolsCache
{
    path toolsCacheFilePath;
    vector<VSTools> vsTools;
    vector<LinuxTools> linuxTools;
    // Following are tools besides vsTools
    vector<Compiler> compilers;
    vector<Linker> linkers;
    vector<Archiver> archivers;
    ToolsCache();
    void detectToolsAndInitialize();
    void initializeToolsCacheVariableFromToolsCacheFile();
    void writeToolsCacheFile();
};

inline ToolsCache toolsCache;
#endif // HMAKE_TOOLSCACHE_HPP
