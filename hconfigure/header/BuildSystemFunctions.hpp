#ifndef HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#define HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#ifdef USE_HEADER_UNITS
import "OS.hpp";
import <map>;
import <mutex>;
import "PlatformSpecific.hpp";
import "nlohmann/json.hpp";
import <set>;
#else
#include "OS.hpp"
#include "PlatformSpecific.hpp"
#include "nlohmann/json.hpp"
#include <map>
#include <mutex>
#include <set>
#endif

using std::set, std::map, std::mutex, std::vector;

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

// Named as slashc to avoid collision with a declaration in nlohmann/json which causes warnings. Will be removed later
// when nlohmann/json is removed.
#ifdef _WIN32
inline char slashc = '\\';
#else
inline char slashc = '/';
#endif

// TODO
// Hashing should be used for compile-command as explained here https://aosabook.org/en/posa/ninja.html
// File paths could be hashed as-well, if impactful. Other formats besides json could be explored for faster loading and
// processing.
using Json = nlohmann::json; // Unordered json
inline PDocument buildCache(kArrayType);
inline unique_ptr<char[]> buildCacheFileBuffer;
inline PDocument sourceDirectoryCache(kArrayType);
inline unique_ptr<char[]> sourceDirectoryCacheBuffer;
inline auto &ralloc = buildCache.GetAllocator();
inline std::mutex buildCacheMutex;
void writeBuildCacheUnlocked();

inline pstring srcDir;

// path of directory which has configure executable of the project
inline pstring configureDir;

enum class BSMode : char // Build System Mode
{
    CONFIGURE = 0,
    BUILD = 1,
    IDE = 2,
};

// By default, mode is configure, however, if, --build cmd option is passed, it is set to BUILD.
inline BSMode bsMode = BSMode::CONFIGURE;

// Used for determining the CTarget to build in BSMode::BUILD. The pstring is of buildTargetFilePaths.
inline set<pstring> targetSubDirectories;

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

// presort will be called for such targets in 3 rounds. Currently, CppSourceTarget and PrebuiltBasic is added.
inline vector<struct BTarget *> preSortBTargets;
void initializeCache(BSMode bsMode_);
BSMode getBuildSystemModeFromArguments(int argc, char **argv);
inline const pstring dashCpp = "-cpp";
inline const pstring dashLink = "-link";

typedef void (*PrintMessage)(const pstring &message);
typedef void (*PrintMessageColor)(const pstring &message, uint32_t color);

extern "C" inline EXPORT PrintMessage printMessagePointer = nullptr;
extern "C" inline EXPORT PrintMessageColor printMessageColorPointer = nullptr;
extern "C" inline EXPORT PrintMessage printErrorMessagePointer = nullptr;
extern "C" inline EXPORT PrintMessageColor printErrorMessageColorPointer = nullptr;

// Provide these with extern "C" linkage as well so ide/editor could pipe the logging.
void printDebugMessage(const pstring &message);
void printMessage(const pstring &message);
void preintMessageColor(const pstring &message, uint32_t color);
void printErrorMessage(const pstring &message);
void printErrorMessageColor(const pstring &message, uint32_t color);

#define HMAKE_HMAKE_INTERNAL_ERROR printErrorMessage(fmt::format("HMake Internal Error {} {}", __FILE__, __LINE__));

void configureOrBuild();

#endif // HMAKE_BUILDSYSTEMFUNCTIONS_HPP
