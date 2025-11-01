#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP

#include "IPCManagerBS.hpp"
#include "ObjectFile.hpp"
#include "RunCommand.hpp"
#include "SpecialNodes.hpp"
#include "TargetCache.hpp"
#include "parallel-hashmap/parallel_hashmap/btree.h"
#include <atomic>
#include <filesystem>
#include <list>
#include <utility>
#include <vector>

using std::vector, std::filesystem::path, std::pair, std::list, std::shared_ptr, std::atomic, std::atomic_flag,
    phmap::btree_set, phmap::flat_hash_map;

class SourceNode;
struct CompareSourceNode
{
    using is_transparent = void; // for example with void,
                                 // but could be int or struct CanSearchOnId;
    bool operator()(const SourceNode &lhs, const SourceNode &rhs) const;
    bool operator()(const Node *lhs, const SourceNode &rhs) const;
    bool operator()(const SourceNode &lhs, const Node *rhs) const;
};

class SourceNode : public ObjectFile
{
  public:
    string compilationOutput;
    CCOrHash compileCommandWithTool;
    flat_hash_set<Node *> headerFiles;
    CppSourceTarget *target;
    const Node *node;
    uint32_t indexInBuildCache = -1;
    uint16_t thrIndex = 0;
    SourceNode(CppSourceTarget *target_, const Node *node_);

  protected:
    SourceNode(CppSourceTarget *target_, const Node *node_, bool add0, bool add1);

  public:
    string getPrintName() const override;
    void initializeBuildCache(uint32_t index);
    string getCompileCommand() const;
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    bool ignoreHeaderFile(string_view child) const;
    void parseDepsFromMSVCTextOutput(string &output, bool isClang);
    void parseDepsFromGCCDepsOutput();
    void parseHeaderDeps(string &output);
    void setSourceNodeFileStatus();
    virtual void updateBuildCache(string &outputStr, string &errorStr, bool &buildCacheModified);
};

bool operator<(const SourceNode &lhs, const SourceNode &rhs);

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

struct SMFile : SourceNode // Scanned Module Rule
{
    BuildCache::Cpp::ModuleFile::SmRules smRulesCache;

    // Those header-files which are #included in this module or hu. These are initialized from config-cache as big-hu
    // have these. While Source::headerFiles have all the header-files of ours and our dependencies for accurate
    // rebuilds.
    flat_hash_map<string, Node *> composingHeaders;
    flat_hash_set<SMFile *> allSMFileDependencies;

    RunCommand run;
    vector<string> logicalNames;

    vector<Node *> *headerFilesCache;
    Node *interfaceNode;
    SMFile *waitingFor = nullptr;

    N2978::IPCManagerBS *ipcManager;

    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;

    // This is used to prevent header-unit addition in updateBTargets list more than once since the same header-unit
    // could be potentially discovered more than once.
    bool addedForBuilding = false;

    // following 2 only used at configure time.
    bool isReqDep = false;
    bool isUseReqDep = false;
    bool compileCommandChanged = false;
    bool firstMessageSent = false;

    // Whether to set ignoreHeaderDeps to true for HeaderUnits which come from such Node includes for which
    // ignoreHeaderDeps is true
    inline static bool ignoreHeaderDepsForIgnoreHeaderUnits = true;

    inline static thread_local vector<string_view> includeNames;
    SMFile(CppSourceTarget *target_, const Node *node_);
    void initializeBuildCache(BuildCache::Cpp::ModuleFile &modCache, uint32_t index);
    void makeAndSendBTCModule(SMFile &mod);
    void makeAndSendBTCNonModule(SMFile &hu);
    void duplicateHeaderFileOrUnitError(const string &headerName, struct HeaderFileOrUnit &first,
                                        HeaderFileOrUnit &second, CppSourceTarget *firstTarget,
                                        CppSourceTarget *secondTarget);
    SMFile *findModule(const string &moduleName) const;
    HeaderFileOrUnit findHeaderFileOrUnit(const string &headerName);
    bool build(Builder &builder);
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    string getOutputFileName() const;
    BTargetType getBTargetType() const override;
    void updateBuildCache(string &outputStr, string &errorStr, bool &buildCacheModified) override;
    string getCompileCommand() const;
    void setFileStatusAndPopulateAllDependencies();
};

inline deque<SMFile> globalSMFiles;

#endif // HMAKE_SMFILE_HPP
