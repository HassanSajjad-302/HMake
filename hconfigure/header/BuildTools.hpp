#ifndef HMAKE_BUILDTOOLS_HPP
#define HMAKE_BUILDTOOLS_HPP
#ifdef USE_HEADER_UNITS
#include "nlohmann/json.hpp";
import <filesystem>;
#else
#include "nlohmann/json.hpp"
#include <filesystem>
#endif

using std::filesystem::path;
using Json = nlohmann::json;

enum class Platform : char
{
    LINUX,
    WINDOWS
};

struct Version
{
    unsigned majorVersion = 0;
    unsigned minorVersion = 0;
    unsigned patchVersion = 0;
    auto operator<=>(const Version &) const = default;
    Version(unsigned majorVersion_ = 0, unsigned minorVersion_ = 0, unsigned patchVersion_ = 0);
};
void to_json(Json &j, const Version &p);
void from_json(const Json &j, Version &v);

enum class BTFamily : char
{
    NOT_assignED,
    GCC,
    MSVC,
    CLANG,
};
void to_json(Json &json, const BTFamily &bTFamily);
void from_json(const Json &json, BTFamily &bTFamily);

struct BuildTool
{
    BTFamily bTFamily = BTFamily::NOT_assignED;
    Version bTVersion;
    path bTPath;
    BuildTool(BTFamily btFamily_, Version btVersion_, path btPath_);
    BuildTool() = default;
};
void to_json(Json &json, const BuildTool &buildTool);
void from_json(const Json &json, BuildTool &buildTool);

// templates could had been used here but to avoid extra typing of < and >, this is preferred.
struct Compiler : BuildTool
{
    Compiler(BTFamily btFamily_, Version btVersion_, path btPath_);
    Compiler() = default;
};

struct Linker : BuildTool
{
    Linker(BTFamily btFamily_, Version btVersion_, path btPath_);
    Linker() = default;
};

struct Archiver : BuildTool
{
    Archiver(BTFamily btFamily_, Version btVersion_, path btPath_);
    Archiver() = default;
};

struct ScannerTool : BuildTool
{
    ScannerTool(BTFamily btFamily_, Version btVersion_, path btPath_);
    ScannerTool() = default;
};

#endif // HMAKE_BUILDTOOLS_HPP

