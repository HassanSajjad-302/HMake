/// \file
/// This file defines CppSrc and CppMod

#ifndef HMAKE_CPPMOD_HPP
#define HMAKE_CPPMOD_HPP

#include "IPCManagerBS.hpp"
#include "ObjectFile.hpp"
#include "RunCommand.hpp"
#include "TargetCache.hpp"
#include "parallel-hashmap/parallel_hashmap/btree.h"
#include <atomic>
#include <filesystem>
#include <list>
#include <utility>
#include <vector>

using std::vector, std::filesystem::path, std::pair, std::list, std::shared_ptr, std::atomic, std::atomic_flag,
    phmap::btree_set, phmap::flat_hash_map;

class CppTarget;
class CppSrc;
struct HeaderFileOrUnit;

struct CompareCppSrc
{
    using is_transparent = void; // for example with void,
                                 // but could be int or struct CanSearchOnId;
    bool operator()(const CppSrc &lhs, const CppSrc &rhs) const;
    bool operator()(const Node *lhs, const CppSrc &rhs) const;
    bool operator()(const CppSrc &lhs, const Node *rhs) const;
};

/// Responsible for compiling C++ source-files. Inherits from ObjectFile which has Node pinter ObjectFile::objectNode
class CppSrc : public ObjectFile
{
  public:
    /// header-files discovered during the build. MSVC can output duplicate files. Also, while compiling modules we can
    /// get same header-file from multiple header-unit or module-deps. So, a set is used to remove duplicates to keep
    /// the build-cache small.
    flat_hash_set<Node *> headerFiles;

    /// The back pointer to the CppTarget owning this in srcFileDeps.
    CppTarget *target;

    /// Node pointer to the source-file
    const Node *node;

    /// Index in BuildCache::Cpp::srcFiles or BuildCache::Cpp::modFiles or BuildCache::Cpp::imodFiles or
    /// BuildCache::Cpp::headerUnits
    uint32_t myBuildCacheIndex = -1;
    CppSrc(CppTarget *target_, const Node *node_);
    string getPrintName() const override;
    /// This function compares compile-command with build-cache and also set Node::toBeChecked of source-node,
    /// object-node and header-files.
    void initializeBuildCache(uint32_t index);
    string getCompileCommand() const;
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    bool ignoreHeaderFile(string_view child) const;
    /// MSVC prints header-files with the compilation output. This function parses them out from that output.
    void parseDepsFromMSVCTextOutput(string &output, bool isClang);
    /// GCC outputs header-files in a .d file. This function parses that
    void parseDepsFromGCCDepsOutput();
    /// Calls either of parseDepsFromGCCDepsOutput or parseDepsFromMSVCTextOutput
    void parseHeaderDeps(string &output);
    /// This compares lastWrite of source-node with object-node and header-files
    void setCppSrcFileStatus();
    /// Called at the end or in the signal-handler when the build-cache is being written. This function will update the
    /// build-cache at myBuildCacheIndex, if this was updated.
    virtual void updateBuildCache();
};

bool operator<(const CppSrc &lhs, const CppSrc &rhs);

enum class SM_FILE_TYPE : uint8_t
{
    PRIMARY_EXPORT = 0,
    PARTITION_EXPORT = 1,
    HEADER_UNIT = 2,
    PRIMARY_IMPLEMENTATION = 3,
};

struct CppMod final : CppSrc
{
    /// Those header-files which are #included in this module or hu. These are initialized from config-cache as big-hu
    /// have these. While Source::headerFiles have all the header-files of ours and our dependencies for accurate
    /// rebuilds.
    flat_hash_map<string, Node *> composingHeaders;

    /// All dependencies of this module or hu. includes both header-units and modules.
    flat_hash_set<CppMod *> allCppModDependencies;

    /// RunCommand::startProcess is called in updateBTarget and then RunCommand::endProcess is called in CppMod::build
    /// when CTB::LAST_MESSAGE is received.
    RunCommand run;

    /// A header-unit can be found by more than 1 logicalNames. Like "std/header1.hpp" and "./header1.hpp". Also, in big
    /// header-units case a big header-unit can be found by any of its composing headers. All composing includes are
    /// added in following array and is sent with requested hu to keep the number of messages minimal. In case of
    /// module, logicalNames[0] is the exportName of the module.
    vector<string> logicalNames;

    /// BMI node for header-units and module interface files. Initialized in CppTarget::readConfigCache.
    Node *interfaceNode;

    /// CppMod::updateBTarget will initialize this and then will call receiveMessage to learn about any dependencies the
    /// compiler require.
    N2978::IPCManagerBS *ipcManager;

    /// The dependency module or hu we are waiting on to compile.
    CppMod *waitingFor = nullptr;

    /// Points to one of the arrays of CppTarget::cppBuildCache of the owning CppTarget.
    BuildCache::Cpp::ModuleFile *myBuildCache;

    SM_FILE_TYPE type;

    /// Following is used only at config-time. Describes whether hu is private hu of the CppTarget.
    bool isReqHu = false;

    /// Following is used only at config-time. Describes whether hu is interface hu of the CppTarget.
    bool isUseReqHu = false;

    /// Composing headers are only sent with the first message. This keeps tracks of that
    bool firstMessageSent = false;

    bool compileCommandChanged = false;

    CppMod(CppTarget *target_, const Node *node_);

    /// Call by CppTarget. In this we set Node::toBeChecked of different Nodes like header-files, interface-node and
    /// objectNode.
    void initializeBuildCache(BuildCache::Cpp::ModuleFile &modCache, uint32_t index);

    /// Called to send the N2978::BTCModule corresponding to a module CppMod whose compilation just completed
    void makeAndSendBTCModule(CppMod &mod);

    /// Called to send the N2978::BTCNonModule corresponding to a hu CppMod whose compilation just completed
    void makeAndSendBTCNonModule(CppMod &hu);

    /// Looks for the received module-name in just CppTarget::imodNames if module-name is of partition export. Looks in
    /// CppTarget::imodNames of dependencies CppTarget as well if it is a primary export.
    CppMod *findModule(const string &moduleName) const;

    /// Looks for the received header-name in CppTarget::reqHeaderNameMapping and CppTarget::useReqHeaderNameMapping of
    /// the dependency CppTargets. While compiling the big-hu, a request for any composing-header will map to the big-hu
    /// in these lookup tables. This case is specially handled in the following function.
    HeaderFileOrUnit findHeaderFileOrUnit(const string &headerName) const;

    /// CppMod::updateBTarget function is responsible for launching the IPC server and the compilation process. This
    /// function interacts with this server and manages the build.
    /// \returns true if we are waiting on a dependency, false if we have completed the compilation.
    bool build(Builder &builder);

    /// Launches IPC server and the compilation process if the module or hu needs to be updated. Sets \p isComplete to
    /// true if we are waiting for a module or hu to get compiled first.
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;

    /// \returns BTargetType::CPPMOD
    BTargetType getBTargetType() const override;

    /// Called at the end or in the signal-handler when the build-cache is being written. This function will update the
    /// build-cache at myBuildCacheIndex, if this was updated.
    void updateBuildCache() override;

    string getCompileCommand() const;

    /// Checks whether this needs to be updated and sets round0 RealBTarget::updateStatus to UpdateStatus::NEEDS_UPDATE.
    /// Otherwise, populates CppTarget::allCppModDependencies based on myBuildCache. So, if any of our dependents need
    /// to be updated, makeAndSend* functions could send our dependencies with us in a single message.
    void setFileStatusAndPopulateAllDependencies();
};

#endif // HMAKE_CPPMOD_HPP
