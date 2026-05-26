/// \file
/// Defines `LOAT` (link-or-archive target) for executables, shared libraries, and static libraries.

#ifndef HMAKE_LOAT_HPP
#define HMAKE_LOAT_HPP
#include "Features.hpp"
#include "ObjectFile.hpp"
#include "PLOAT.hpp"

#include <stack>

using std::stack, std::filesystem::create_directories, std::shared_ptr;

/// Link-or-archive target: links object files into an executable or shared library, or archives them into a static lib.
class LOAT : public PLOAT
{
    using BaseType = PLOAT;

  public:
    /// Object files collected from `ObjectFileProducer` dependencies in `completeRoundOne()`.
    vector<const ObjectFile *> objectFiles;

    /// Shared libraries that must be copied beside the executable on Windows (`CopyDLLToExeDirOnNTOs::YES`).
    vector<PLOAT *> dllsToBeCopied;

    /// Build output directory for this target (object files, PDBs, etc.). Created at configure-time if unset.
    Node *myBuildDir = nullptr;

    /// Hash of the link/archive command template without object-file paths (intended for incremental caching).
    uint64_t commandWithoutTargets;

    void makeBuildCacheFilesDirPathAtConfigTime();
    LOAT(Configuration &config_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, bool buildExplicit, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, Node *myBuildDir_, const string &name_, TargetType targetType);
    LOAT(Configuration &config_, Node *myBuildDir_, bool buildExplicit, const string &name_, TargetType targetType);
    void setOutputName(string str);

    void setFileStatus() override;
    void completeRoundOne() override;

    string getPrintName() const override;
    void setLinkOrArchiveCommands(std::pmr::string &linkWithTargets, bool returnWithoutTargets) const;
    template <typename T> bool evaluate(T property) const;
    bool isEventRegistered(Builder &builder) override;
    bool isEventCompleted(Builder &builder, string_view) override;
    void writeConfigCacheAtConfigTime(string &buffer) override;
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
