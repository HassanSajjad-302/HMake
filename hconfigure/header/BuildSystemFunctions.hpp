#ifndef HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#define HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#ifdef USE_HEADER_UNITS
import "OS.hpp";
import <map>;
import <set>;
import <string>;
import <mutex>;
#else
#include "OS.hpp"
#include <map>
#include <mutex>
#include <set>
#include <string>
#endif

#ifdef WIN32

#else

#define EXPORT __attribute__((visibility("default")))

#endif

using std::string, std::set, std::map, std::mutex;

inline string srcDir;

// path of directory which has configure executable of the project
inline string configureDir;

enum class BSMode : unsigned short // Build System Mode
{
    CONFIGURE = 0,
    BUILD = 1,
    IDE = 2,
};

// By default, mode is configure, however, if, --build cmd option is passed, it is set to BUILD.
inline BSMode bsMode = BSMode::CONFIGURE;

// Used for determining the CTarget to build in BSMode::BUILD. The string is of buildTargetFilePaths.
inline set<string> targetSubDirectories;

// Following can be used for holding memory through build-system run and is used for target<CTarget> in GetTarget
// functions
template <typename T> inline set<T> targets;

// Following can be used to hold pointers for all targets in the build system. It is used by CTarget and BTarget
// constructor.
template <typename T> inline set<T *> targetPointers;

#ifdef _WIN32
inline constexpr OS os = OS::NT;
#else
inline constexpr OS os = OS::LINUX;
#endif

inline std::mutex printMutex;

void initializeCache(BSMode bsMode_);
BSMode getBuildSystemModeFromArguments(int argc, char **argv);
inline const string dashCpp = "-cpp";
inline const string dashLink = "-link";

typedef void (*PrintMessage)(const string &message);
typedef void (*PrintMessageColor)(const string &message, uint32_t color);

extern "C" inline EXPORT PrintMessage printMessagePointer = nullptr;
extern "C" inline EXPORT PrintMessageColor printMessageColorPointer = nullptr;
extern "C" inline EXPORT PrintMessage printErrorMessagePointer = nullptr;
extern "C" inline EXPORT PrintMessageColor printErrorMessageColorPointer = nullptr;

void printMessage(const string &message);
void preintMessageColor(const string &message, uint32_t color);
void printErrorMessage(const string &message);
void printErrorMessageColor(const string &message, uint32_t color);

void configureOrBuild();

#endif // HMAKE_BUILDSYSTEMFUNCTIONS_HPP
