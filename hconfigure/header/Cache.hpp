#ifndef HMAKE_CACHE_HPP
#define HMAKE_CACHE_HPP
#ifdef USE_HEADER_UNITS
import "ConfigType.hpp";
import "PlatformSpecific.hpp";
import "TargetType.hpp";
#include "nlohmann/json.hpp";
import <vector>;
#else
#include "ConfigType.hpp"
#include "PlatformSpecific.hpp"
#include "TargetType.hpp"
#include "nlohmann/json.hpp"
#include <vector>
#endif

using Json = nlohmann::json;
using std::vector;
struct Cache
{
    Json cacheFileJson;
    string sourceDirectoryPath;
    ConfigType configurationType;
    // isToolInVSToolsArray to be used only on Windows. Determines if the index of tool is in VSTools array or is in
    // plain array. In VSTools array, compiler and linker also have include-directories and library-directories with
    // them which are loaded from toolsCache global variable.
    bool isCompilerInToolsArray;
    unsigned selectedCompilerArrayIndex;
    bool isLinkerInToolsArray;
    unsigned selectedLinkerArrayIndex;
    bool isArchiverInToolsArray;
    unsigned selectedArchiverArrayIndex;
    bool isScannerInToolsArray;
    unsigned selectedScannerArrayIndex;
    enum TargetType libraryType;
    Json cacheVariables;
    vector<pstring> compileConfigureCommands;
    Cache();
    void initializeCacheVariableFromCacheFile();
    void registerCacheVariables();
};
void to_json(Json &j, const Cache &cacheLocal);
void from_json(const Json &j, Cache &cacheLocal);

inline Cache cache;

template <typename T> struct CacheVariable
{
    T value;
    pstring jsonPString;
    CacheVariable(pstring cacheVariableString_, T defaultValue);
};

template <typename T>
CacheVariable<T>::CacheVariable(pstring cacheVariableString_, T defaultValue)
    : jsonPString(std::move(cacheVariableString_))
{
    Json &cacheVariablesJson = cache.cacheVariables;
    if (cacheVariablesJson.contains(jsonPString))
    {
        value = cacheVariablesJson.at(jsonPString).get<T>();
    }
    else
    {
        value = defaultValue;
        cacheVariablesJson[jsonPString] = value;
    }
}

#endif // HMAKE_CACHE_HPP

