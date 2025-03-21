#ifndef HMAKE_LOAT_HPP
#define HMAKE_LOAT_HPP
#ifdef USE_HEADER_UNITS
import "Features.hpp";
import "HashedCommand";
import "ObjectFile.hpp";
import "PLOAT.hpp";
import "RunCommand.hpp";
import <stack>;
#else
#include "Features.hpp"
#include "HashedCommand.hpp"
#include "ObjectFile.hpp"
#include "PLOAT.hpp"
#include "RunCommand.hpp"
#include <stack>
#endif

using std::stack, std::filesystem::create_directories, std::shared_ptr;

// LinkOrArchiveTarget
class LOAT : public PLOAT
{
    using BaseType = PLOAT;

  public:
    string reqLinkerFlags;
    string_view linkOrArchiveCommandWithoutTargets;
    string linkOrArchiveCommandWithTargets;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    HashedCommand commandWithoutTargetsWithTool;

    vector<const ObjectFile *> objectFiles;
    vector<PLOAT *> dllsToBeCopied;
    // Needed for pdb files.
    Node *buildCacheFilesDirPathNode = nullptr;

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
    void updateBTarget(Builder &builder, unsigned short round) override;
    void writeTargetConfigCacheAtConfigureTime();
    void readConfigCacheAtBuildTime();

    string getTarjanNodeName() const override;
    RunCommand Archive();
    RunCommand Link();
    void setLinkOrArchiveCommands();
    string getLinkOrArchiveCommandPrint();
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
