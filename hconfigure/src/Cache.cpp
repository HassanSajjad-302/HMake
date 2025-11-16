
#include "Cache.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "Node.hpp"
#include <fstream>

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
}

void Cache::initializeCacheVariableFromCacheFile()
{
    // Cache from cacheFileJson is read in both of build system modes but is saved only in CONFIGURE mode.

    const path filePath = path(configureNode->filePath + slashc + "cache.json");
    Json cacheFileJsonLocal;
    ifstream(filePath) >> cacheFileJsonLocal;
    *this = cacheFileJsonLocal;
    cacheFileJson = std::move(cacheFileJsonLocal);
}

void Cache::registerCacheVariables()
{
    // Cache is saved only if mode is configure
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const path filePath = path(configureNode->filePath + slashc + "cache.json");
        cacheFileJson[JConsts::cacheVariables] = cacheVariables;
        ofstream(filePath) << cacheFileJson.dump(4);
    }
}

void to_json(Json &j, const Cache &cacheLocal)
{
    j[JConsts::sourceDirectory] = cacheLocal.sourceDirectoryPath;
    j[JConsts::isCompilerInToolsArray] = cacheLocal.isCompilerInToolsArray;
    j[JConsts::compilerSelectedArrayIndex] = cacheLocal.selectedCompilerArrayIndex;
    j[JConsts::isLinkerInToolsArray] = cacheLocal.isLinkerInToolsArray;
    j[JConsts::linkerSelectedArrayIndex] = cacheLocal.selectedLinkerArrayIndex;
    j[JConsts::isArchiverInToolsArray] = cacheLocal.isArchiverInToolsArray;
    j[JConsts::archiverSelectedArrayIndex] = cacheLocal.selectedArchiverArrayIndex;
    j[JConsts::isScannerInToolsArray] = cacheLocal.isScannerInToolsArray;
    j[JConsts::scannerSelectedArrayIndex] = cacheLocal.selectedScannerArrayIndex;
    j[JConsts::numberOfBuildThreads] = cacheLocal.numberOfBuildThreads;
    j[JConsts::cacheVariables] = cacheLocal.cacheVariables;
    j[JConsts::configureExeBuildScript] = cacheLocal.configureExeBuildScript;
    j[JConsts::buildExeBuildScript] = cacheLocal.buildExeBuildScript;
}

void from_json(const Json &j, Cache &cacheLocal)
{
    cacheLocal.sourceDirectoryPath = j.at(JConsts::sourceDirectory).get<string>();
    path srcPath = path(cacheLocal.sourceDirectoryPath);
    if (srcPath.is_relative())
    {
        srcPath = path(configureNode->filePath + slashc + cacheLocal.sourceDirectoryPath);
        srcPath = srcPath.lexically_normal();
        srcPath = srcPath.parent_path();
    }

    srcNode = Node::getNodeNonNormalized(srcPath.string(), false);

    cacheLocal.isCompilerInToolsArray = j.at(JConsts::isCompilerInToolsArray).get<bool>();
    cacheLocal.selectedCompilerArrayIndex = j.at(JConsts::compilerSelectedArrayIndex).get<uint8_t>();
    cacheLocal.isLinkerInToolsArray = j.at(JConsts::isLinkerInToolsArray).get<bool>();
    cacheLocal.selectedLinkerArrayIndex = j.at(JConsts::linkerSelectedArrayIndex).get<uint8_t>();
    cacheLocal.isArchiverInToolsArray = j.at(JConsts::isArchiverInToolsArray).get<bool>();
    cacheLocal.selectedArchiverArrayIndex = j.at(JConsts::archiverSelectedArrayIndex).get<uint8_t>();
    cacheLocal.isScannerInToolsArray = j.at(JConsts::isScannerInToolsArray).get<bool>();
    cacheLocal.selectedScannerArrayIndex = j.at(JConsts::scannerSelectedArrayIndex).get<uint8_t>();
    cacheLocal.numberOfBuildThreads = j.at(JConsts::numberOfBuildThreads).get<uint16_t>();
    cacheLocal.cacheVariables = j.at(JConsts::cacheVariables).get<Json>();
    cacheLocal.configureExeBuildScript = j.at(JConsts::configureExeBuildScript).get<string>();
    cacheLocal.buildExeBuildScript = j.at(JConsts::buildExeBuildScript).get<string>();
}
