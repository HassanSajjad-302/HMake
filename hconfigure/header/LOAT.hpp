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

    bool recurrentCall = false;

  public:
    BuildCache::Link linkBuildCache;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    HashedCommand commandWithoutTargetsWithTool;

    vector<const ObjectFile *> objectFiles;
    vector<PLOAT *> dllsToBeCopied;
    // Needed for pdb files.
    Node *myBuildDir = nullptr;

    bool archiving = false;
    bool archived = false;
    bool recurent = false;

    void makeBuildCacheFilesDirPathAtConfigTime();
    LOAT(Configuration &config_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, bool buildExplicit, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, Node *myBuildDir_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, Node *myBuildDir_, bool buildExplicit, const string &name_, TargetType targetType);
    void setOutputName(string str);

    BTargetType getBTargetType() const override;

    void setFileStatus();
    void completeRoundOne() override;
    bool writeBuildCache(string &buffer) override;
    void writeCacheAtConfigureTime();
    void readCacheAtBuildTime();

    string getPrintName() const override;
    void setLinkOrArchiveCommands(std::pmr::string &linkWithTargets);
    template <typename T> bool evaluate(T property) const;
    bool isEventRegistered(Builder &builder) override;
    bool isEventCompleted(Builder &builder, string_view) override;
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
        static_assert(false);
    }
}

#endif // HMAKE_LOAT_HPP
