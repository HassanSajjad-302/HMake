
#ifdef USE_HEADER_UNITS
import "Cache.hpp";
import "BuildSystemFunctions.hpp";
import "JConsts.hpp";
import "Node.hpp";
import "Settings.hpp";
import <fstream>;
#else
#include "Cache.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "Node.hpp"
#include "Settings.hpp"
#include <fstream>
#endif

using std::ifstream, std::ofstream;

Cache::Cache()
{
    constexpr bool isPresentInTools = os == OS::NT ? true : false;
    sourceDirectoryPath = "..";
    isCompilerInToolsArray = true;
    selectedCompilerArrayIndex = 0;
    isLinkerInToolsArray = isPresentInTools;
    selectedLinkerArrayIndex = 0;
    isArchiverInToolsArray = isPresentInTools;
    selectedArchiverArrayIndex = 0;
    isScannerInToolsArray = isPresentInTools;
    selectedScannerArrayIndex = 0;
    libraryType = TargetType::LIBRARY_STATIC;
    configurationType = ConfigType::RELEASE;
}

void Cache::initializeCacheVariableFromCacheFile()
{
    // Cache from cacheFileJson is read in both of build system modes but is saved only in CONFIGURE mode.

    const path filePath = path(configureNode->filePath + slashc + "cache.json");
    Json cacheFileJsonLocal;
    ifstream(filePath) >> cacheFileJsonLocal;
    *this = cacheFileJsonLocal;
    cacheFileJson = std::move(cacheFileJsonLocal);
    // Settings are saved only if mode is configure.
    if (bsMode == BSMode::CONFIGURE)
    {
        if (const path p = path(configureNode->filePath + slashc + "settings.json"); !exists(p))
        {
            const Json settingsJson = Settings{};
            ofstream(p) << settingsJson.dump(4);
        }
    }
}

void Cache::registerCacheVariables()
{
    // Cache is saved only if mode is configure
    if (bsMode == BSMode::CONFIGURE)
    {
        const path filePath = path(configureNode->filePath + slashc + "cache.json");
        cacheFileJson[JConsts::cacheVariables] = cacheVariables;
        ofstream(filePath) << cacheFileJson.dump(4);
    }
}

void to_json(Json &j, const Cache &cacheLocal)
{
    j[JConsts::sourceDirectory] = cacheLocal.sourceDirectoryPath;
    j[JConsts::configuration] = cacheLocal.configurationType;
    j[JConsts::isCompilerInToolsArray] = cacheLocal.isCompilerInToolsArray;
    j[JConsts::compilerSelectedArrayIndex] = cacheLocal.selectedCompilerArrayIndex;
    j[JConsts::isLinkerInToolsArray] = cacheLocal.isLinkerInToolsArray;
    j[JConsts::linkerSelectedArrayIndex] = cacheLocal.selectedLinkerArrayIndex;
    j[JConsts::isArchiverInToolsArray] = cacheLocal.isArchiverInToolsArray;
    j[JConsts::archiverSelectedArrayIndex] = cacheLocal.selectedArchiverArrayIndex;
    j[JConsts::isScannerInToolsArray] = cacheLocal.isScannerInToolsArray;
    j[JConsts::scannerSelectedArrayIndex] = cacheLocal.selectedScannerArrayIndex;
    j[JConsts::libraryType] = cacheLocal.libraryType;
    j[JConsts::cacheVariables] = cacheLocal.cacheVariables;
    j[JConsts::compileConfigureCommands] = cacheLocal.compileConfigureCommands;
}

void from_json(const Json &j, Cache &cacheLocal)
{
    cacheLocal.sourceDirectoryPath = j.at(JConsts::sourceDirectory).get<pstring>();
    path srcPath = path(cacheLocal.sourceDirectoryPath);
    if (srcPath.is_relative())
    {
        srcPath = path(configureNode->filePath + slashc + cacheLocal.sourceDirectoryPath);
        srcPath = srcPath.lexically_normal();
        srcPath = srcPath.parent_path();
    }

    srcNode = Node::getNodeFromNonNormalizedPath(srcPath, false);

    cacheLocal.configurationType = j.at(JConsts::configuration).get<ConfigType>();
    cacheLocal.isCompilerInToolsArray = j.at(JConsts::isCompilerInToolsArray).get<bool>();
    cacheLocal.selectedCompilerArrayIndex = j.at(JConsts::compilerSelectedArrayIndex).get<int>();
    cacheLocal.isLinkerInToolsArray = j.at(JConsts::isLinkerInToolsArray).get<bool>();
    cacheLocal.selectedLinkerArrayIndex = j.at(JConsts::linkerSelectedArrayIndex).get<int>();
    cacheLocal.isArchiverInToolsArray = j.at(JConsts::isArchiverInToolsArray).get<bool>();
    cacheLocal.selectedArchiverArrayIndex = j.at(JConsts::archiverSelectedArrayIndex).get<int>();
    cacheLocal.isScannerInToolsArray = j.at(JConsts::isScannerInToolsArray).get<bool>();
    cacheLocal.selectedScannerArrayIndex = j.at(JConsts::scannerSelectedArrayIndex).get<int>();
    cacheLocal.libraryType = j.at(JConsts::libraryType).get<TargetType>();
    if (cacheLocal.libraryType != TargetType::LIBRARY_STATIC && cacheLocal.libraryType != TargetType::LIBRARY_SHARED &&
        cache.libraryType != TargetType::LIBRARY_OBJECT)
    {
        printErrorMessage("Cache libraryType TargetType is not one of LIBRARY_STATIC or LIBRARY_SHARED \n");
        throw std::exception();
    }
    cacheLocal.cacheVariables = j.at(JConsts::cacheVariables).get<Json>();
    cacheLocal.compileConfigureCommands = j.at(JConsts::compileConfigureCommands).get<vector<pstring>>();
}
