#ifndef TARGETCACHE_HPP
#define TARGETCACHE_HPP

#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "SpecialNodes.hpp";
#else
#include "BuildSystemFunctions.hpp"
#include "SpecialNodes.hpp"
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#endif

using phmap::flat_hash_map;

struct ConfigCacheTarget
{
    // string will have 4 byte size instead of 8 byte size.
    string_view name;
    span<char> configCache;
    span<char> buildCache;
};

inline vector<ConfigCacheTarget> configCacheTargets;
inline flat_hash_map<string, uint32_t> nameToIndexMap;

struct TargetCache
{
    /// Needed to address in configCacheTargets;
    uint32_t targetCacheIndex = -1;
    explicit TargetCache(const string &name);
};

/// Stores either compile-commands or its based on the USE_COMMAND_HASH CMake configuration macro.
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
        struct InclNode
        {
            Node *node;
            bool isStandard = false;
            bool ignoreHeaderDeps = false;
        };
        span<InclNode> reqInclsArray;
        span<InclNode> useReqInclsArray;
        span<Node *> reqHUDirsArray;
        span<Node *> useReqHUDirsArray;
        span<Node *> sourceFiles;
        span<Node *> moduleFiles;
        span<Node *> headerUnits;
        Node *buildCacheFilesDirPath;
    };

    struct Link
    {
        span<Node *> reqLibraryDirsArray;
        span<Node *> useReqLibraryDirsArray;
        Node *outputFileNode;
        Node *buildCacheFilesDirPath;
    };
};

struct BuildCache
{
    struct Cpp
    {
        struct SourceFile
        {
            Node *fullPath;
            CCOrHash compileCommandWithTool;
            span<Node *> headerFiles;
            void initialize(const char *ptr, uint32_t &bytesRead);
        };

        struct ModuleFile
        {
            struct SmRules
            {
                struct SingleHeaderUnitDep
                {
                    Node *fullPath;
                    bool angle{};
                    uint32_t targetIndex{};
                    uint32_t myIndex{};
                    void initialize(const char *ptr, uint32_t &bytesRead);
                };

                struct SingleModuleDep
                {
                    Node *fullPath;
                    string_view logicalName;
                    void initialize(const char *ptr, uint32_t &bytesRead);
                };

                string_view exportName;
                bool isInterface{};
                span<SingleHeaderUnitDep> headerUnitArray;
                span<SingleModuleDep> moduleArray;
                void initialize(const char *ptr, uint32_t &bytesRead);
            };

            SourceFile srcFile;
            SmRules smRules;
            CCOrHash compileCommandWithTool;
            void initialize(const char *ptr, uint32_t &bytesRead);
        };

        span<SourceFile> srcFiles;
        span<ModuleFile> modFiles;
        span<ModuleFile> headerUnits;
        void initialize(uint32_t targetCacheIndex);
        span<SourceFile> readSourceFilesBuildCacheSpan(const char *ptr, uint32_t &bytesRead);
    };

    struct Link
    {
        CCOrHash commandWithoutArgumentsWithTools;
        span<Node *> objectFiles;
    };
};

using ModuleFileCache = BuildCache::Cpp::ModuleFile;

bool readBool(const char *ptr, uint32_t &bytesRead);
uint32_t readUint32(const char *ptr, uint32_t &bytesRead);
string_view readStringView(const char *ptr, uint32_t &bytesRead);
Node *readNode(const char *ptr, uint32_t &bytesRead);
CCOrHash readCCOrHash(const char *ptr, uint32_t &bytesRead);
span<Node *> readNodeSpan(const char *ptr, uint32_t &bytesRead);
ConfigCache::Cpp readCppConfigCache(const char *ptr, uint32_t &bytesRead);
ConfigCache::Link readLinkConfigCache(const char *ptr, uint32_t &bytesRead);
BuildCache::Cpp::SourceFile readSourceFileBuildCache(const char *ptr, uint32_t &bytesRead);
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
void writeNode(vector<char> &buffer, const Node *value);
void writeCCOrHash(vector<char> &buffer, const CCOrHash &value);
void writeNodeSpan(vector<char> &buffer, span<Node *> array);
void writeCppConfigCache(vector<char> &buffer, const ConfigCache::Cpp &data);
void writeLinkConfigCache(vector<char> &buffer, const ConfigCache::Link &data);
void writeSourceFileBuildCache(vector<char> &buffer, const BuildCache::Cpp::SourceFile &data);
void writeSourceFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::SourceFile> &data);
void writeSingleHeaderUnitDep(vector<char> &buffer, const ModuleFileCache::SmRules::SingleHeaderUnitDep &data);
void writeSingleModuleDep(vector<char> &buffer, const ModuleFileCache::SmRules::SingleModuleDep &data);
void writeSMRules(vector<char> &buffer, const ModuleFileCache::SmRules &data);
void writeModuleFileBuildCache(vector<char> &buffer, const ModuleFileCache &data);
void writeModuleFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::ModuleFile> &data);
void writeCppBuildCache(vector<char> &buffer, const BuildCache::Cpp &data);
void writeLinkBuildCache(vector<char> &buffer, const BuildCache::Link &data);

template <typename T> void writeIncDirsAtConfigTime(vector<char> *buffer, const vector<T> &include)
{
    buffer->reserve(include.size() * sizeof(T) + sizeof(uint32_t));
    writeUint32(buffer, include.size());
    for (auto &elem : include)
    {
        const InclNode &inclNode = getNode(elem);
        writeNode(*buffer, include.node);
        writeBool(*buffer, inclNode.isStandard);
        writeBool(*buffer, inclNode.ignoreHeaderDeps);
        if constexpr (std::is_same_v<T, InclNodeTargetMap>)
        {
            auto &headerUnitNode = static_cast<const HeaderUnitNode &>(inclNode);

            writeBool(*buffer, headerUnitNode.targetCacheIndex);
            writeBool(*buffer, headerUnitNode.headerUnitIndex);
        }
    }
}

template <typename T> void readInclDirsAtBuildTime(const char *ptr, uint32_t &bytesRead, vector<T> &include)
{
    const uint32_t reserveSize = readUint32(ptr + bytesRead, bytesRead);
    include.reserve(reserveSize * sizeof(T) + sizeof(uint32_t));
    for (uint32_t i = 0; i < include.size(); ++i)
    {
        Node *node = readNode(ptr + bytesRead, bytesRead);
        bool isStandard = readBool(ptr + bytesRead, bytesRead);
        bool ignoreHeaderDeps = readBool(ptr + bytesRead, bytesRead);
        if constexpr (std::is_same_v<T, InclNodeTargetMap>)
        {
            bool targetCacheIndex = readBool(ptr + bytesRead, bytesRead);
            bool headerUnitIndex = readBool(ptr + bytesRead, bytesRead);
            include.emplace_back(node, isStandard, ignoreHeaderDeps, targetCacheIndex, headerUnitIndex);
        }
        else
        {
            include.emplace_back(node, isStandard, ignoreHeaderDeps);
        }
    }
}
#endif // TARGETCACHE_HPP
