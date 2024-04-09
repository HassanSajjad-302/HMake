#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP

#ifdef USE_HEADER_UNITS
import "nlohmann/json.hpp";
import "Node.hpp";
import "ObjectFileProducer.hpp";
import <filesystem>;
import <list>;
import <set>;
import <utility>;
import <vector>;
import <atomic>;
#else
#include "Node.hpp"
#include "ObjectFileProducer.hpp"
#include "nlohmann/json.hpp"
#include <atomic>
#include <filesystem>
#include <list>
#include <set>
#include <utility>
#include <vector>
#endif

using Json = nlohmann::json;
using std::map, std::set, std::vector, std::filesystem::path, std::pair, std::list, std::shared_ptr, std::atomic,
    std::atomic_flag;

class LibDirNode
{
  public:
    Node *node = nullptr;
    bool isStandard = false;
    explicit LibDirNode(Node *node_, bool isStandard_ = false);
    static void emplaceInList(list<LibDirNode> &libDirNodes, LibDirNode &libDirNode);
    static void emplaceInList(list<LibDirNode> &libDirNodes, Node *node_, bool isStandard_ = false);
};

class InclNode : public LibDirNode
{
  public:
    // Used with includeDirectories to specify whether to ignore include-files from these directories from being stored
    // in target-cache file
    bool ignoreHeaderDeps = false;
    explicit InclNode(Node *node_, bool isStandard_ = false, bool ignoreHeaderDeps_ = false);
    static bool emplaceInList(list<InclNode> &includes, InclNode &libDirNode);
    static bool emplaceInList(list<InclNode> &includes, Node *node_, bool isStandard_ = false,
                              bool ignoreHeaderDeps_ = false);
};
bool operator<(const InclNode &lhs, const InclNode &rhs);

struct SourceNode;
struct CompareSourceNode
{
    using is_transparent = void; // for example with void,
                                 // but could be int or struct CanSearchOnId;
    bool operator()(const SourceNode &lhs, const SourceNode &rhs) const;
    bool operator()(Node *lhs, const SourceNode &rhs) const;
    bool operator()(const SourceNode &lhs, Node *rhs) const;
};

struct SourceNode : public ObjectFile
{
    RAPIDJSON_DEFAULT_ALLOCATOR sourceNodeAllocator;
    PValue *sourceJson = nullptr;
    class CppSourceTarget *target;
    const Node *node;
    bool ignoreHeaderDeps = false;
    SourceNode(CppSourceTarget *target_, Node *node_);
    pstring getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    pstring getTarjanNodeName() const override;
    void updateBTarget(Builder &builder, unsigned short round) override;
    void setSourceNodeFileStatus();
};

void to_json(Json &j, const SourceNode &sourceNode);
void to_json(Json &j, const SourceNode *sourceNode);
bool operator<(const SourceNode &lhs, const SourceNode &rhs);

enum class SM_REQUIRE_TYPE : char
{
    NOT_ASSIGNED = 0,
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

struct HeaderUnitConsumer
{
    bool angle;
    pstring logicalName;
    HeaderUnitConsumer(bool angle_, pstring logicalName_);
    auto operator<=>(const HeaderUnitConsumer &headerUnitConsumer) const = default;
};

struct PValueObjectFileMapping
{
    PValue *requireJson;
    Node *objectFileOutputFilePath;

    PValueObjectFileMapping(PValue *requireJson_, Node *objectFileOutputFilePath_);
};

struct SMFile : public SourceNode // Scanned Module Rule
{
    pstring logicalName;
    vector<PValueObjectFileMapping> pValueObjectFileMapping;
    // Key is the pointer to the header-unit while value is the consumption-method of that header-unit by this smfile.
    // A header-unit might be consumed in multiple ways specially if this file is consuming it one way and the file it
    // is depending on is consuming it another way.
    map<const SMFile *, set<HeaderUnitConsumer>> headerUnitsConsumptionMethods;
    set<SMFile *, IndexInTopologicalSortComparatorRoundZero> allSMFileDependenciesRoundZero;

    unique_ptr<char[]> smRuleFileBuffer;
    size_t headerUnitsIndex = UINT64_MAX;
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;

    bool angle = false;
    bool readJsonFromSMRulesFile = false;
    bool isInterface = false;

    // Whether to set ignoreHeaderDeps to true for HeaderUnits which come from such Node includes for which
    // ignoreHeaderDeps is true
    inline static bool ignoreHeaderDepsForIgnoreHeaderUnits = true;
    SMFile(CppSourceTarget *target_, Node *node_);
    void updateBTarget(Builder &builder, unsigned short round) override;
    void saveRequiresJsonAndInitializeHeaderUnits(Builder &builder, pstring &smrulesFileOutputClang);
    void initializeNewHeaderUnit(const PValue &inclNodes, Builder &builder);
    void addNewBTargetInFinalBTargets(Builder &builder);
    void iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder);
    bool shouldGenerateSMFileInRoundOne();
    pstring getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const override;
    BTargetType getBTargetType() const override;
    void setFileStatusAndPopulateAllDependencies();
    pstring getFlag(const pstring &outputFilesWithoutExtension) const;
    pstring getFlagPrint(const pstring &outputFilesWithoutExtension) const;
    pstring getRequireFlag(const SMFile &dependentSMFile) const;
    pstring getRequireFlagPrint(const SMFile &logicalName_) const;
    pstring getModuleCompileCommandPrintLastHalf();
};

/*void to_json(Json &j, const SMFile &smFile);
void to_json(Json &j, const SMFile *smFile);*/

#endif // HMAKE_SMFILE_HPP
