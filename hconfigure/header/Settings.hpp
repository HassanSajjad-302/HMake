#ifndef HMAKE_SETTINGS_HPP
#define HMAKE_SETTINGS_HPP

#include "fmt/color.h"
#include "nlohmann/json.hpp"
#include <string>
#include <thread>

using std::string;
using Json = nlohmann::ordered_json;

enum class PathPrintLevel
{
    NO = 0,
    HALF = 1,
    FULL = 2,
};

struct PathPrint
{
    PathPrintLevel printLevel;
    unsigned depth;
    bool addQuotes;
    bool isDirectory;
    bool isTool;
};
void to_json(Json &json, const PathPrint &outputSettings);
void from_json(const Json &json, PathPrint &outputSettings);

// TODO
//  Replace environment with standard

struct CompileCommandPrintSettings
{
    PathPrint tool{
        .printLevel = PathPrintLevel::HALF, .depth = 0, .addQuotes = false, .isDirectory = false, .isTool = true};
    bool environmentCompilerFlags = false;
    bool compilerFlags = true;
    bool compilerTransitiveFlags = true;
    bool compileDefinitions = true;

    PathPrint projectIncludeDirectories{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = true, .isTool = false};
    PathPrint environmentIncludeDirectories{
        .printLevel = PathPrintLevel::NO, .depth = 1, .addQuotes = false, .isDirectory = true, .isTool = false};
    bool onlyLogicalNameOfRequireIFC = true;
    PathPrint requireIFCs{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint sourceFile{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    // Includes flags like /showIncludes /Fo /reference /ifcOutput /interface /internalPartition
    bool infrastructureFlags = false;
    PathPrint ifcOutputFile{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint objectFile{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint outputAndErrorFiles{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    bool pruneHeaderDepsFromMSVCOutput = true;
    bool pruneCompiledSourceFileNameFromMSVCOutput = true;
    bool ratioForHMakeTime = false;
    bool showPercentage = false;
};
void to_json(Json &json, const CompileCommandPrintSettings &ccpSettings);
void from_json(const Json &json, CompileCommandPrintSettings &ccpSettings);

struct ArchiveCommandPrintSettings
{
    PathPrint tool{
        .printLevel = PathPrintLevel::HALF, .depth = 0, .addQuotes = false, .isDirectory = false, .isTool = true};
    bool infrastructureFlags = true;
    PathPrint objectFiles{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint archive{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint outputAndErrorFiles{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
};
void to_json(Json &json, const ArchiveCommandPrintSettings &acpSettings);
void from_json(const Json &json, ArchiveCommandPrintSettings &acpSettings);

struct LinkCommandPrintSettings
{
    PathPrint tool{
        .printLevel = PathPrintLevel::HALF, .depth = 0, .addQuotes = false, .isDirectory = false, .isTool = true};
    bool infrastructureFlags = false;
    bool linkerFlags = true;
    bool linkerTransitiveFlags = true;
    PathPrint objectFiles{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint libraryDependencies{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint libraryDirectories{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = true, .isTool = false};
    PathPrint environmentLibraryDirectories{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = true, .isTool = false};
    PathPrint binary{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint outputAndErrorFiles{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
};
void to_json(Json &json, const LinkCommandPrintSettings &lcpSettings);
void from_json(const Json &json, LinkCommandPrintSettings &lcpSettings);

struct PrintColorSettings
{
    unsigned compileCommandColor = static_cast<int>(fmt::color::dark_green);
    unsigned archiveCommandColor = static_cast<int>(fmt::color::brown);
    unsigned linkCommandColor = static_cast<int>(fmt::color::dark_red);
    unsigned toolErrorOutput = static_cast<int>(fmt::color::red);
    unsigned hbuildStatementOutput = static_cast<int>(fmt::color::yellow);
    unsigned hbuildSequenceOutput = static_cast<int>(fmt::color::cyan);
    unsigned hbuildErrorOutput = static_cast<int>(fmt::color::orange);
};
void to_json(Json &json, const PrintColorSettings &printColorSettings);
void from_json(const Json &json, PrintColorSettings &printColorSettings);

struct GeneralPrintSettings
{
    bool preBuildCommandsStatement = true;
    bool preBuildCommands = true;
    bool postBuildCommandsStatement = true;
    bool postBuildCommands = true;
    bool copyingTarget = true;
    bool threadId = true;
};
void to_json(Json &json, const GeneralPrintSettings &generalPrintSettings);
void from_json(const Json &json, GeneralPrintSettings &generalPrintSettings);

struct Settings
{
    unsigned int maximumBuildThreads = std::thread::hardware_concurrency();
    unsigned int maximumLinkThreads = std::thread::hardware_concurrency();

    CompileCommandPrintSettings ccpSettings;
    ArchiveCommandPrintSettings acpSettings;
    LinkCommandPrintSettings lcpSettings;
    PrintColorSettings pcSettings;
    GeneralPrintSettings gpcSettings;
};

void to_json(Json &json, const Settings &settings_);
void from_json(const Json &json, Settings &settings_);
string getReducedPath(const string &subjectPath, const PathPrint &pathPrint);
inline Settings settings;
#endif // HMAKE_SETTINGS_HPP
