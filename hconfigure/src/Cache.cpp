#include "Cache.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "Node.hpp"
#include <fstream>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <thread>

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
    numberOfBuildProcesses = std::thread::hardware_concurrency();
}

void Cache::initializeCacheVariableFromCacheFile()
{
    const path filePath = path(configureNode->filePath + slashc + "cache.json");
    if (!std::filesystem::exists(filePath))
    {
        cacheFileJson.SetObject();
        return;
    }
    ifstream ifs(filePath);
    string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    cacheFileJson.Parse(content.c_str());
    if (cacheFileJson.HasParseError())
    {
        cacheFileJson.SetObject();
        return;
    }

    if (cacheFileJson.HasMember(JConsts::sourceDirectory.c_str()))
    {
        sourceDirectoryPath = cacheFileJson[JConsts::sourceDirectory.c_str()].GetString();
        path srcPath = path(sourceDirectoryPath);
        if (srcPath.is_relative())
        {
            srcPath = path(configureNode->filePath + slashc + sourceDirectoryPath);
            srcPath = srcPath.lexically_normal();
            srcPath = srcPath.parent_path();
        }
        srcNode = Node::getHalfNode(srcPath.string());
        normalizationBasePath = srcNode->filePath;
    }

    if (cacheFileJson.HasMember(JConsts::isCompilerInToolsArray.c_str()))
        isCompilerInToolsArray = cacheFileJson[JConsts::isCompilerInToolsArray.c_str()].GetBool();
    if (cacheFileJson.HasMember(JConsts::compilerSelectedArrayIndex.c_str()))
        selectedCompilerArrayIndex =
            static_cast<uint8_t>(cacheFileJson[JConsts::compilerSelectedArrayIndex.c_str()].GetUint());
    if (cacheFileJson.HasMember(JConsts::isLinkerInToolsArray.c_str()))
        isLinkerInToolsArray = cacheFileJson[JConsts::isLinkerInToolsArray.c_str()].GetBool();
    if (cacheFileJson.HasMember(JConsts::linkerSelectedArrayIndex.c_str()))
        selectedLinkerArrayIndex =
            static_cast<uint8_t>(cacheFileJson[JConsts::linkerSelectedArrayIndex.c_str()].GetUint());
    if (cacheFileJson.HasMember(JConsts::isArchiverInToolsArray.c_str()))
        isArchiverInToolsArray = cacheFileJson[JConsts::isArchiverInToolsArray.c_str()].GetBool();
    if (cacheFileJson.HasMember(JConsts::archiverSelectedArrayIndex.c_str()))
        selectedArchiverArrayIndex =
            static_cast<uint8_t>(cacheFileJson[JConsts::archiverSelectedArrayIndex.c_str()].GetUint());
    if (cacheFileJson.HasMember(JConsts::isScannerInToolsArray.c_str()))
        isScannerInToolsArray = cacheFileJson[JConsts::isScannerInToolsArray.c_str()].GetBool();
    if (cacheFileJson.HasMember(JConsts::scannerSelectedArrayIndex.c_str()))
        selectedScannerArrayIndex =
            static_cast<uint8_t>(cacheFileJson[JConsts::scannerSelectedArrayIndex.c_str()].GetUint());
    if (cacheFileJson.HasMember(JConsts::numberOfBuildThreads.c_str()))
        numberOfBuildProcesses = static_cast<uint16_t>(cacheFileJson[JConsts::numberOfBuildThreads.c_str()].GetUint());
    if (cacheFileJson.HasMember(JConsts::configureExeBuildScript.c_str()))
        configureExeBuildScript = cacheFileJson[JConsts::configureExeBuildScript.c_str()].GetString();
    if (cacheFileJson.HasMember(JConsts::buildExeBuildScript.c_str()))
        buildExeBuildScript = cacheFileJson[JConsts::buildExeBuildScript.c_str()].GetString();
}

void Cache::registerCacheVariables()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const path filePath = path(configureNode->filePath + slashc + "cache.json");
        auto &doc = cacheFileJson;
        if (!doc.IsObject())
        {
            doc.SetObject();
        }

        auto set_member = [&](const string &key, auto val) {
            if (doc.HasMember(key.c_str()))
            {
                doc.RemoveMember(key.c_str());
            }
            rapidjson::Value k(key.c_str(), doc.GetAllocator());
            rapidjson::Value v;
            if constexpr (std::is_same_v<decltype(val), bool>)
            {
                v.SetBool(val);
            }
            else if constexpr (std::is_same_v<decltype(val), string>)
            {
                v.SetString(val.c_str(), val.length(), doc.GetAllocator());
            }
            else
            {
                v.SetUint(val);
            }
            doc.AddMember(k, v, doc.GetAllocator());
        };

        set_member(JConsts::sourceDirectory, sourceDirectoryPath);
        set_member(JConsts::isCompilerInToolsArray, isCompilerInToolsArray);
        set_member(JConsts::compilerSelectedArrayIndex, selectedCompilerArrayIndex);
        set_member(JConsts::isLinkerInToolsArray, isLinkerInToolsArray);
        set_member(JConsts::linkerSelectedArrayIndex, selectedLinkerArrayIndex);
        set_member(JConsts::isArchiverInToolsArray, isArchiverInToolsArray);
        set_member(JConsts::archiverSelectedArrayIndex, selectedArchiverArrayIndex);
        set_member(JConsts::isScannerInToolsArray, isScannerInToolsArray);
        set_member(JConsts::scannerSelectedArrayIndex, selectedScannerArrayIndex);
        set_member(JConsts::numberOfBuildThreads, numberOfBuildProcesses);
        set_member(JConsts::configureExeBuildScript, configureExeBuildScript);
        set_member(JConsts::buildExeBuildScript, buildExeBuildScript);

        ofstream ofs(filePath);
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        ofs << buffer.GetString();
    }
}
