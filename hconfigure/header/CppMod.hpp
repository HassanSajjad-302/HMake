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
    uint32_t indexInBuildCache = -1;
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
    /// build-cache at indexInBuildCache, if this was updated.
    virtual void updateBuildCache();
};

bool operator<(const CppSrc &lhs, const CppSrc &rhs);

enum class SM_REQUIRE_TYPE : uint8_t
{
    NOT_assignED = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    HEADER_UNIT = 3,
};

enum class SM_FILE_TYPE : uint8_t
{
    NOT_ASSIGNED = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    PARTITION_IMPLEMENTATION = 3,
    HEADER_UNIT = 4,
    PRIMARY_IMPLEMENTATION = 5,
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

    ///
    Node *interfaceNode;
    CppMod *waitingFor = nullptr;

    N2978::IPCManagerBS *ipcManager;

    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;

    // following 2 only used at configure time.
    bool isReqDep = false;
    bool isUseReqDep = false;
    bool compileCommandChanged = false;
    bool firstMessageSent = false;

    // Whether to set ignoreHeaderDeps to true for HeaderUnits which come from such Node includes for which
    // ignoreHeaderDeps is true
    inline static bool ignoreHeaderDepsForIgnoreHeaderUnits = true;

    inline static thread_local vector<string_view> includeNames;
    CppMod(CppTarget *target_, const Node *node_);
    void initializeBuildCache(BuildCache::Cpp::ModuleFile &modCache, uint32_t index);
    void makeAndSendBTCModule(CppMod &mod);
    void makeAndSendBTCNonModule(CppMod &hu);
    void returnAfterCompleting();
    void duplicateHeaderFileOrUnitError(const string &headerName, struct HeaderFileOrUnit &first,
                                        HeaderFileOrUnit &second, CppTarget *firstTarget,
                                        CppTarget *secondTarget) const;
    CppMod *findModule(const string &moduleName) const;
    HeaderFileOrUnit findHeaderFileOrUnit(const string &headerName);
    bool build(Builder &builder);
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    string getOutputFileName() const;
    BTargetType getBTargetType() const override;
    void updateBuildCache() override;
    string getCompileCommand() const;
    void setFileStatusAndPopulateAllDependencies();
};

#endif // HMAKE_CPPMOD_HPP
