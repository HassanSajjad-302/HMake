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
#include "ObjectFile.hpp"
#include "SpecialNodes.hpp"
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
    BuildCache::Cpp::SourceFile buildCache;
    CppSourceTarget *target;
    const Node *node;
    uint32_t indexInBuildCache = -1;
    bool ignoreHeaderDeps = false;
    SourceNode(CppSourceTarget *target_, Node *node_);

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
};

struct SMFile : SourceNode // Scanned Module Rule
{
    BuildCache::Cpp::ModuleFile::SmRules smRulesCache;
    // buildCache.compileCommandWithToolCache is scanning command while this is the compile command.
    CCOrHash compileCommandWithToolCache;
    string logicalName;
    // Key is the pointer to the header-unit while value is the consumption-method of that header-unit by this smfile.
    // A header-unit might be consumed in multiple ways specially if this file is consuming it one way and the file it
    // depends on is consuming it another way.
    flat_hash_map<const SMFile *, bool> headerUnitsConsumptionData;

    // TODO
    // Maybe use vector and do in-place sorting especially if big hu are used since the number of elements become really
    // small.
    vector<SMFile *> allSMFileDependenciesRoundZero;

    unique_ptr<vector<char>> smRuleFileBuffer;
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;

    bool isInterface = false;
    // In case of header-unit, it is a bmi-file.
    bool isObjectFileOutdated = false;

    // In case of header-unit, it is a bmi-file.
    // atomic
    bool isObjectFileOutdatedCallCompleted = false;
    // This is used to prevent header-unit addition in BTargets list more than once since the same header-unit could be
    // potentially discovered more than once.
    bool addedForRoundOne = false;

    // Whether to set ignoreHeaderDeps to true for HeaderUnits which come from such Node includes for which
    // ignoreHeaderDeps is true
    inline static bool ignoreHeaderDepsForIgnoreHeaderUnits = true;

    inline static thread_local vector<string_view> includeNames;
    inline static thread_local vector<InclNodePointerTargetMap> huDirPlusTargets;
    SMFile(CppSourceTarget *target_, Node *node_);
    SMFile(CppSourceTarget *target_, const Node *node_, string logicalName_);
    void initializeBuildCache(uint32_t index);
    void setLogicalNameAndAddToRequirePath();
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    string getOutputFileName() const;
    bool calledOnce = false;
    void saveSMRulesJsonToSMRulesCache(const string &smrulesFileOutputClang);
    InclNodePointerTargetMap findHeaderUnitTarget(Node *headerUnitNode) const;
    void initializeNewHeaderUnitsSMRulesNotOutdated(Builder &builder);
    void initializeHeaderUnits(Builder &builder);
    void setSMFileType();
    // In case of header-units, this check the ifc file.
    void checkObjectFileOutdatedHeaderUnits();
    void checkObjectFileOutdatedModules();
    string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    BTargetType getBTargetType() const override;
    void updateBuildCache() override;
    string getCompileCommand();
    void setFileStatusAndPopulateAllDependencies();
    string getFlag() const;
    string getFlagPrint() const;
    string getRequireFlag(const SMFile &dependentSMFile) const;
    string getRequireFlagPrint(const SMFile &logicalName_) const;
    string getModuleCompileCommandPrintLastHalf() const;
};

inline deque<SMFile> globalSMFiles;

#endif // HMAKE_SMFILE_HPP
