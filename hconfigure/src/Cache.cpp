
#ifdef USE_HEADER_UNITS
import "Cache.hpp";
import "BuildSystemFunctions.hpp";
import "JConsts.hpp";
import "Settings.hpp";
import "fstream";
#else
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "JConsts.hpp"
#include "Settings.hpp"
#include "fstream"
#endif

using std::ifstream, std::ofstream;

Cache::Cache()
{
    constexpr bool isPresentInVSTools = os == OS::NT ? true : false;
    sourceDirectoryPath = "../";
    isCompilerInVSToolsArray = isPresentInVSTools;
    selectedCompilerArrayIndex = 0;
    isLinkerInVSToolsArray = isPresentInVSTools;
    selectedLinkerArrayIndex = 0;
    isArchiverInVSToolsArray = isPresentInVSTools;
    selectedArchiverArrayIndex = 0;
    libraryType = TargetType::LIBRARY_STATIC;
}

void Cache::initializeCacheVariableFromCacheFile()
{
    // Cache from cacheFileJson is read in both of build system modes but is saved only in CONFIGURE mode.

    path filePath = path(configureDir) / "cache.hmake";
    Json cacheFileJsonLocal;
    ifstream(filePath) >> cacheFileJsonLocal;
    *this = cacheFileJsonLocal;
    cacheFileJson = std::move(cacheFileJsonLocal);
    // Settings are saved only if mode is configure.
    if (bsMode == BSMode::CONFIGURE)
    {
        if (!std::filesystem::exists(path(configureDir) / path("settings.hmake")))
        {
            Json settingsJson = Settings{};
            ofstream(path(configureDir) / path("settings.hmake")) << settingsJson.dump(4);
        }
    }
}

void Cache::registerCacheVariables()
{
    // Cache is saved only if mode is configure
    if (bsMode == BSMode::CONFIGURE)
    {
        path filePath = path(configureDir) / "cache.hmake";
        cacheFileJson[JConsts::cacheVariables] = cacheVariables;
        ofstream(filePath) << cacheFileJson.dump(4);
    }
}

void to_json(Json &j, const Cache &cacheLocal)
{
    j[JConsts::sourceDirectory] = cacheLocal.sourceDirectoryPath.generic_string();
    j[JConsts::configuration] = cacheLocal.configurationType;
    j[JConsts::isCompilerInVSToolsArray] = cacheLocal.isCompilerInVSToolsArray;
    j[JConsts::compilerSelectedArrayIndex] = cacheLocal.selectedCompilerArrayIndex;
    j[JConsts::isLinkerInVSToolsArray] = cacheLocal.isLinkerInVSToolsArray;
    j[JConsts::linkerSelectedArrayIndex] = cacheLocal.selectedLinkerArrayIndex;
    j[JConsts::isArchiverInVSToolsArray] = cacheLocal.isArchiverInVSToolsArray;
    j[JConsts::archiverSelectedArrayIndex] = cacheLocal.selectedArchiverArrayIndex;
    j[JConsts::libraryType] = cacheLocal.libraryType;
    j[JConsts::cacheVariables] = cacheLocal.cacheVariables;
    j[JConsts::compileConfigureCommands] = cacheLocal.compileConfigureCommands;
}

void from_json(const Json &j, Cache &cacheLocal)
{
    cacheLocal.sourceDirectoryPath = j.at(JConsts::sourceDirectory).get<path>();
    if (cacheLocal.sourceDirectoryPath.is_relative())
    {
        cacheLocal.sourceDirectoryPath = (configureDir / cacheLocal.sourceDirectoryPath).lexically_normal();
    }

    srcDir = cacheLocal.sourceDirectoryPath.generic_string();

    cacheLocal.configurationType = j.at(JConsts::configuration).get<ConfigType>();
    cacheLocal.isCompilerInVSToolsArray = j.at(JConsts::isCompilerInVSToolsArray).get<bool>();
    cacheLocal.selectedCompilerArrayIndex = j.at(JConsts::compilerSelectedArrayIndex).get<int>();
    cacheLocal.isLinkerInVSToolsArray = j.at(JConsts::isLinkerInVSToolsArray).get<bool>();
    cacheLocal.selectedLinkerArrayIndex = j.at(JConsts::linkerSelectedArrayIndex).get<int>();
    cacheLocal.isArchiverInVSToolsArray = j.at(JConsts::isArchiverInVSToolsArray).get<bool>();
    cacheLocal.selectedArchiverArrayIndex = j.at(JConsts::archiverSelectedArrayIndex).get<int>();
    cacheLocal.libraryType = j.at(JConsts::libraryType).get<TargetType>();
    if (cacheLocal.libraryType != TargetType::LIBRARY_STATIC && cacheLocal.libraryType != TargetType::LIBRARY_SHARED &&
        cache.libraryType != TargetType::LIBRARY_OBJECT)
    {
        printErrorMessage("Cache libraryType TargetType is not one of LIBRARY_STATIC or LIBRARY_SHARED \n");
        throw std::exception();
    }
    cacheLocal.cacheVariables = j.at(JConsts::cacheVariables).get<Json>();
    cacheLocal.compileConfigureCommands = j.at(JConsts::compileConfigureCommands).get<vector<string>>();
}

