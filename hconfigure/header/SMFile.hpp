#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP

#include "ObjectFileProducer.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

using Json = nlohmann::ordered_json;
using std::string, std::map, std::set, std::vector, std::filesystem::path, std::pair;

class Node;
struct CompareNode
{
    using is_transparent = void;
    bool operator()(const Node &lhs, const Node &rhs) const;
    bool operator()(const string &lhs, const Node &rhs) const;
    bool operator()(const Node &lhs, const string &rhs) const;
};

class Node
{
    std::filesystem::file_time_type lastUpdateTime;

  public:
    Node(const path &filePath_, bool isFile, bool mayNotExist = false);
    // This keeps info if a file is touched. If it's touched, it's not touched again.
    inline static set<Node, CompareNode> allFiles;
    string filePath;
    std::filesystem::file_time_type getLastUpdateTime() const;
    // Create a node and inserts it into the allFiles if it is not already there
    static const Node *getNodeFromString(const string &str, bool isFile, bool mayNotExist = false);

  private:
    // Because checking for lastUpdateTime is expensive, it is done only once even if file is used in multiple targets.
    bool isUpdated = false;

  public:
    bool doesNotExist = false;
    // Used with includeDirectories to specify whether to ignore include-files from these directories from being stored
    // in target-cache file
    bool ignoreIncludes = false;
};
bool operator<(const Node &lhs, const Node &rhs);
void to_json(Json &j, const Node *node);

struct IncludeNode
{
};
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
    Json headerFilesJson;
    const Node *node;
    bool presentInCache = false;
    bool presentInSource = false;
    bool ignoreHeaderDeps = false;
    class CppSourceTarget *target;
    set<const Node *> headerDependencies;
    SourceNode(CppSourceTarget *target_, Node *node_);
    string getObjectFileOutputFilePath() override;
    string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) override;
    void updateBTarget(unsigned short round, Builder &builder) override;
    void setSourceNodeFileStatus(const string &ex, RealBTarget &realBTarget);
};

void to_json(Json &j, const SourceNode &sourceNode);
void to_json(Json &j, const SourceNode *smFile);
bool operator<(const SourceNode &lhs, const SourceNode &rhs);

enum class SM_REQUIRE_TYPE : unsigned short
{
    NOT_ASSIGNED = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    HEADER_UNIT = 3,
};

enum class SM_FILE_TYPE : unsigned short
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
    string logicalName;
    HeaderUnitConsumer(bool angle_, string logicalName_);
    auto operator<=>(const HeaderUnitConsumer &headerUnitConsumer) const = default;
};

struct SMFile : public SourceNode // Scanned Module Rule
{
    string logicalName;
    // Key is the pointer to the header-unit while value is the consumption-method of that header-unit by this smfile.
    // A header-unit might be consumed in multiple ways specially if this file is consuming it one way and the file it
    // is depending on is consuming it another way.
    map<const SMFile *, set<HeaderUnitConsumer>> headerUnitsConsumptionMethods;
    set<SMFile *> allSMFileDependenciesRoundZero;
    Json requiresJson;
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;
    bool angle;
    bool hasProvide = false;
    bool standardHeaderUnit = false;
    bool smrulesFileParsed = false;
    // Used to determine whether the file is present in cache and whether it needs an updated SMRules file.
    bool generateSMFileInRoundOne = false;

    inline static bool ignoreStandardHeaderUnitsHeaderDeps = true;
    SMFile(CppSourceTarget *target_, Node *node_);
    void updateBTarget(unsigned short round, class Builder &builder) override;
    string getObjectFileOutputFilePath() override;
    string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) override;
    void saveRequiresJsonAndInitializeHeaderUnits(Builder &builder);
    void initializeNewHeaderUnit(const Json &requireJson, Builder &builder);
    void iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder);
    static bool isSubDirPathStandard(const path &headerUnitPath, set<const Node *> &standardIncludes);
    void setSMFileStatusRoundZero();
    void duringSort(Builder &builder, unsigned short round, unsigned int indexInTopologicalSortComparator) override;
    string getFlag(const string &outputFilesWithoutExtension) const;
    string getFlagPrint(const string &outputFilesWithoutExtension) const;
    string getRequireFlag(const SMFile &dependentSMFile) const;
    string getRequireFlagPrint(const SMFile &logicalName_) const;
    string getModuleCompileCommandPrintLastHalf();
};
#endif // HMAKE_SMFILE_HPP
