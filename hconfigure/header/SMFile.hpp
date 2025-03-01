#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP

#ifdef USE_HEADER_UNITS
import "SpecialNodes.hpp";
import "ObjectFile.hpp";
import <filesystem>;
import <list>;
import <utility>;
import <vector>;
import <atomic>;
#else
#include "SpecialNodes.hpp"
#include "ObjectFile.hpp"
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

struct HeaderUnitIndexInfo
{
    uint64_t targetIndex;
    uint64_t myIndex;
    HeaderUnitIndexInfo(uint64_t targetIndex_, uint64_t myIndex_);
    Value &getSingleHeaderUnitDep() const;
};

class SourceNode : public ObjectFile
{
  public:
    RAPIDJSON_DEFAULT_ALLOCATOR sourceNodeAllocator;
    Value sourceJson{kArrayType};
    CppSourceTarget *target;
    const Node *node;
    uint64_t indexInBuildCache = UINT64_MAX;
    bool ignoreHeaderDeps = false;
    SourceNode(CppSourceTarget *target_, Node *node_);

  protected:
    SourceNode(CppSourceTarget *target_, const Node *node_, bool add0, bool add1, bool add2);

  public:
    string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    string getTarjanNodeName() const override;
    static void initializeSourceJson(Value &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                     const CppSourceTarget &target);
    void updateBTarget(Builder &builder, unsigned short round) override;
    bool checkHeaderFiles(const Node *compareNode) const;
    void setSourceNodeFileStatus();
};

void to_json(Json &j, const SourceNode &sourceNode);
void to_json(Json &j, const SourceNode *sourceNode);
bool operator<(const SourceNode &lhs, const SourceNode &rhs);

enum class SM_REQUIRE_TYPE : char
{
    NOT_assignED = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    HEADER_UNIT = 3,
};

enum class SM_FILE_TYPE : char
{
    NOT_ASSIGNED = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    PARTITION_IMPLEMENTATION = 3,
    HEADER_UNIT = 4,
    PRIMARY_IMPLEMENTATION = 5,
    // Used only in GenerateModuleData
    HEADER_UNIT_DISABLED = 6,
};

struct HeaderUnitConsumer
{
    bool angle;
    string logicalName;
    HeaderUnitConsumer(bool angle_, string logicalName_);
};

struct ValueObjectFileMapping
{
    // should be const
    Value *requireJson;
    Node *objectFileOutputFilePath;

    ValueObjectFileMapping(Value *requireJson_, Node *objectFileOutputFilePath_);
};

struct SMFile : SourceNode // Scanned Module Rule
{
    string logicalName;
    vector<ValueObjectFileMapping> pValueObjectFileMapping;
    // Key is the pointer to the header-unit while value is the consumption-method of that header-unit by this smfile.
    // A header-unit might be consumed in multiple ways specially if this file is consuming it one way and the file it
    // depends on is consuming it another way.
    flat_hash_map<const SMFile *, HeaderUnitConsumer> headerUnitsConsumptionData;
    btree_set<SMFile *, IndexInTopologicalSortComparatorRoundZero> allSMFileDependenciesRoundZero;

    unique_ptr<vector<char>> smRuleFileBuffer;
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;

    bool isInterface = false;
    bool isSMRulesJsonSet = false;
    bool foundFromCache = false;
    bool isObjectFileOutdated = false;
    bool isSMRuleFileOutdated = false;

    // atomic
    bool isObjectFileOutdatedCallCompleted = false;
    bool isSMRuleFileOutdatedCallCompleted = false;
    // This is used to prevent header-unit addition in BTargets list more than once since the same header-unit could be
    // potentially discovered more than once.
    bool addedForRoundOne = false;

    // Whether to set ignoreHeaderDeps to true for HeaderUnits which come from such Node includes for which
    // ignoreHeaderDeps is true
    inline static bool ignoreHeaderDepsForIgnoreHeaderUnits = true;
    SMFile(CppSourceTarget *target_, Node *node_);
    SMFile(CppSourceTarget *target_, Node *node_, SM_FILE_TYPE type_, bool olderHeaderUnit);
    void setSMRulesJson(string_view smRulesJson);
    void checkHeaderFilesIfSMRulesJsonSet();
    void setLogicalNameAndAddToRequirePath();
    void updateBTarget(Builder &builder, unsigned short round) override;
    bool calledOnce = false;
    void saveSMRulesJsonToSourceJson(const string &smrulesFileOutputClang);
    static void initializeModuleJson(Value &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                     const CppSourceTarget &target);
    InclNodePointerTargetMap findHeaderUnitTarget(Node *headerUnitNode) const;
    void initializeHeaderUnits(Builder &builder);
    void addNewBTargetInFinalBTargetsRound1(Builder &builder);
    void setSMFileType(Builder &builder);
    void isObjectFileOutdatedComparedToSourceFileAndItsDeps();
    void isSMRulesFileOutdatedComparedToSourceFileAndItsDeps();
    string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    BTargetType getBTargetType() const override;
    void setFileStatusAndPopulateAllDependencies();
    string getFlag(const string &outputFilesWithoutExtension) const;
    string getFlagPrint(const string &outputFilesWithoutExtension) const;
    string getRequireFlag(const SMFile &dependentSMFile) const;
    string getRequireFlagPrint(const SMFile &logicalName_) const;
    string getModuleCompileCommandPrintLastHalf() const;
};

inline deque<SMFile> globalSMFiles;

#endif // HMAKE_SMFILE_HPP
