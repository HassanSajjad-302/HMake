/// \file
/// Defines `LOAT` (link-or-archive target) for executables, shared libraries, and static libraries.

#ifndef HMAKE_LOAT_HPP
#define HMAKE_LOAT_HPP
#include "Features.hpp"
#include "ObjectFile.hpp"
#include "PLOAT.hpp"

/// Link-or-archive target: links object files into an executable or shared library, or archives them into a static lib.
class LOAT : public PLOAT
{
  public:
    /// Build output directory for this target (object files, PDBs, etc.). Created at configure-time if unset.
    Node *myBuildDir = nullptr;

    void makeBuildCacheFilesDirPathAtConfigTime();
    LOAT(Configuration &config_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, bool buildExplicit, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, Node *myBuildDir_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, Node *myBuildDir_, bool buildExplicit, const string &name_, TargetType targetType);
    void setOutputName(string str);

    void completeRoundOne() override;

    string getPrintName() const override;
    void populateObjectNodes(std::pmr::vector<Node *> &objectNodes) const;
    void setLinkOrArchiveCommands(std::pmr::string &linkWithTargets, bool returnWithoutTargets,
                                  span<Node *> objectNodes = {}) const;
    bool isEventRegistered(Builder &builder) override;
    bool isEventCompleted(Builder &builder, string_view) override;
    void writeConfigCacheAtConfigTime(string &buffer) override;

  private:
    void copyRuntimeDlls() const;
};

#endif // HMAKE_LOAT_HPP
