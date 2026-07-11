#ifndef HMAKE_CACHE_HPP
#define HMAKE_CACHE_HPP

#include "BuildSystemFunctions.hpp"
#include <rapidjson/document.h>
#include <vector>
#include <type_traits>

using std::vector;
struct Cache
{
    rapidjson::Document cacheFileJson;
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
    uint16_t numberOfBuildProcesses;
    string configureExeBuildScript;
    string buildExeBuildScript;
    Cache();
    void initializeCacheVariableFromCacheFile();
    void registerCacheVariables();
};

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
    auto& doc = cache.cacheFileJson;
    if (!doc.IsObject())
    {
        doc.SetObject();
    }
    if (!doc.HasMember("cache-variables"))
    {
        rapidjson::Value key("cache-variables", doc.GetAllocator());
        rapidjson::Value val(rapidjson::kObjectType);
        doc.AddMember(key, val, doc.GetAllocator());
    }
    auto& cacheVars = doc["cache-variables"];
    if (cacheVars.HasMember(jsonString.c_str()))
    {
        const auto& member = cacheVars[jsonString.c_str()];
        if constexpr (std::is_same_v<T, bool>)
        {
            value = member.GetBool();
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            value = member.GetString();
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            value = member.GetInt();
        }
    }
    else
    {
        value = defaultValue;
        rapidjson::Value key(jsonString.c_str(), doc.GetAllocator());
        rapidjson::Value val;
        if constexpr (std::is_same_v<T, bool>)
        {
            val.SetBool(defaultValue);
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            val.SetString(defaultValue.c_str(), doc.GetAllocator());
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            val.SetInt(defaultValue);
        }
        cacheVars.AddMember(key, val, doc.GetAllocator());
    }
}

#endif // HMAKE_CACHE_HPP
