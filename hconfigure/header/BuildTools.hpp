#ifndef HMAKE_BUILDTOOLS_HPP
#define HMAKE_BUILDTOOLS_HPP

#include "filesystem"
#include "nlohmann/json.hpp"

using std::filesystem::path;
using Json = nlohmann::ordered_json;

enum class Platform
{
    LINUX,
    WINDOWS
};

enum class Comparison
{
    EQUAL,
    GREATER_THAN,
    LESSER_THAN,
    GREATER_THAN_OR_EQUAL_TO,
    LESSER_THAN_OR_EQUAL_TO
};

struct Version
{
    unsigned majorVersion;
    unsigned minorVersion;
    unsigned patchVersion;
    Version(unsigned majorVersion_ = 0, unsigned minorVersion_ = 0, unsigned patchVersion_ = 0);
    Comparison comparison; // Used in flags
};
void to_json(Json &j, const Version &p);
void from_json(const Json &j, Version &v);
bool operator<(const Version &lhs, const Version &rhs);
bool operator>(const Version &lhs, const Version &rhs);
bool operator<=(const Version &lhs, const Version &rhs);
bool operator>=(const Version &lhs, const Version &rhs);

enum class BTFamily
{
    GCC,
    MSVC,
    CLANG
};
void to_json(Json &json, const BTFamily &bTFamily);
void from_json(const Json &json, BTFamily &bTFamily);

struct BuildTool
{
    BTFamily bTFamily;
    Version bTVersion;
    path bTPath;
};
void to_json(Json &json, const BuildTool &buildTool);
void from_json(const Json &json, BuildTool &buildTool);

struct Compiler : BuildTool
{
};

struct Linker : BuildTool
{
};

struct Archiver : BuildTool
{
};

#endif // HMAKE_BUILDTOOLS_HPP
