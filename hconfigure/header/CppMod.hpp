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
    /// header-files discovered during the build. MSVC can output duplicate files. Also, while compiling modules /// we
    /// can get same header-file from multiple header-unit or module-deps. So, a set is used to remove duplicates to
    /// keep the build-cache small.
    flat_hash_set<Node *> headerFiles;

    /// The back pointer to the CppTarget owning this in srcFileDeps.
    CppTarget *target;

    /// Node pointer to the source-file
    const Node *node;

    ///
    uint32_t indexInBuildCache = -1;
    CppSrc(CppTarget *target_, const Node *node_);

  protected:
    CppSrc(CppTarget *target_, const Node *node_, bool add0, bool add1);

  public:
    string getPrintName() const override;
    void initializeBuildCache(uint32_t index);
    string getCompileCommand() const;
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    bool ignoreHeaderFile(string_view child) const;
    void parseDepsFromMSVCTextOutput(string &output, bool isClang);
    void parseDepsFromGCCDepsOutput();
    void parseHeaderDeps(string &output);
    void setCppSrcFileStatus();
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

struct CppMod : CppSrc
{
    BuildCache::Cpp::ModuleFile::SmRules smRulesCache;

    // Those header-files which are #included in this module or hu. These are initialized from config-cache as big-hu
    // have these. While Source::headerFiles have all the header-files of ours and our dependencies for accurate
    // rebuilds.
    flat_hash_map<string, Node *> composingHeaders;
    flat_hash_set<CppMod *> allCppModDependencies;

    RunCommand run;
    vector<string> logicalNames;

    vector<Node *> *headerFilesCache;
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
