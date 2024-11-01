#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP

#ifdef USE_HEADER_UNITS
import "nlohmann/json.hpp";
import "InclNodeTargetMap.hpp";
import "ObjectFile.hpp";
import <filesystem>;
import <list>;
import <utility>;
import <vector>;
import <atomic>;
#else
#include "InclNodeTargetMap.hpp"
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
    PValue &getSingleHeaderUnitDep() const;
};

class SourceNode : public ObjectFile
{
  public:
    RAPIDJSON_DEFAULT_ALLOCATOR sourceNodeAllocator;
    PValue sourceJson{kArrayType};
    CppSourceTarget *target;
    const Node *node;
    // TODO: 4-bytes enough or maybe 2bytes
    uint64_t indexInBuildCache = UINT64_MAX;
    bool ignoreHeaderDeps = false;
    SourceNode(CppSourceTarget *target_, Node *node_);

  protected:
    SourceNode(CppSourceTarget *target_, const Node *node_, bool add0, bool add1, bool add2);

  public:
    pstring getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    pstring getTarjanNodeName() const override;
    static void initializeSourceJson(PValue &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                     const CppSourceTarget &target);
    void updateBTarget(Builder &builder, unsigned short round) override;
    void populateModuleData(Builder &builder);
    bool checkHeaderFiles2(const Node *compareNode, bool alsoCheckHeaderUnit) const;
    bool checkHeaderFiles(const Node *compareNode) const;
    InclNodePointerTargetMap findHeaderUnitTarget(Node *headerUnitNode);
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
    pstring logicalName;
    HeaderUnitConsumer(bool angle_, pstring logicalName_);
};

struct PValueObjectFileMapping
{
    // should be const
    PValue *requireJson;
    Node *objectFileOutputFilePath;

    PValueObjectFileMapping(PValue *requireJson_, Node *objectFileOutputFilePath_);
};

struct SMFile : SourceNode // Scanned Module Rule
{
    pstring logicalName;
    vector<PValueObjectFileMapping> pValueObjectFileMapping;
    // Key is the pointer to the header-unit while value is the consumption-method of that header-unit by this smfile.
    // A header-unit might be consumed in multiple ways specially if this file is consuming it one way and the file it
    // depends on is consuming it another way.
    flat_hash_map<const SMFile *, HeaderUnitConsumer> headerUnitsConsumptionData;
    btree_set<SMFile *, IndexInTopologicalSortComparatorRoundZero> allSMFileDependenciesRoundZero;

    unique_ptr<vector<pchar>> smRuleFileBuffer;
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
    SMFile(CppSourceTarget *target_, Node *node_, SM_FILE_TYPE type_);
    SMFile(CppSourceTarget *target_, Node *node_, SM_FILE_TYPE type_, bool olderHeaderUnit);
    void setSMRulesJson(pstring_view smRulesJson);
    void checkHeaderFilesIfSMRulesJsonSet();
    void setLogicalNameAndAddToRequirePath();
    void updateBTarget(Builder &builder, unsigned short round) override;
    bool calledOnce = false;
    void saveSMRulesJsonToSourceJson(const pstring &smrulesFileOutputClang);
    static void initializeModuleJson(PValue &j, const Node *node, decltype(ralloc) &sourceNodeAllocator,
                                     const CppSourceTarget &target);
    void initializeHeaderUnits(Builder &builder);
    void addNewBTargetInFinalBTargets(Builder &builder);
    void setSMFileType(Builder &builder);
    void isObjectFileOutdatedComparedToSourceFileAndItsDeps();
    void isSMRulesFileOutdatedComparedToSourceFileAndItsDeps();
    pstring getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    BTargetType getBTargetType() const override;
    void setFileStatusAndPopulateAllDependencies();
    pstring getFlag(const pstring &outputFilesWithoutExtension) const;
    pstring getFlagPrint(const pstring &outputFilesWithoutExtension) const;
    pstring getRequireFlag(const SMFile &dependentSMFile) const;
    pstring getRequireFlagPrint(const SMFile &logicalName_) const;
    pstring getModuleCompileCommandPrintLastHalf() const;
};

/*void to_json(Json &j, const SMFile &smFile);
void to_json(Json &j, const SMFile *smFile);*/

#endif // HMAKE_SMFILE_HPP
