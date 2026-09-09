/// \file
/// Defines `Loat` (link-or-archive target) for executables, shared libraries, and static libraries.

#ifndef HMAKE_LOAT_HPP
#define HMAKE_LOAT_HPP
#include "Features.hpp"
#include "ObjectFile.hpp"
#include "Ploat.hpp"

/// Link-or-archive target: links object files into an executable or shared library, or archives them into a static lib.
class Loat : public Ploat
{
  public:
    /// Build output directory for this target (object files, PDBs, etc.). Created at configure-time if unset.
    Node *myBuildDir = nullptr;

    void makeBuildCacheFilesDirPathAtConfigTime();
    Loat(Configuration &config_, const string &name_, TargetType targetType);
    Loat(Configuration &config_, bool buildExplicit, const string &name_, TargetType targetType);
    Loat(Configuration &config_, Node *myBuildDir_, const string &name_, TargetType targetType);
    Loat(Configuration &config_, Node *myBuildDir_, bool buildExplicit, const string &name_, TargetType targetType);
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
