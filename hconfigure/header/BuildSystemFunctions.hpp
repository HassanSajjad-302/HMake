#ifndef HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#define HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#ifdef USE_HEADER_UNITS
import "Features.hpp";
import <map>;
import <set>;
import <string>;
#else
#include "Features.hpp"
#include <map>
#include <set>
#include <string>
#endif

using std::string, std::set, std::map;

inline string srcDir;

// path of directory which has configure executable of the project
inline string configureDir;

enum class BSMode // Build System Mode
{
    BUILD,
    CONFIGURE,
};

// By default, mode is configure, however, if, --build cmd option is passed, it is set to BUILD.
inline BSMode bsMode = BSMode::CONFIGURE;

// Used for determining the CTarget to build in BSMode::BUILD. The string is of buildTargetFilePaths.
inline set<string> targetSubDirectories;

// Following can be used for holding memory through build-system run and is used for target<CTarget> in GetTarget
// functions
template <typename T> inline set<T> targets;

// Following can be used to hold pointers for all targets in the build system. It is used by CTarget and BTarget
// constructor by emplacing this
template <typename T> inline set<T *> targetPointers;

#ifdef _WIN32
inline constexpr OS os = OS::NT;
#else
inline constexpr OS os = OS::LINUX;
#endif

inline std::mutex printMutex;

void setBoolsAndSetRunDir(int argc, char **argv);
inline const string pes = "{}"; // paranthesis-escape-sequence, meant to be used in fmt::print.
inline const string dashCpp = "-cpp";
inline const string dashLink = "-link";
void configureOrBuild();
#endif // HMAKE_BUILDSYSTEMFUNCTIONS_HPP

