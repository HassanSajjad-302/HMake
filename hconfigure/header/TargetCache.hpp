/// \file
/// Classes defined here are used for build-cache and config-cache reading/writing.

#ifndef TARGETCACHE_HPP
#define TARGETCACHE_HPP

#include "BuildSystemFunctions.hpp"
#include "parallel-hashmap/parallel_hashmap/phmap.h"

using phmap::flat_hash_map;

/// HMake has 2 different cache that are config-cache and build-cache. config-cache is only read but not written at
/// build-time. By separating config-cache and build-cache, we keep the build-cache smaller and keep the builds faster.
///
/// TargetCache works with FileTargetCache to manage the config-cache and build-cache reading and writing. Any class
/// that wants to store cache in config-cache or build-cache should inherit from this class. Its constructor take a name
/// that must be unique for 2 inherited objects. Otherwise, configure step will fail.
///
/// In config-cache, we store name, then the size of the data and then the data itself.
/// However, in build-cache, we store only the size and the data but not the name. The order is same in both
/// config-cache and build-cache.
///
/// initializeBuildCache() calls first readConfigCache() and then readBuildCache().
///
/// readConfigCache() populates fileTargetCaches and nameToIndexMap containers. Then in buildSpecification() and
/// later, whenever an object of TargetCache is instantiated, in the TargetCache constructor, we use nameToIndexMap
/// container to retrieve the index of FileTargetCache element in fileTargetCaches container. FileTargetCache contains
/// the both configCache and buildCache data that were populated in readConfigCache() and readBuildCache() respectively.
class TargetCache
{
  public:
    /// Address of this element in fileTargetCaches vector
    uint32_t cacheIndex = -1;
    explicit TargetCache(const string &name);
    virtual bool writeBuildCache(vector<char> &buffer);
};

/// Every Target-Cache has representation on both config-cache and build-cache. It could be empty. The order is
/// persistent across builds.
struct FileTargetCache
{
    /// Back-pointer to the targetCache
    TargetCache *targetCache = nullptr;
    string_view name;
    string_view configCache;
    string_view buildCache;
};

inline vector<FileTargetCache> fileTargetCaches;
inline flat_hash_map<string, uint32_t> nameToIndexMap;

/// Stores either compile-commands or its hash based on the USE_COMMAND_HASH CMake configuration macro.
struct CCOrHash
{
#ifdef USE_COMMAND_HASH
    uint64_t hash{};
#else
    string_view hash;
#endif

    void serialize(vector<char> &buffer) const;
    void deserialize(const char *ptr, uint32_t &bytesRead);
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
            struct SingleHeaderUnitDep
            {
                Node *node;
                uint32_t targetIndex{};
                uint32_t myIndex{};
                void serialize(vector<char> &buffer) const;
                void deserialize(const char *ptr, uint32_t &bytesRead);
            };

            struct SingleModuleDep
            {
                Node *node;
                uint32_t targetIndex{};
                uint32_t myIndex{};
                void serialize(vector<char> &buffer) const;
                void deserialize(const char *ptr, uint32_t &bytesRead);
            };

            SourceFile srcFile;
            vector<SingleHeaderUnitDep> headerUnitArray;
            vector<SingleModuleDep> moduleArray;
            bool headerStatusChanged;
            void serialize(vector<char> &buffer) const;
            void deserialize(const char *ptr, uint32_t &bytesRead);
        };

        vector<SourceFile> srcFiles;
        vector<ModuleFile> modFiles;
        vector<ModuleFile> imodFiles;
        vector<ModuleFile> headerUnits;
        void serialize(vector<char> &buffer) const;
        void deserialize(uint32_t targetCacheIndex);
    };

    struct Link
    {
        CCOrHash commandWithoutArgumentsWithTools;
        vector<Node *> objectFiles;
    };
};

using ModuleFile = BuildCache::Cpp::ModuleFile;

bool readBool(const char *ptr, uint32_t &bytesRead);
uint8_t readUint8(const char *ptr, uint32_t &bytesRead);
uint32_t readUint32(const char *ptr, uint32_t &bytesRead);
uint64_t readUint64(const char *ptr, uint32_t &bytesRead);
string_view readStringView(const char *ptr, uint32_t &bytesRead);
Node *readHalfNode(const char *ptr, uint32_t &bytesRead);

void writeBool(vector<char> &buffer, const bool &value);
void writeUint8(vector<char> &buffer, const uint8_t &data);
void writeUint32(vector<char> &buffer, uint32_t data);
void writeUint64(vector<char> &buffer, uint64_t data);
void writeStringView(vector<char> &buffer, const string_view &data);
void writeNode(vector<char> &buffer, const Node *node);
void writeNodeVector(vector<char> &buffer, const vector<Node *> &array);
#endif // TARGETCACHE_HPP
