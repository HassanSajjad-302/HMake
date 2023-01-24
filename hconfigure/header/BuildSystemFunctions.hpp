#ifndef HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#define HMAKE_BUILDSYSTEMFUNCTIONS_HPP

#include "Features.hpp"
#include <map>
#include <set>
#include <string>

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
inline set<string> buildTargetFilePaths;
// Used for determining the CTarget to build in BSMode::BUILD. The string is of buildTargetFilePaths.
inline map<string, set<class CTarget *>> targetFilePaths;
// Pointers of all CTarget declared in hmake.cpp file.
inline map<int, CTarget *> cTargets;
#ifdef _WIN32
inline constexpr OS os = OS::NT;
#else
inline constexpr OS os = OS::LINUX;
#endif
void setBoolsAndSetRunDir(int argc, char **argv);
inline const string pes = "{}"; // paranthesis-escape-sequence, meant to be used in fmt::print.
void configureOrBuild();
#endif // HMAKE_BUILDSYSTEMFUNCTIONS_HPP
