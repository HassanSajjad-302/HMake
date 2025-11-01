#ifndef HMAKE_LOAT_HPP
#define HMAKE_LOAT_HPP
#include "Features.hpp"
#include "HashedCommand.hpp"
#include "ObjectFile.hpp"
#include "PLOAT.hpp"
#include <stack>

using std::stack, std::filesystem::create_directories, std::shared_ptr;

// LinkOrArchiveTarget
class LOAT : public PLOAT
{
    using BaseType = PLOAT;

  public:
    BuildCache::Link linkBuildCache;
    string reqLinkerFlags;
    string linkOutput;
    string linkWithTargets;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    HashedCommand commandWithoutTargetsWithTool;

    vector<const ObjectFile *> objectFiles;
    vector<PLOAT *> dllsToBeCopied;
    // Needed for pdb files.
    Node *buildCacheFilesDirPathNode = nullptr;

    uint16_t thrIndex;
    bool archiving = false;
    bool archived = false;

    void makeBuildCacheFilesDirPathAtConfigTime(string buildCacheFilesDirPath);
    LOAT(Configuration &config_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, bool buildExplicit, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, const string &buildCacheFileDirPath_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, const string &buildCacheFileDirPath_, bool buildExplicit, const string &name_,
         TargetType targetType);
    void setOutputName(string str);

    BTargetType getBTargetType() const override;

    void setFileStatus();
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    void updateBuildCache(void *ptr, string &outputStr, string &errorStr, bool &buildCacheModified) override;
    void writeBuildCache(vector<char> &buffer) override;
    void writeCacheAtConfigureTime();
    void readCacheAtBuildTime();

    string getPrintName() const override;
    void setLinkOrArchiveCommands();
    template <typename T> bool evaluate(T property) const;
};

bool operator<(const LOAT &lhs, const LOAT &rhs);

template <typename T> bool LOAT::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else
    {
        return config.linkerFeatures.evaluate(property);
    }
}

#endif // HMAKE_LOAT_HPP
