#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP

#ifdef USE_HEADER_UNITS
import "IPCManagerBS.hpp";
import "RunCommand.hpp";
import "SpecialNodes.hpp";
import "TargetCache.hpp";
import "ObjectFile.hpp";
import <filesystem>;
import <list>;
import <utility>;
import <vector>;
import <atomic>;
#else
#include "IPCManagerBS.hpp"
#include "ObjectFile.hpp"
#include "RunCommand.hpp"
#include "SpecialNodes.hpp"
#include "TargetCache.hpp"
#include "btree.h"
#include "nlohmann/json.hpp"
#include <atomic>
#include <filesystem>
#include <list>
#include <utility>
#include <vector>
#endif

using Json = nlohmann::json;
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
    CCOrHash compileCommandWithTool;
    flat_hash_set<Node *> headerFiles;
    CppSourceTarget *target;
    const Node *node;
    uint32_t indexInBuildCache = -1;
    bool ignoreHeaderDeps = false;
    SourceNode(CppSourceTarget *target_, const Node *node_);

  protected:
    SourceNode(CppSourceTarget *target_, const Node *node_, bool add0, bool add1);

  public:
    string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    string getPrintName() const override;
    void initializeBuildCache(uint32_t index);
    void completeCompilation();
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    bool ignoreHeaderFile(string_view child) const;
    void parseDepsFromMSVCTextOutput(string &output, bool isClang);
    void parseDepsFromGCCDepsOutput();
    void parseHeaderDeps(string &output);
    void setSourceNodeFileStatus();
    virtual void updateBuildCache();
};

void to_json(Json &j, const SourceNode &sourceNode);
void to_json(Json &j, const SourceNode *sourceNode);
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
    string logicalName;

    flat_hash_set<SMFile *> allSMFileDependencies;
    vector<string> logicalNames;
    vector<Node *> *headerFilesCache;
    Node *interfaceNode;
    SMFile *waitingFor = nullptr;

    N2978::IPCManagerBS *ipcManager;
    RunCommand run;

    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;

    // This is used to prevent header-unit addition in updateBTargets list more than once since the same header-unit
    // could be potentially discovered more than once.
    bool addedForBuilding = false;

    bool isReqDep = false;
    bool isUseReqDep = false;
    bool isSystem = false;
    bool compileCommandChanged = false;

    // Whether to set ignoreHeaderDeps to true for HeaderUnits which come from such Node includes for which
    // ignoreHeaderDeps is true
    inline static bool ignoreHeaderDepsForIgnoreHeaderUnits = true;

    inline static thread_local vector<string_view> includeNames;
    SMFile(CppSourceTarget *target_, const Node *node_);
    SMFile(CppSourceTarget *target_, const Node *node_, bool);
    void initializeBuildCache(BuildCache::Cpp::ModuleFile &modCache, uint32_t index);
    void makeAndSendBTCModule(SMFile &mod);
    void makeAndSendBTCNonModule(SMFile &hu);
    void duplicateHeaderFileOrUnitError(const string &headerName, struct HeaderFileOrUnit &first,
                                        HeaderFileOrUnit &second, CppSourceTarget *firstTarget,
                                        CppSourceTarget *secondTarget);
    SMFile *findModule(const string &moduleName) const;
    HeaderFileOrUnit *findHeaderFileOrUnit(const string &headerName);
    bool build(Builder &builder);
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    string getOutputFileName() const;
    // In case of header-units, this check the ifc file.
    string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    BTargetType getBTargetType() const override;
    void updateBuildCache() override;
    string getCompileCommand() const;
    void setFileStatusAndPopulateAllDependencies();
    string getFlag() const;
    string getFlagPrint() const;
    string getRequireFlag(const SMFile &dependentSMFile) const;
    string getRequireFlagPrint(const SMFile &logicalName_) const;
    string getModuleCompileCommandPrintLastHalf() const;
};

inline deque<SMFile> globalSMFiles;

#endif // HMAKE_SMFILE_HPP
