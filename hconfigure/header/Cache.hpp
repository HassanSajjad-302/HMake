#ifndef HMAKE_CACHE_HPP
#define HMAKE_CACHE_HPP

#include "ConfigType.hpp"
#include "TargetType.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <string>
#include <vector>

using Json = nlohmann::ordered_json;
using std::string, std::vector, std::filesystem::path;
struct Cache
{
    Json cacheFileJson;
    path sourceDirectoryPath = "../";
    ConfigType configurationType;
    // isToolInVSToolsArray to be used only on Windows. Determines if the index of tool is in VSTools array or is in
    // plain array. In VSTools array, compiler and linker also have include-directories and library-directories with
    // them which are loaded from toolsCache global variable.
    bool isCompilerInVSToolsArray;
    unsigned selectedCompilerArrayIndex;
    bool isLinkerInVSToolsArray;
    unsigned selectedLinkerArrayIndex;
    bool isArchiverInVSToolsArray;
    unsigned selectedArchiverArrayIndex;
    enum TargetType libraryType;
    Json cacheVariables;
    vector<string> compileConfigureCommands;
    Cache();
    // TODO
    // In Executable, Library and Variant, the default properties are initialized from Cache. Few Properties inherited
    // from Features should also be initialized from Cache
    void initializeCacheVariableFromCacheFile();
    void registerCacheVariables();
};
void to_json(Json &j, const Cache &cacheLocal);
void from_json(const Json &j, Cache &cacheLocal);

inline Cache cache;

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
        cacheVariablesJson["FILE1"] = defaultValue;
    }
}

#endif // HMAKE_CACHE_HPP
