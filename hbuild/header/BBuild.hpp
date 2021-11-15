

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "filesystem"
#include "nlohmann/json.hpp"
#include "string"

using std::string, std::vector, std::filesystem::path;
using Json = nlohmann::ordered_json;

enum BTargetType
{
    EXECUTABLE,
    STATIC,
    SHARED,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED
};
void from_json(const Json &j, BTargetType &targetType);

struct SourceDirectory
{
    path sourceDirectory;
    string regex;
};
void from_json(const Json &j, SourceDirectory &sourceDirectory);

struct BIDD
{
    string path;
    bool copy;
};
void from_json(const Json &j, BIDD &bCompileDefinition);

struct BLibraryDependency
{
    string path;
    string hmakeFilePath;
    bool copy;
    bool preBuilt;
    bool imported;
};
void from_json(const Json &j, BLibraryDependency &bCompileDefinition);

struct BCompileDefinition
{
    string name;
    string value;
};
void from_json(const Json &j, BCompileDefinition &bCompileDefinition);

class BTarget
{
  public:
    string outputName;
    string outputDirectory;
    explicit BTarget(const string &targetFilePath);
};

// BuildPreBuiltTarget
struct BPTarget
{
    explicit BPTarget(const string &targetFilePath, const path &copyFrom);
};

struct BVariant
{
    explicit BVariant(const path &variantFilePath);
};

struct BProject
{
    explicit BProject(const path &projectFilePath);
};

struct BPackage
{
    explicit BPackage(const path &packageFilePath);
};
#endif // HMAKE_HBUILD_SRC_BBUILD_HPP
