#ifndef HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#define HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#ifdef USE_HEADER_UNITS
import "HashValues.hpp";
import "OS.hpp";
import "phmap.h";
import "PlatformSpecific.hpp";
import "nlohmann/json.hpp";
#else
#include "HashValues.hpp"
#include "OS.hpp"
#include "PlatformSpecific.hpp"
#include "nlohmann/json.hpp"
#include "phmap.h"
#include <deque>
#include <mutex>
#endif

using std::mutex, std::vector, std::deque, phmap::node_hash_set, phmap::flat_hash_set;


// Named as slashc to avoid collision with a declaration in nlohmann/json which causes warnings. Will be removed later
// when nlohmann/json is removed.
#ifdef _WIN32
inline char slashc = '\\';
#else
inline char slashc = '/';
#endif

struct IndexedNode
{

};
// TODO
// Explore
using Json = nlohmann::json; // Unordered json
inline PDocument targetCache(kArrayType);
inline mutex buildOrConfigCacheMutex;
inline unique_ptr<vector<pchar>> buildCacheFileBuffer;
inline PDocument nodesCacheJson(kArrayType);
inline unique_ptr<vector<pchar>> nodesCacheBuffer;

inline vector<struct BTarget *> roundEndTargets{10};
inline std::atomic<uint64_t> roundEndTargetsCount = 0;

inline auto &ralloc = targetCache.GetAllocator();

// Node representing source directory
inline class Node *srcNode;

// Node representing configure directory
inline Node *configureNode;

inline Node *currentNode;

enum class BSMode : char // Build System Mode
{
    CONFIGURE = 0,
    BUILD = 1,
    IDE = 2,
};

// By default, mode is configure, however, if, --build cmd option is passed, it is set to BUILD.
inline BSMode bsMode = BSMode::CONFIGURE;

// Following can be used for holding memory through build-system run and is used for target<CTarget> in GetTarget
// functions
template <typename T> inline node_hash_set<T> targets;

template <typename T> inline deque<T> targets2;

// Following can be used to hold pointers for all targets in the build system. It is used by CTarget and BTarget
// constructor.
template <typename T> inline flat_hash_set<T *> targetPointers;

#ifdef _WIN32
inline constexpr OS os = OS::NT;
#else
inline constexpr OS os = OS::LINUX;
#endif

inline std::mutex printMutex;

void initializeCache(BSMode bsMode_);
void setBuildSystemModeFromArguments(int argc, char **argv);
inline const pstring dashCpp = "-cpp";
inline const pstring dashLink = "-link";

typedef void (*PrintMessage)(const pstring &message);
typedef void (*PrintMessageColor)(const pstring &message, uint32_t color);

pstring getFileNameJsonOrOut(const pstring &name);
inline PrintMessage printMessagePointer = nullptr;
inline PrintMessageColor printMessageColorPointer = nullptr;
inline PrintMessage printErrorMessagePointer = nullptr;
inline PrintMessageColor printErrorMessageColorPointer = nullptr;

// Provide these with extern "C" linkage as well so ide/editor could pipe the logging.
void printDebugMessage(const pstring &message);
void printMessage(const pstring &message);
void printMessageColor(const pstring &message, uint32_t color);
void printErrorMessage(const pstring &message);
void printErrorMessageColor(const pstring &message, uint32_t color);

#define HMAKE_HMAKE_INTERNAL_ERROR printErrorMessage(fmt::format("HMake Internal Error {} {}", __FILE__, __LINE__));

pstring getLastNameAfterSlash(pstring_view name);
pstring_view getLastNameAfterSlashView(pstring_view name);
pstring removeDashCppFromName(pstring_view name);
bool configureOrBuild();
void constructGlobals();
void destructGlobals();

#define GLOBAL_VARIABLE(type, var)                                                                                     \
    inline char _##var[sizeof(type)];                                                                                  \
    inline type &var = reinterpret_cast<type &>(_##var);

#define STATIC_VARIABLE(type, var)                                                                                     \
    static inline char _##var[sizeof(type)];                                                                           \
    static inline type &var = reinterpret_cast<type &>(_##var);

#endif // HMAKE_BUILDSYSTEMFUNCTIONS_HPP
