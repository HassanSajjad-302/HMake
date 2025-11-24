#ifndef HMAKE_CACHE_HPP
#define HMAKE_CACHE_HPP

#include "BuildSystemFunctions.hpp"
#include "nlohmann/json.hpp"
#include <thread>
#include <vector>

using Json = nlohmann::json;
using std::vector;
struct Cache
{
    Json cacheFileJson;
    string sourceDirectoryPath;
    // isToolInVSToolsArray to be used only on Windows. Determines if the index of tool is in VSTools array or is in
    // plain array. In VSTools array, compiler and linker also have include-dirs and library-dirs with
    // them which are loaded from toolsCache global variable.
    bool isCompilerInToolsArray;
    uint8_t selectedCompilerArrayIndex;
    bool isLinkerInToolsArray;
    uint8_t selectedLinkerArrayIndex;
    bool isArchiverInToolsArray;
    uint8_t selectedArchiverArrayIndex;
    bool isScannerInToolsArray;
    uint8_t selectedScannerArrayIndex;
    uint16_t numberOfBuildThreads = std::thread::hardware_concurrency();
    Json cacheVariables;
    string configureExeBuildScript;
    string buildExeBuildScript;
    Cache();
    void initializeCacheVariableFromCacheFile();
    void registerCacheVariables();
};
void to_json(Json &j, const Cache &cacheLocal);
void from_json(const Json &j, Cache &cacheLocal);

GLOBAL_VARIABLE(Cache, cache)

template <typename T> struct CacheVariable
{
    T value;
    string jsonString;
    CacheVariable(string cacheVariableString_, T defaultValue);
};

template <typename T>
CacheVariable<T>::CacheVariable(string cacheVariableString_, T defaultValue)
    : jsonString(std::move(cacheVariableString_))
{
    Json &cacheVariablesJson = cache.cacheVariables;
    if (cacheVariablesJson.contains(jsonString))
    {
        value = cacheVariablesJson.at(jsonString).get<T>();
    }
    else
    {
        value = defaultValue;
        cacheVariablesJson[jsonString] = value;
    }
}

#endif // HMAKE_CACHE_HPP
