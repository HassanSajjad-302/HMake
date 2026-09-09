#ifndef HMAKE_TOOLCHAINS_HPP
#define HMAKE_TOOLCHAINS_HPP

#include "BuildTools.hpp"
#include "Features.hpp"

#include <map>
#include <vector>

/// One fully resolved, stable named toolchain from toolchains.json.
///
/// `family` identifies the tool implementation (clang/gcc/msvc), while
/// `style` identifies its command-line convention (gnu/msvc).
struct Toolchain
{
    string name;
    string family;
    string style;
    string version;
    string target;

    Compiler compiler;
    Linker linker;
    Archiver archiver;

    std::vector<string> includeDirs;
    std::vector<string> libraryDirs;
    std::vector<string> bootstrapArguments;

    // Parsed target-triple values used by Configuration compatibility checks.
    TargetOS targetOs = TargetOS::NONE;
    Arch targetArch = Arch::NONE;
    AddressModel targetAddressModel = AddressModel::NONE;
};

struct Toolchains
{
    Toolchains();

    void initialize(const path &sourceDirectory);

    /// Probes and validates a Linux compiler's default profile, then appends a new named registry entry.
    /// Saves to the user registry by default, or the initialized project's registry when project is true.
    /// Existing names in either registry are never replaced.
    void calibrate(string_view compiler, string_view name, bool project = false);

    /// Removes one registry definition, never the first (built-in default) toolchain.
    /// Searches both initialized registries and edits the one defining name; compiler files are not removed.
    void remove(string_view name);

    string toJson() const;

    std::map<string, Toolchain, std::less<>> entries;
    std::vector<Toolchain *> registryOrder;

  private:
    path userToolchainsFilePath;
    path projectToolchainsFilePath;

    void loadFile(const path &filePath);
};

inline Toolchains toolchains;
#endif // HMAKE_TOOLCHAINS_HPP
