#ifndef HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#define HMAKE_BUILDSYSTEMFUNCTIONS_HPP
#ifdef USE_HEADER_UNITS
import "HashValues.hpp";
import "OS.hpp";
import "phmap.h";
import "nlohmann/json.hpp";
#else
#include "HashValues.hpp"
#include "OS.hpp"
#include "nlohmann/json.hpp"
#include "phmap.h"
#include <deque>
#include <mutex>
#endif

using std::mutex, std::vector, std::deque, phmap::node_hash_set, phmap::flat_hash_set, rapidjson::kArrayType,
    rapidjson::kStringType;

// Named as slashc to avoid collision with a declaration in nlohmann/json which causes warnings. Will be removed later
// when nlohmann/json is removed.
#ifdef _WIN32
inline char slashc = '\\';
#else
inline char slashc = '/';
#endif

using Json = nlohmann::json; // Unordered json

inline flat_hash_set<string> cmdTargets;
inline mutex configCacheMutex;
inline Document configCache(kArrayType);
inline unique_ptr<vector<char>> configCacheBuffer;
inline Document buildCache(kArrayType);
inline unique_ptr<vector<char>> buildCacheBuffer;
inline Document nodesCacheJson(kArrayType);
inline unique_ptr<vector<char>> nodesCacheBuffer;

inline auto &ralloc = buildCache.GetAllocator();

// Node representing source dir
inline class Node *srcNode;

// Node representing configure dir
inline Node *configureNode;

inline Node *currentNode;

enum class BSMode : char // Build System Mode
{
    CONFIGURE = 0,
    BUILD = 1,
#ifdef BUILD_MODE
    BOTH = 1,
#else
    BOTH = 0,
#endif
};

// By default, mode is configure, however, if, --build cmd option is passed, it is set to BUILD.

#ifdef BUILD_MODE
inline constexpr BSMode bsMode = BSMode::BUILD;
#else
inline constexpr BSMode bsMode = BSMode::CONFIGURE;
#endif

// Global variable for holding memory
template <typename T> inline deque<T> targets;

// Following can be used to hold pointers for all targets in the build system. It is used by CTarget and BTarget
// constructor.
template <typename T> inline flat_hash_set<T *> targetPointers;

#ifdef _WIN32
inline constexpr OS os = OS::NT;
#else
inline constexpr OS os = OS::LINUX;
#endif

inline string currentMinusConfigure;
void initializeCache(BSMode bsMode_);
inline const string dashCpp = "-cpp";
inline const string dashLink = "-link";

typedef void (*PrintMessage)(const string &message);
typedef void (*PrintMessageColor)(const string &message, uint32_t color);

string getFileNameJsonOrOut(const string &name);
inline PrintMessage printMessagePointer = nullptr;
inline PrintMessageColor printMessageColorPointer = nullptr;
inline PrintMessage printErrorMessagePointer = nullptr;
inline PrintMessageColor printErrorMessageColorPointer = nullptr;

// Provide these with extern "C" linkage as well so ide/editor could pipe the logging.
void printDebugMessage(const string &message);
void printMessage(const string &message);
void printMessageColor(const string &message, uint32_t color);
void printErrorMessage(const string &message);
void printErrorMessageNoReturn(const string &message);
void printErrorMessageColor(const string &message, uint32_t color);

#define HMAKE_HMAKE_INTERNAL_ERROR printErrorMessage(FORMAT("HMake Internal Error {} {}", __FILE__, __LINE__));

string getLastNameAfterSlash(string_view name);
string_view getLastNameAfterSlashV(string_view name);
string getNameBeforeLastSlash(string_view name);
string_view getNameBeforeLastSlashV(string_view name);
string getNameBeforeLastPeriod(string_view name);
string removeDashCppFromName(string_view name);
string_view removeDashCppFromNameSV(string_view name);
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
