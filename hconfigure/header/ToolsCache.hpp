#ifndef HMAKE_TOOLSCACHE_HPP
#define HMAKE_TOOLSCACHE_HPP

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

    void initialize(const path &sourceDirectory = {});

    const Toolchain *find(string_view name);
    string_view defaultName();
    string toJson();

  private:
    path userToolchainsFilePath;
    std::map<string, Toolchain, std::less<>> entries;
    std::vector<string> registryOrder;
    bool userFileLoaded = false;
    bool sourceFileLoaded = false;

    void addBuiltin(const string &name, Toolchain toolchain);
    void loadFile(const path &filePath);
};

inline Toolchains toolchains;
#endif // HMAKE_TOOLSCACHE_HPP
