#ifndef HMAKE_BUILDTOOLS_HPP
#define HMAKE_BUILDTOOLS_HPP

#include "nlohmann/json.hpp"
#include <filesystem>

using std::filesystem::path;
using Json = nlohmann::json;

enum class Platform : uint8_t
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

enum class BTFamily : uint8_t
{
    GCC,
    MSVC,
};
void to_json(Json &json, const BTFamily &bTFamily);
void from_json(const Json &json, BTFamily &bTFamily);

enum class BTSubFamily : uint8_t
{
    NONE,
    CLANG,
};
void to_json(Json &json, const BTSubFamily &btSubFamily);
void from_json(const Json &json, BTSubFamily &btSubFamily);

struct BuildTool
{
    BTFamily bTFamily{};
    BTSubFamily btSubFamily{};
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

#endif // HMAKE_BUILDTOOLS_HPP
