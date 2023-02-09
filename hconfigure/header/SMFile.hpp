#ifndef HMAKE_SMFILE_HPP
#define HMAKE_SMFILE_HPP

#include "BasicTargets.hpp"
#include "PostBasic.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

using Json = nlohmann::ordered_json;
using std::string, std::map, std::set, std::vector, std::filesystem::path, std::pair;

class Node
{
    std::filesystem::file_time_type lastUpdateTime;
    // Because checking for lastUpdateTime is expensive, it is done only once even if file is used in multiple targets.
    bool isUpdated = false;

  public:
    Node(const path &filePath_, bool isFile);
    // This keeps info if a file is touched. If it's touched, it's not touched again.
    inline static set<Node> allFiles;
    string filePath;
    std::filesystem::file_time_type getLastUpdateTime() const;
    // Create a node and inserts it into the allFiles if it is not already there
    static const Node *getNodeFromString(const string &str, bool isFile);
};
bool operator<(const Node &lhs, const Node &rhs);
void to_json(Json &j, const Node *node);

struct CachedFile
{
    const Node *node;
    bool presentInCache = false;
    bool presentInSource = false;
    explicit CachedFile(const string &filePath);
};

struct SourceNode : public CachedFile, public BTarget
{
    CppSourceTarget *target;
    std::shared_ptr<PostCompile> postCompile;
    set<const Node *> headerDependencies;
    SourceNode(CppSourceTarget *target_, const string &filePath);
    virtual string getOutputFilePath();
    void updateBTarget(unsigned short round) override;
    void printMutexLockRoutine(unsigned short round) override;
};

void to_json(Json &j, const SourceNode &sourceNode);
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
};
bool operator<(const HeaderUnitConsumer &lhs, const HeaderUnitConsumer &rhs);

struct SMFile : public SourceNode // Scanned Module Rule
{
    std::shared_ptr<PostBasic> postBasic;
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;
    string logicalName;
    bool angle;
    Json requiresJson;

    SMFile(CppSourceTarget *target_, const string &srcPath);
    void updateBTarget(unsigned short round) override;
    void printMutexLockRoutine(unsigned short round) override;
    string getOutputFilePath() override;
    // State Variables
    map<const SMFile *, set<HeaderUnitConsumer>> headerUnitsConsumptionMethods;
    vector<SMFile *> fileDependencies;
    set<SMFile *> commandLineFileDependencies;

    // If this SMFile is HeaderUnit, then following is the Target whose hu-include-directory this is present in.
    CppSourceTarget *ahuTarget;
    bool hasProvide = false;
    bool standardHeaderUnit = false;

    string getFlag(const string &outputFilesWithoutExtension) const;
    string getFlagPrint(const string &outputFilesWithoutExtension) const;
    string getRequireFlag(const SMFile &dependentSMFile) const;
    string getRequireFlagPrint(const SMFile &logicalName_) const;
    string getModuleCompileCommandPrintLastHalf() const;
};

struct SMFilePathAndModuleScopeComparator
{
    bool operator()(const SMFile &lhs, const SMFile &rhs) const;
};
#endif // HMAKE_SMFILE_HPP
