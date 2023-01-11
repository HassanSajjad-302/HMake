#ifndef HMAKE_CACHE_HPP
#define HMAKE_CACHE_HPP

#include "BuildTools.hpp"
#include "Features.hpp"
#include "TargetType.hpp"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>

using Json = nlohmann::ordered_json;
using std::string, std::vector;
struct Cache
{

    ~Cache() noexcept;
    Json cacheFileJson;
    string packageCopyPath;
    ConfigType projectConfigurationType;
    vector<Compiler> compilerArray;
    unsigned selectedCompilerArrayIndex;
    vector<Linker> linkerArray;
    unsigned selectedLinkerArrayIndex;
    vector<Archiver> archiverArray;
    unsigned selectedArchiverArrayIndex;
    enum TargetType libraryType;
    Json cacheVariables;
    Environment environment;
    bool saveCache = false;
    // TODO
    // In Executable, Library and Variant, the default properties are initialized from Cache. Few Properties inherited
    // from Features should also be initialized from Cache
    void initializeCacheVariableFromCacheFile();
    void registerCacheVariables();
};
inline Cache cache;

template <typename T> struct CacheVariable
{
    T value;
    string jsonString;
    CacheVariable(string cacheVariableString_, T defaultValue);
};

template <typename T>
CacheVariable<T>::CacheVariable(string cacheVariableString_, T defaultValue) : jsonString(move(cacheVariableString_))
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
