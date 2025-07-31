
#ifdef USE_HEADER_UNITS
import "TargetCache.hpp";
#else
#include "TargetCache.hpp"
#endif
#include <BuildSystemFunctions.hpp>

TargetCache::TargetCache(const string &name)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const uint64_t index = valueIndexInSubArray(configCache, Value(svtogsr(name)));
        if (index == UINT64_MAX)
        {
            configCache.PushBack(Value(kArrayType), ralloc);
            buildCache.PushBack(Value(kArrayType), ralloc);
            targetCacheIndex = configCache.Size() - 1;
            configCache[targetCacheIndex].PushBack(Value(kStringType).SetString(name.c_str(), name.size(), ralloc),
                                                   ralloc);
        }
        else
        {
            targetCacheIndex = index;
            getConfigCache().Clear();
            configCache[targetCacheIndex].PushBack(Value(kStringType).SetString(name.c_str(), name.size(), ralloc),
                                                   ralloc);
        }

#ifndef BUILD_MODE
        myId = idCount;
        ++idCount;
        if (auto [pos, ok] = targetCacheIndexAndMyIdHashMap.emplace(targetCacheIndex, myId); !ok)
        {
            printErrorMessage(
                FORMAT("Attempting to add 2 targets with same name {} in config-cache.json\n", name));
            errorExit();
        }

#endif
        buildOrConfigCacheCopy.PushBack(Value(kStringType).SetString(name.c_str(), name.size(), ralloc), ralloc);
    }
    else
    {
        const uint64_t index = valueIndexInSubArrayConsidered(configCache, Value(svtogsr(name)));
        if (index != UINT64_MAX)
        {
            targetCacheIndex = index;
            buildOrConfigCacheCopy = Value().CopyFrom(getBuildCache(), cacheAlloc);
        }
        else
        {
            printErrorMessage(FORMAT(
                "Target {} not found in config-cache.\nMaybe you need to run hhelper first to update the target-cache.",
                name));
            errorExit();
        }
    }
    flat_hash_map<int, int> a;
    a.emplace(2, 3);
}

Value &TargetCache::getConfigCache() const
{
    return configCache[targetCacheIndex];
}

Value &TargetCache::getBuildCache() const
{
    return buildCache[targetCacheIndex];
}

void TargetCache::copyBackConfigCacheMutexLocked() const
{
    std::lock_guard _(configCacheMutex);
    getConfigCache().CopyFrom(buildOrConfigCacheCopy, ralloc);
}


// Node Index or FilePath
struct NodeIndexOrFilePath
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    uint32_t index{};
#else
    string_view filePath;
#endif
};

// Hash Or Compile Command
struct CCOrHash
{
#ifdef USE_COMMAND_HASH
    uint32_t hash{};
#else
    string_view compilerCommand;
#endif
};

struct ConfigCache
{
    struct Cpp
    {
        span<NodeIndexOrFilePath> reqInclsArray;
        span<NodeIndexOrFilePath> useReqInclsArray;
        span<NodeIndexOrFilePath> reqHUDirsArray;
        span<NodeIndexOrFilePath> useReqHUDirsArray;
        span<NodeIndexOrFilePath> sourceFiles;
        span<NodeIndexOrFilePath> moduleFiles;
        span<NodeIndexOrFilePath> headerUnits;
        NodeIndexOrFilePath buildCacheFilesDirPath;
    };

    struct Link
    {
        span<NodeIndexOrFilePath> reqLibraryDirsArray;
        span<NodeIndexOrFilePath> useReqLibraryDirsArray;
        NodeIndexOrFilePath outputFileNode;
        NodeIndexOrFilePath buildCacheFilesDirPath;
    };
};

struct BuildCache
{
    struct Cpp
    {
        struct SourceFile
        {
            NodeIndexOrFilePath fullPath;
            CCOrHash compileCommandWithTool;
            span<NodeIndexOrFilePath> headerFiles;
        };

        struct ModuleFileCache
        {
            struct SmRules
            {
                struct SingleHeaderUnitDep
                {
                    NodeIndexOrFilePath fullPath;
                    bool angle{};
                    uint32_t targetIndex{};
                    uint32_t myIndex{};
                };

                struct SingleModuleDep
                {
                    NodeIndexOrFilePath fullPath;
                    string logicalName;
                };

                string exportName;
                bool isInterface{};
                span<SingleHeaderUnitDep> headerUnitArray;
                span<SingleModuleDep> moduleArray;
            };

            SourceFile srcFile;
            span<NodeIndexOrFilePath> headerFiles;
            SmRules smRules;
            CCOrHash compileCommandWithTool;
        };

        span<SourceFile> sourceFiles;
        span<ModuleFileCache> moduleFiles;
        span<ModuleFileCache> headerUnits;
    };

    struct Link
    {
        CCOrHash commandWithoutArgumentsWithTools;
        span<NodeIndexOrFilePath> objectFiles;
    };
};

using ModuleFileCache = BuildCache::Cpp::ModuleFileCache;

bool readBool(const char *ptr, uint32_t &bytesRead);
uint32_t readUint32(const char *ptr, uint32_t &bytesRead);
string_view readStringView(const char *ptr, uint32_t &bytesRead);
NodeIndexOrFilePath readNodeIndexOrFilePath(const char *ptr, uint32_t &bytesRead);
CCOrHash readCCOrHash(const char *ptr, uint32_t &bytesRead);
span<NodeIndexOrFilePath> readNoIndexOrFilePathSpan(const char *ptr, uint32_t &bytesRead);
ConfigCache::Cpp readCppConfigCache(const char *ptr, uint32_t &bytesRead);
ConfigCache::Link readLinkConfigCache(const char *ptr, uint32_t &bytesRead);
BuildCache::Cpp::SourceFile readSourceFileBuildCache(const char *ptr, uint32_t &bytesRead);
span<BuildCache::Cpp::SourceFile> readSourceFilesBuildCacheSpan(const char *ptr, uint32_t &bytesRead);
ModuleFileCache::SmRules::SingleHeaderUnitDep readSingleHeaderUnitDep(const char *ptr, uint32_t &bytesRead);
span<ModuleFileCache::SmRules::SingleHeaderUnitDep> readSingleHeaderUnitDepSpan(const char *ptr, uint32_t &bytesRead);
ModuleFileCache::SmRules::SingleModuleDep readSingleModuleDep(const char *ptr, uint32_t &bytesRead);
span<ModuleFileCache::SmRules::SingleModuleDep> readSingleModuleDepSpan(const char *ptr, uint32_t &bytesRead);
ModuleFileCache::SmRules readSMRules(const char *ptr, uint32_t &bytesRead);
ModuleFileCache readModuleFileBuildCache(const char *ptr, uint32_t &bytesRead);
span<ModuleFileCache> readModuleFilesBuildCacheSpan(const char *ptr, uint32_t &bytesRead);
BuildCache::Cpp readCppBuildCache(const char *ptr, uint32_t &bytesRead);
BuildCache::Link readLinkBuildCache(const char *ptr, uint32_t &bytesRead);

void writeBool(vector<char> &buffer, const bool &value);
void writeUint32(vector<char> &buffer, const uint32_t &data);
void writeStringView(vector<char> &buffer, const string_view &data);
void writeNodeIndexOrFilePath(vector<char> &buffer, const NodeIndexOrFilePath &value);
void writeCCOrHash(vector<char> &buffer, const CCOrHash &value);
void writeNodeIndexOrFilePathSpan(vector<char> &buffer, span<NodeIndexOrFilePath> array);
void writeCppConfigCache(vector<char> &buffer, const ConfigCache::Cpp &data);
void writeLinkConfigCache(vector<char> &buffer, const ConfigCache::Link &data);
void writeSourceFileBuildCache(vector<char> &buffer, const BuildCache::Cpp::SourceFile &data);
void writeSourceFileBuildCacheSpan(vector<char> &buffer, const BuildCache::Cpp::SourceFile &data);
void writeSourceFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::SourceFile> &data);
void writeSingleHeaderUnitDep(vector<char> &buffer,
                              const ModuleFileCache::SmRules::SingleHeaderUnitDep &data);
void writeSingleModuleDep(vector<char> &buffer, const ModuleFileCache::SmRules::SingleModuleDep &data);
void writeSMRules(vector<char> &buffer, const ModuleFileCache::SmRules &data);
void writeModuleFileBuildCache(vector<char> &buffer, const ModuleFileCache &data);
void writeModuleFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::ModuleFileCache> &data);
void writeCppBuildCache(vector<char> &buffer, const BuildCache::Cpp &data);
void writeLinkBuildCache(vector<char> &buffer, const BuildCache::Link &data);

bool readBool(const char *ptr, uint32_t &bytesRead)
{
    bool result;
    memcpy(&result, ptr, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint32_t readUint32(const char *ptr, uint32_t &bytesRead)
{
    uint32_t result;
    memcpy(&result, ptr, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

string_view readStringView(const char *ptr, uint32_t &bytesRead)
{
    uint32_t strSize = readUint32(ptr, bytesRead);
    uint32_t offset = bytesRead;
    bytesRead += strSize;
    return {ptr + offset, strSize};
}

NodeIndexOrFilePath readNodeIndexOrFilePath(const char *ptr, uint32_t &bytesRead)
{
    NodeIndexOrFilePath node;
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    node.index = readUint32(ptr, bytesRead);
#else
    node.filePath = readStringView(ptr, bytesRead);
#endif
    return node;
}

CCOrHash readCCOrHash(const char *ptr, uint32_t &bytesRead)
{
    CCOrHash cmd;
#ifdef USE_COMMAND_HASH
    cmd.hash = readUint32(ptr, bytesRead);
#else
    cmd.compilerCommand = readStringView(ptr, bytesRead);
#endif
    return cmd;
}

span<NodeIndexOrFilePath> readNoIndexOrFilePathSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(NodeIndexOrFilePath);
    return {(NodeIndexOrFilePath *)(ptr + offset), count};
}

ConfigCache::Cpp readCppConfigCache(const char *ptr, uint32_t &bytesRead)
{
    ConfigCache::Cpp cfg;
    cfg.reqInclsArray = readNoIndexOrFilePathSpan(ptr, bytesRead);
    cfg.useReqInclsArray = readNoIndexOrFilePathSpan(ptr, bytesRead);
    cfg.reqHUDirsArray = readNoIndexOrFilePathSpan(ptr, bytesRead);
    cfg.useReqHUDirsArray = readNoIndexOrFilePathSpan(ptr, bytesRead);
    cfg.sourceFiles = readNoIndexOrFilePathSpan(ptr, bytesRead);
    cfg.moduleFiles = readNoIndexOrFilePathSpan(ptr, bytesRead);
    cfg.headerUnits = readNoIndexOrFilePathSpan(ptr, bytesRead);
    cfg.buildCacheFilesDirPath = readNodeIndexOrFilePath(ptr, bytesRead);
    return cfg;
}

ConfigCache::Link readLinkConfigCache(const char *ptr, uint32_t &bytesRead)
{
    ConfigCache::Link ln;
    ln.reqLibraryDirsArray = readNoIndexOrFilePathSpan(ptr, bytesRead);
    ln.useReqLibraryDirsArray = readNoIndexOrFilePathSpan(ptr, bytesRead);
    ln.outputFileNode = readNodeIndexOrFilePath(ptr, bytesRead);
    ln.buildCacheFilesDirPath = readNodeIndexOrFilePath(ptr, bytesRead);
    return ln;
}

BuildCache::Cpp::SourceFile readSourceFileBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::SourceFile sf;
    sf.fullPath = readNodeIndexOrFilePath(ptr, bytesRead);
    sf.compileCommandWithTool = readCCOrHash(ptr, bytesRead);
    sf.headerFiles = readNoIndexOrFilePathSpan(ptr, bytesRead);
    return sf;
}

span<BuildCache::Cpp::SourceFile> readSourceFilesBuildCacheSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(BuildCache::Cpp::SourceFile);
    return {(BuildCache::Cpp::SourceFile *)(ptr + offset), count};
}

BuildCache::Cpp::ModuleFileCache::SmRules::SingleHeaderUnitDep
readSingleHeaderUnitDep(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFileCache::SmRules::SingleHeaderUnitDep huDep;
    huDep.fullPath = readNodeIndexOrFilePath(ptr, bytesRead);
    huDep.angle = readBool(ptr, bytesRead);
    huDep.targetIndex = readUint32(ptr, bytesRead);
    huDep.myIndex = readUint32(ptr, bytesRead);
    return huDep;
}

span<ModuleFileCache::SmRules::SingleHeaderUnitDep> readSingleHeaderUnitDepSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(ModuleFileCache::SmRules::SingleHeaderUnitDep);
    return {(ModuleFileCache::SmRules::SingleHeaderUnitDep *)(ptr + offset), count};
}

BuildCache::Cpp::ModuleFileCache::SmRules::SingleModuleDep
readSingleModuleDep(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFileCache::SmRules::SingleModuleDep d;
    d.fullPath = readNodeIndexOrFilePath(ptr, bytesRead);
    d.logicalName = string(readStringView(ptr, bytesRead));
    return d;
}

span<ModuleFileCache::SmRules::SingleModuleDep> readSingleModuleDepSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(ModuleFileCache::SmRules::SingleModuleDep);
    return {(ModuleFileCache::SmRules::SingleModuleDep *)(ptr + offset), count};
}

BuildCache::Cpp::ModuleFileCache::SmRules readSMRules(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFileCache::SmRules rules;
    rules.exportName = string(readStringView(ptr, bytesRead));
    rules.isInterface = readBool(ptr, bytesRead);
    rules.headerUnitArray = readSingleHeaderUnitDepSpan(ptr, bytesRead);
    rules.moduleArray = readSingleModuleDepSpan(ptr, bytesRead);
    return rules;
}

BuildCache::Cpp::ModuleFileCache readModuleFileBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFileCache mf;
    mf.srcFile = readSourceFileBuildCache(ptr, bytesRead);
    mf.headerFiles = readNoIndexOrFilePathSpan(ptr, bytesRead);
    mf.smRules = readSMRules(ptr, bytesRead);
    mf.compileCommandWithTool = readCCOrHash(ptr, bytesRead);
    return mf;
}

span<ModuleFileCache> readModuleFilesBuildCacheSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(ModuleFileCache);
    return {(ModuleFileCache *)(ptr + offset), count};
}

BuildCache::Cpp readCppBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp bc;
    bc.sourceFiles = readSourceFilesBuildCacheSpan(ptr, bytesRead);
    bc.moduleFiles = readModuleFilesBuildCacheSpan(ptr, bytesRead);
    bc.headerUnits = readModuleFilesBuildCacheSpan(ptr, bytesRead);
    return bc;
}

BuildCache::Link readLinkBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Link ln;
    ln.commandWithoutArgumentsWithTools = readCCOrHash(ptr, bytesRead);
    ln.objectFiles = readNoIndexOrFilePathSpan(ptr, bytesRead);
    return ln;
}

void writeBool(vector<char> &buffer, const bool &value)
{
    buffer.emplace_back(static_cast<char>(value));
}

void writeUint32(vector<char> &buffer, const uint32_t &data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(data));
}

void writeStringView(vector<char> &buffer, const string_view &data)
{
    writeUint32(buffer, static_cast<uint32_t>(data.size()));
    buffer.insert(buffer.end(), data.begin(), data.end());
}

void writeNodeIndexOrFilePath(vector<char> &buffer, const NodeIndexOrFilePath &value)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    writeUint32(buffer, value.index);
#else
    writeStringView(buffer, value.filePath);
#endif
}

void writeCCOrHash(vector<char> &buffer, const CCOrHash &value)
{
#ifdef USE_COMMAND_HASH
    writeUint32(buffer, value.hash);
#else
    writeStringView(buffer, value.compilerCommand);
#endif
}

void writeNodeIndexOrFilePathSpan(vector<char> &buffer, span<NodeIndexOrFilePath> array)
{
    writeUint32(buffer, static_cast<uint32_t>(array.size()));
    for (auto &e : array)
    {
        writeNodeIndexOrFilePath(buffer, e);
    }
}

void writeCppConfigCache(vector<char> &buffer, const ConfigCache::Cpp &data)
{
    writeNodeIndexOrFilePathSpan(buffer, data.reqInclsArray);
    writeNodeIndexOrFilePathSpan(buffer, data.useReqInclsArray);
    writeNodeIndexOrFilePathSpan(buffer, data.reqHUDirsArray);
    writeNodeIndexOrFilePathSpan(buffer, data.useReqHUDirsArray);
    writeNodeIndexOrFilePathSpan(buffer, data.sourceFiles);
    writeNodeIndexOrFilePathSpan(buffer, data.moduleFiles);
    writeNodeIndexOrFilePathSpan(buffer, data.headerUnits);
    writeNodeIndexOrFilePath(buffer, data.buildCacheFilesDirPath);
}

void writeLinkConfigCache(vector<char> &buffer, const ConfigCache::Link &data)
{
    writeNodeIndexOrFilePathSpan(buffer, data.reqLibraryDirsArray);
    writeNodeIndexOrFilePathSpan(buffer, data.useReqLibraryDirsArray);
    writeNodeIndexOrFilePath(buffer, data.outputFileNode);
    writeNodeIndexOrFilePath(buffer, data.buildCacheFilesDirPath);
}

void writeSourceFileBuildCache(vector<char> &buffer, const BuildCache::Cpp::SourceFile &data)
{
    writeNodeIndexOrFilePath(buffer, data.fullPath);
    writeCCOrHash(buffer, data.compileCommandWithTool);
    writeNodeIndexOrFilePathSpan(buffer, data.headerFiles);
}

void writeSourceFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::SourceFile> &data)
{
    writeUint32(buffer, static_cast<uint32_t>(data.size()));
    for (auto &e : data)
    {
        writeSourceFileBuildCache(buffer, e);
    }
}

void writeSingleHeaderUnitDep(vector<char> &buffer,
                              const BuildCache::Cpp::ModuleFileCache::SmRules::SingleHeaderUnitDep &data)
{
    writeNodeIndexOrFilePath(buffer, data.fullPath);
    writeBool(buffer, data.angle);
    writeUint32(buffer, data.targetIndex);
    writeUint32(buffer, data.myIndex);
}

void writeSingleModuleDep(vector<char> &buffer, const BuildCache::Cpp::ModuleFileCache::SmRules::SingleModuleDep &data)
{
    writeNodeIndexOrFilePath(buffer, data.fullPath);
    writeStringView(buffer, data.logicalName);
}

void writeSMRules(vector<char> &buffer, const BuildCache::Cpp::ModuleFileCache::SmRules &data)
{
    writeStringView(buffer, data.exportName);
    writeBool(buffer, data.isInterface);

    writeUint32(buffer, static_cast<uint32_t>(data.headerUnitArray.size()));
    for (auto &h : data.headerUnitArray)
    {
        writeSingleHeaderUnitDep(buffer, h);
    }

    writeUint32(buffer, static_cast<uint32_t>(data.moduleArray.size()));
    for (auto &m : data.moduleArray)
    {
        writeSingleModuleDep(buffer, m);
    }
}

void writeModuleFileBuildCache(vector<char> &buffer, const BuildCache::Cpp::ModuleFileCache &data)
{
    writeSourceFileBuildCache(buffer, data.srcFile);
    writeNodeIndexOrFilePathSpan(buffer, data.headerFiles);
    writeSMRules(buffer, data.smRules);
    writeCCOrHash(buffer, data.compileCommandWithTool);
}

void writeModuleFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::ModuleFileCache> &data)
{
    writeUint32(buffer, static_cast<uint32_t>(data.size()));
    for (auto &e : data)
    {
        writeModuleFileBuildCache(buffer, e);
    }
}

void writeCppBuildCache(vector<char> &buffer, const BuildCache::Cpp &data)
{
    writeSourceFileBuildCacheSpan(buffer, data.sourceFiles);
    writeModuleFileBuildCacheSpan(buffer, data.moduleFiles);
    writeModuleFileBuildCacheSpan(buffer, data.headerUnits);
}

void writeLinkBuildCache(vector<char> &buffer, const BuildCache::Link &data)
{
    writeCCOrHash(buffer, data.commandWithoutArgumentsWithTools);
    writeNodeIndexOrFilePathSpan(buffer, data.objectFiles);
}
