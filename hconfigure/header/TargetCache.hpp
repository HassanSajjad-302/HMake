/// \file
/// Classes defined here are used for build-cache and config-cache reading/writing.

#ifndef TARGETCACHE_HPP
#define TARGETCACHE_HPP

#include "BuildSystemFunctions.hpp"
#include "gtl/include/gtl/phmap.hpp"

using gtl::flat_hash_map;

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
    virtual bool writeBuildCache(string &buffer);
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

struct BuildCache
{
    struct Cpp
    {
        struct SourceFile
        {
            Node *node;
            uint64_t compileCommand;
            uint64_t launchTime; // process-launch time
            vector<Node *> headerFiles;
            void serialize(string &buffer) const;
            void deserialize(const char *ptr, uint32_t &bytesRead);
        };

        struct ModuleFile
        {

            struct SingleDep
            {
                Node *node;
                uint64_t compileCommand;
                uint32_t targetIndex{};
                uint32_t myIndex{};
                void serialize(string &buffer) const;
                void deserialize(const char *ptr, uint32_t &bytesRead);
            };

            struct SingleHeaderUnitDep : SingleDep
            {
            };

            struct SingleModuleDep : SingleDep
            {
            };

            SourceFile srcFile;
            vector<SingleHeaderUnitDep> headerUnitArray;
            vector<SingleModuleDep> moduleArray;
            bool headerStatusChanged;
            void serialize(string &buffer) const;
            void deserialize(const char *ptr, uint32_t &bytesRead);
        };

        vector<SourceFile> srcFiles;
        vector<ModuleFile> modFiles;
        vector<ModuleFile> imodFiles;
        vector<ModuleFile> headerUnits;
        void serialize(string &buffer) const;
        void deserialize(uint32_t targetCacheIndex);
    };

    struct Link
    {
        uint64_t commandWithoutArgumentsWithTools;
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

void writeBool(string &buffer, const bool &value);
void writeUint8(string &buffer, const uint8_t &data);
void writeUint32(string &buffer, uint32_t data);
void writeUint64(string &buffer, uint64_t data);
void writeStringView(string &buffer, const string_view &data);
void writeNode(string &buffer, const Node *node);
void writeNodeVector(string &buffer, const vector<Node *> &array);


// Used in CppMod
inline void writeBool(std::pmr::string &buffer, const bool &value)
{
    buffer.push_back(value);
}

inline void writeUint8(std::pmr::string &buffer, const uint8_t &data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

inline void writeUint32(std::pmr::string &buffer, const uint32_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

inline void writeUint64(std::pmr::string &buffer, const uint64_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

inline void writeStringView(std::pmr::string &buffer, const string_view &data)
{
    writeUint32(buffer, data.size());
    buffer.append(data.begin(), data.end());
}

#endif // TARGETCACHE_HPP
