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
    class TargetCache *targetCache;
    // string will have 4 byte size instead of 8 byte size.
    span<char> name;
    // At config-time the conif-cache and build-cache are written once using the following variables.
    // While at build-time, the build-cache is written multiple times to save progress as more files are built so it is
    // written using the TargetCache::updateBuildCache function using the above targetCache pointer. This is done by
    // TargetCacheDiskWriteManager.
    span<char> configCache;
    span<char> buildCache;
};

inline vector<ConfigCacheTarget> configCacheTargets;
inline flat_hash_map<string, uint32_t> nameToIndexMap;

class TargetCache
{
  public:
    /// Needed to address in configCacheTargets;
    uint32_t targetCacheIndex = -1;
    explicit TargetCache(const string &name);
    virtual void updateBuildCache(void *ptr);
    virtual void writeBuildCache(vector<char> &buffer);
};

/// Stores either compile-commands or its based on the USE_COMMAND_HASH CMake configuration macro.
struct CCOrHash
{
#ifdef USE_COMMAND_HASH
    uint32_t hash{};
#else
    string_view hash;
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
            Node *node;
            CCOrHash compileCommandWithTool;
            vector<Node *> headerFiles;
            void serialize(vector<char> &buffer) const;
            void deserialize(const char *ptr, uint32_t &bytesRead);
        };

        struct ModuleFile
        {
            struct SmRules
            {
                struct SingleHeaderUnitDep
                {
                    Node *node;
                    bool angle{};
                    uint32_t targetIndex{};
                    uint32_t myIndex{};
                    void serialize(vector<char> &buffer) const;
                    void deserialize(const char *ptr, uint32_t &bytesRead);
                };

                struct SingleModuleDep
                {
                    Node *fullPath;
                    string_view logicalName;
                    void serialize(vector<char> &buffer) const;
                    void deserialize(const char *ptr, uint32_t &bytesRead);
                };

                string_view exportName;
                bool isInterface{};
                vector<SingleHeaderUnitDep> headerUnitArray;
                vector<SingleModuleDep> moduleArray;
                void serialize(vector<char> &buffer) const;
                void deserialize(const char *ptr, uint32_t &bytesRead);
            };

            SourceFile srcFile;
            SmRules smRules;
            CCOrHash compileCommandWithTool;
            void serialize(vector<char> &buffer) const;
            void deserialize(const char *ptr, uint32_t &bytesRead);
        };

        vector<SourceFile> srcFiles;
        vector<ModuleFile> modFiles;
        vector<ModuleFile> headerUnits;
        void serialize(vector<char> &buffer) const;
        void deserialize(uint32_t targetCacheIndex);
    };

    struct Link
    {
        CCOrHash commandWithoutArgumentsWithTools;
        span<Node *> objectFiles;
    };
};

using ModuleFile = BuildCache::Cpp::ModuleFile;

bool readBool(const char *ptr, uint32_t &bytesRead);
uint32_t readUint32(const char *ptr, uint32_t &bytesRead);
string_view readStringView(const char *ptr, uint32_t &bytesRead);
Node *readHalfNode(const char *ptr, uint32_t &bytesRead);
CCOrHash readCCOrHash(const char *ptr, uint32_t &bytesRead);
span<Node *> readNodeSpan(const char *ptr, uint32_t &bytesRead);
ConfigCache::Cpp readCppConfigCache(const char *ptr, uint32_t &bytesRead);
ConfigCache::Link readLinkConfigCache(const char *ptr, uint32_t &bytesRead);
BuildCache::Link readLinkBuildCache(const char *ptr, uint32_t &bytesRead);

void writeBool(vector<char> &buffer, const bool &value);
void writeUint32(vector<char> &buffer, uint32_t data);
void writeStringView(vector<char> &buffer, const string_view &data);
void writeNode(vector<char> &buffer, const Node *value);
void writeCCOrHash(vector<char> &buffer, const CCOrHash &value);
void writeNodeVector(vector<char> &buffer, const vector<Node *> &array);
void writeCppConfigCache(vector<char> &buffer, const ConfigCache::Cpp &data);
void writeLinkConfigCache(vector<char> &buffer, const ConfigCache::Link &data);
void writeLinkBuildCache(vector<char> &buffer, const BuildCache::Link &data);
#endif // TARGETCACHE_HPP
