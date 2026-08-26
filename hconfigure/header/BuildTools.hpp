#ifndef HMAKE_BUILDTOOLS_HPP
#define HMAKE_BUILDTOOLS_HPP

#include <compare>
#include <cstdint>
#include <filesystem>
#include <string>

using std::string;
using std::filesystem::path;

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

enum class BTFamily : uint8_t
{
    GCC,
    MSVC,
};

enum class BTSubFamily : uint8_t
{
    NONE,
    CLANG,
};

struct BuildTool
{
    BTFamily bTFamily{};
    BTSubFamily btSubFamily{};
    Version bTVersion;
    string bTPath;
    BuildTool(BTFamily btFamily_, BTSubFamily btSubFamily_, Version btVersion_, string btPath_);
    BuildTool() = default;
};

// templates could had been used here but to avoid extra typing of < and >, this is preferred.
struct Compiler : BuildTool
{
    Compiler(BTFamily btFamily_, BTSubFamily btSubFamily_, Version btVersion_, string btPath_);
    Compiler() = default;
};

struct Linker : BuildTool
{
    Linker(BTFamily btFamily_, BTSubFamily btSubFamily_, Version btVersion_, string btPath_);
    Linker() = default;
};

struct Archiver : BuildTool
{
    Archiver(BTFamily btFamily_, BTSubFamily btSubFamily_, Version btVersion_, string btPath_);
    Archiver() = default;
};

#endif // HMAKE_BUILDTOOLS_HPP
