#ifndef HMAKE_LINKORARCHIVETARGET_HPP
#define HMAKE_LINKORARCHIVETARGET_HPP

#include "SMFile.hpp"

using std::shared_ptr;
class LinkOrArchiveTarget : public CTarget, public BTarget
{
  public:
    class CppSourceTarget *cppSourceTarget;
    shared_ptr<PostBasic> postBasicLinkOrArchive;
    Linker linker;
    Archiver archiver;
    string linkerTransitiveFlags;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    string linkOrArchiveCommand;
    string linkOrArchiveCommandPrintFirstHalf;
    set<string> libraryDirectoriesStandard;
    set<Node *> libraryDirectories;

    set<LinkOrArchiveTarget *> publicLibs;
    set<LinkOrArchiveTarget *> privateLibs;
    set<LinkOrArchiveTarget *> publicPrebuilts;
    set<LinkOrArchiveTarget *> privatePrebuilts;

    string linkCommand;
    string buildCacheFilesDirPath;
    string actualOutputName;

    LinkOrArchiveTarget(string name, TargetType targetType);
    void initializeForBuild() override;
    void updateBTarget() override;
    void printMutexLockRoutine() override;
    void setJson() override;
    BTarget *getBTarget() override;
    PostBasic Archive();
    PostBasic Link();
    void populateCTargetDependencies() override;
    void addPrivatePropertiesToPublicProperties() override;
    void pruneAndSaveBuildCache(bool successful);
    void setLinkOrArchiveCommandAndPrint();
    string &getLinkOrArchiveCommand();
    string &getLinkOrArchiveCommandPrintFirstHalf();
    void checkForPreBuiltAndCacheDir() override;
    void checkForRelinkPrebuiltDependencies();
    string outputName;
    string outputDirectory;
    string publicLinkerFlags;
};

#endif // HMAKE_LINKORARCHIVETARGET_HPP
