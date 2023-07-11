#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP
#ifdef USE_HEADER_UNITS
import "ObjectFileProducer.hpp";
#include "nlohmann/json.hpp";
import <filesystem>;
import <list>;
import <set>;
import <utility>;
import <vector>;
import <atomic>;
#else
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

class Node;
struct CompareNode
{
    using is_transparent = void;
    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const pstring &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const pstring &rhs) const;
};

class Node
{
  public:
    pstring filePath;

  private:
    std::filesystem::file_time_type lastUpdateTime;
    atomic<bool> systemCheckCalled{};
    atomic<bool> systemCheckCompleted{};

  public:
    Node(const path &filePath_);
    // This keeps info if a file is touched. If it's touched, it's not touched again.
    inline static set<Node, CompareNode> allFiles;
    std::filesystem::file_time_type getLastUpdateTime() const;

    /*    static path getFinalNodePathFromPath(const pstring &str);
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromPath(const pstring &str, bool isFile, bool mayNotExist = false);*/

    static path getFinalNodePathFromPath(path filePath);
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromPath(const path &p, bool isFile, bool mayNotExist = false);

  private:
    void performSystemCheck(bool isFile, bool mayNotExist);
    // Because checking for lastUpdateTime is expensive, it is done only once even if file is used in multiple targets.
    bool isUpdated = false;

  public:
    bool doesNotExist = false;
};
bool operator<(const Node &lhs, const Node &rhs);
void to_json(Json &j, const Node *node);

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
    HEADER_UNIT = 3,
    PRIMARY_IMPLEMENTATION = 4,
    PARTITION_IMPLEMENTATION = 5,
    GLOBAL_MODULE_FRAGMENT = 6,
};

struct HeaderUnitConsumer
{
    bool angle;
    pstring logicalName;
    HeaderUnitConsumer(bool angle_, pstring logicalName_);
    auto operator<=>(const HeaderUnitConsumer &headerUnitConsumer) const = default;
};

struct SMFile : public SourceNode // Scanned Module Rule
{
    pstring logicalName;
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

    // Whether to set ignoreHeaderDeps to true for HeaderUnits which come from such Node includes for which
    // ignoreHeaderDeps is true
    inline static bool ignoreHeaderDepsForIgnoreHeaderUnits = true;
    SMFile(CppSourceTarget *target_, Node *node_);
    void decrementTotalSMRuleFileCount(Builder &builder);
    void updateBTarget(Builder &builder, unsigned short round) override;
    void saveRequiresJsonAndInitializeHeaderUnits(Builder &builder);
    void initializeNewHeaderUnit(const PValue &requirePValue, Builder &builder);
    void iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder);
    bool generateSMFileInRoundOne();
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
