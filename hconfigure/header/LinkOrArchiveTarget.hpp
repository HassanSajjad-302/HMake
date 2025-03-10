#ifndef HMAKE_LINKORARCHIVETARGET_HPP
#define HMAKE_LINKORARCHIVETARGET_HPP
#ifdef USE_HEADER_UNITS
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "HashedCommand";
import "Node.hpp";
import "RunCommand.hpp";
import "PrebuiltLinkOrArchiveTarget.hpp";
import "Utilities.hpp";
import <stack>;
#else
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "HashedCommand.hpp"
#include "PrebuiltLinkOrArchiveTarget.hpp"
#include "RunCommand.hpp"
#include <stack>
#endif
#include <ObjectFile.hpp>

using std::stack, std::filesystem::create_directories, std::shared_ptr;

class LinkOrArchiveTarget : public PrebuiltLinkOrArchiveTarget
{
    using BaseType = PrebuiltLinkOrArchiveTarget;

  public:
    string requirementLinkerFlags;
    string_view linkOrArchiveCommandWithoutTargets;
    string linkOrArchiveCommandWithTargets;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    HashedCommand commandWithoutTargetsWithTool;

    vector<const ObjectFile *> objectFiles;
    vector<PrebuiltLinkOrArchiveTarget *> dllsToBeCopied;
    // Needed for pdb files.
    Node *buildCacheFilesDirPathNode = nullptr;

    bool archiving = false;
    bool archived = false;

    void makeBuildCacheFilesDirPathAtConfigTime(string buildCacheFilesDirPath);
    LinkOrArchiveTarget(Configuration &config_, const string &name_, TargetType targetType);
    LinkOrArchiveTarget(Configuration &config_, bool buildExplicit, const string &name_, TargetType targetType);
    LinkOrArchiveTarget(Configuration &config_, const string &buildCacheFileDirPath_, const string &name_,
                        TargetType targetType);
    LinkOrArchiveTarget(Configuration &config_, const string &buildCacheFileDirPath_, bool buildExplicit,
                        const string &name_, TargetType targetType);

    virtual string getLinkOrArchiveCommandWithoutTargets();
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

bool operator<(const LinkOrArchiveTarget &lhs, const LinkOrArchiveTarget &rhs);

template <typename T> bool LinkOrArchiveTarget::evaluate(T property) const
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

#endif // HMAKE_LINKORARCHIVETARGET_HPP
