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

    string_view defaultName() const
    {
        return registryOrder.front();
    }
    string toJson() const;

    std::map<string, Toolchain, std::less<>> entries;

  private:
    path userToolchainsFilePath;
    std::vector<string> registryOrder;

    void loadFile(const path &filePath);
};

inline Toolchains toolchains;
#endif // HMAKE_TOOLCHAINS_HPP
