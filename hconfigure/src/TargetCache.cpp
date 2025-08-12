
#ifdef USE_HEADER_UNITS
import "TargetCache.hpp";
#else
#include "TargetCache.hpp"
#endif
#include <BuildSystemFunctions.hpp>

namespace
{
uint64_t idCount = 0;
flat_hash_set<string> targetCacheIndexAndMyIdHashMap;
uint64_t myId = 0;

void checkForSameTargetName(const string &targetName)
{
    myId = idCount;
    ++idCount;
    if (auto [pos, ok] = targetCacheIndexAndMyIdHashMap.emplace(targetName); !ok)
    {
        printErrorMessage(FORMAT("Attempting to add 2 targets with same name {} in config-cache.json\n", targetName));
        errorExit();
    }
}
} // namespace

TargetCache::TargetCache(const string &name)
{
    const auto it = nameToIndexMap.find(name);
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (it == nameToIndexMap.end())
        {
            targetCacheIndex = configCacheTargets.size();
            configCacheTargets.emplace_back(name);
        }
        else
        {
            targetCacheIndex = it->second;
        }

        checkForSameTargetName(name);
    }
    else
    {
        if (it == nameToIndexMap.end())
        {
            printErrorMessage(FORMAT(
                "Target {} not found in config-cache.\nMaybe you need to run hhelper first to update the target-cache.",
                name));
            errorExit();
        }
        targetCacheIndex = it->second;
    }
}

void TargetCache::updateBuildCache(void *ptr)
{
    // Should not be called if a target has not this overridden
    HMAKE_HMAKE_INTERNAL_ERROR
}

void TargetCache::writeBuildCache(vector<char> &buffer)
{
}

bool readBool(const char *ptr, uint32_t &bytesRead)
{
    bool result;
    memcpy(&result, ptr, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint32_t readUint32(const char *ptr, uint32_t &bytesRead)
{
    uint32_t result;
    memcpy(&result, ptr, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

string_view readStringView(const char *ptr, uint32_t &bytesRead)
{
    uint32_t strSize = readUint32(ptr, bytesRead);
    uint32_t offset = bytesRead;
    bytesRead += strSize;
    return {ptr + offset, strSize};
}

Node *readHalfNode(const char *ptr, uint32_t &bytesRead)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    return Node::getHalfNode(readUint32(ptr, bytesRead));
#else
    return Node::getHalfNode(readStringView(ptr, bytesRead));
#endif
}

CCOrHash readCCOrHash(const char *ptr, uint32_t &bytesRead)
{
    CCOrHash cmd;
#ifdef USE_COMMAND_HASH
    cmd.hash = readUint32(ptr, bytesRead);
#else
    cmd.compilerCommand = readStringView(ptr, bytesRead);
#endif
    return cmd;
}

span<Node *> readNodeSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(Node *);
    return {(Node **)(ptr + offset), count};
}

ConfigCache::Cpp readCppConfigCache(const char *ptr, uint32_t &bytesRead)
{
    ConfigCache::Cpp cfg;
    cfg.reqInclsArray = readNodeSpan(ptr, bytesRead);
    cfg.useReqInclsArray = readNodeSpan(ptr, bytesRead);
    cfg.reqHUDirsArray = readNodeSpan(ptr, bytesRead);
    cfg.useReqHUDirsArray = readNodeSpan(ptr, bytesRead);
    cfg.sourceFiles = readNodeSpan(ptr, bytesRead);
    cfg.moduleFiles = readNodeSpan(ptr, bytesRead);
    cfg.headerUnits = readNodeSpan(ptr, bytesRead);
    cfg.buildCacheFilesDirPath = readHalfNode(ptr, bytesRead);
    return cfg;
}

ConfigCache::Link readLinkConfigCache(const char *ptr, uint32_t &bytesRead)
{
    ConfigCache::Link ln;
    ln.reqLibraryDirsArray = readNodeSpan(ptr, bytesRead);
    ln.useReqLibraryDirsArray = readNodeSpan(ptr, bytesRead);
    ln.outputFileNode = readHalfNode(ptr, bytesRead);
    ln.buildCacheFilesDirPath = readHalfNode(ptr, bytesRead);
    return ln;
}

BuildCache::Cpp::SourceFile readSourceFileBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::SourceFile sf;
    return sf;
}

void BuildCache::Cpp::SourceFile::serialize(vector<char> &buffer) const
{
    writeNode(buffer, node);
    writeCCOrHash(buffer, compileCommandWithTool);
    writeNodeVector(buffer, headerFiles);
}

void BuildCache::Cpp::SourceFile::deserialize(const char *ptr, uint32_t &bytesRead)
{
    node = readHalfNode(ptr, bytesRead);
    compileCommandWithTool = readCCOrHash(ptr, bytesRead);
    headerFiles.reserve(readUint32(ptr, bytesRead));
    for (Node *&n2 : headerFiles)
    {
        n2 = readHalfNode(ptr, bytesRead);
    }
}

void ModuleFile::SmRules::SingleHeaderUnitDep::serialize(vector<char> &buffer) const
{
    writeNode(buffer, node);
    writeBool(buffer, angle);
    writeUint32(buffer, targetIndex);
    writeUint32(buffer, myIndex);
}

void ModuleFile::SmRules::SingleHeaderUnitDep::deserialize(const char *ptr, uint32_t &bytesRead)
{
    node = readHalfNode(ptr, bytesRead);
    angle = readBool(ptr, bytesRead);
    targetIndex = readUint32(ptr, bytesRead);
    myIndex = readUint32(ptr, bytesRead);
}

void ModuleFile::SmRules::SingleModuleDep::serialize(vector<char> &buffer) const
{
    writeNode(buffer, fullPath);
    writeStringView(buffer, logicalName);
}

void ModuleFile::SmRules::SingleModuleDep::deserialize(const char *ptr, uint32_t &bytesRead)
{
    fullPath = readHalfNode(ptr, bytesRead);
    logicalName = readStringView(ptr, bytesRead);
}

void ModuleFile::SmRules::serialize(vector<char> &buffer) const
{
    writeStringView(buffer, exportName);
    writeBool(buffer, isInterface);

    writeUint32(buffer, headerUnitArray.size());
    for (const SingleHeaderUnitDep &h : headerUnitArray)
    {
       h.serialize(buffer);
    }

    writeUint32(buffer, moduleArray.size());
    for (const SingleModuleDep &m : moduleArray)
    {
        m.serialize(buffer);
    }
}

void ModuleFile::SmRules::deserialize(const char *ptr, uint32_t &bytesRead)
{
    exportName = readStringView(ptr, bytesRead);
    isInterface = readBool(ptr, bytesRead);
    headerUnitArray.resize(readUint32(ptr, bytesRead));
    for (SingleHeaderUnitDep &hud : headerUnitArray)
    {
        hud.deserialize(ptr, bytesRead);
    }
    moduleArray.resize(readUint32(ptr, bytesRead));
    for (SingleModuleDep &modDep : moduleArray)
    {
        modDep.deserialize(ptr, bytesRead);
    }
}

void ModuleFile::deserialize(const char *ptr, uint32_t &bytesRead)
{
    srcFile.deserialize(ptr, bytesRead);
    smRules.deserialize(ptr, bytesRead);
    compileCommandWithTool = readCCOrHash(ptr, bytesRead);
}

void BuildCache::Cpp::serialize(vector<char> &buffer) const
{
    writeUint32(buffer, srcFiles.size());
    for (const SourceFile &sf : srcFiles)
    {
        sf.serialize(buffer);
    }

    writeUint32(buffer, modFiles.size());
    for (const ModuleFile &mf : modFiles)
    {
        mf.serialize(buffer);
    }

    writeUint32(buffer, headerUnits.size());
    for (const ModuleFile &hud : headerUnits)
    {
        hud.serialize(buffer);
    }
}

void BuildCache::Cpp::deserialize(uint32_t targetCacheIndex)
{
    span<char> configCache = configCacheTargets[targetCacheIndex].configCache;
    uint32_t bytesRead = 0;
    srcFiles.resize(readUint32(configCache.data(), bytesRead));
    for (SourceFile &source : srcFiles)
    {
        source.deserialize(configCache.data(), bytesRead);
    }
    modFiles.resize(readUint32(configCache.data(), bytesRead));
    for (ModuleFile &modFile : modFiles)
    {
        modFile.deserialize(configCache.data(), bytesRead);
    }
    headerUnits.resize(readUint32(configCache.data(), bytesRead));
    for (ModuleFile &hud : headerUnits)
    {
        hud.deserialize(configCache.data(), bytesRead);
    }
}

BuildCache::Link readLinkBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Link ln;
    ln.commandWithoutArgumentsWithTools = readCCOrHash(ptr, bytesRead);
    ln.objectFiles = readNodeSpan(ptr, bytesRead);
    return ln;
}

void writeBool(vector<char> &buffer, const bool &value)
{
    buffer.emplace_back(static_cast<char>(value));
}

void writeUint32(vector<char> &buffer, const uint32_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(data));
}

void writeStringView(vector<char> &buffer, const string_view &data)
{
    writeUint32(buffer, data.size());
    buffer.insert(buffer.end(), data.begin(), data.end());
}

void writeNode(vector<char> &buffer, const Node *value)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    writeUint32(buffer, value.index);
#else
    writeStringView(buffer, value.filePath);
#endif
}

void writeCCOrHash(vector<char> &buffer, const CCOrHash &value)
{
#ifdef USE_COMMAND_HASH
    writeUint32(buffer, value.hash);
#else
    writeStringView(buffer, value.compilerCommand);
#endif
}

void writeNodeVector(vector<char> &buffer, const vector<Node *> &array)
{
    writeUint32(buffer, array.size());
    for (auto &e : array)
    {
        writeNode(buffer, e);
    }
}

void writeCppConfigCache(vector<char> &buffer, const ConfigCache::Cpp &data)
{
    writeNodeSpan(buffer, data.reqInclsArray);
    writeNodeSpan(buffer, data.useReqInclsArray);
    writeNodeVector(buffer, data.reqHUDirsArray);
    writeNodeVector(buffer, data.useReqHUDirsArray);
    writeNodeVector(buffer, data.sourceFiles);
    writeNodeVector(buffer, data.moduleFiles);
    writeNodeVector(buffer, data.headerUnits);
    writeNode *(buffer, data.buildCacheFilesDirPath);
}

void writeLinkConfigCache(vector<char> &buffer, const ConfigCache::Link &data)
{
    writeNodeVector(buffer, data.reqLibraryDirsArray);
    writeNodeVector(buffer, data.useReqLibraryDirsArray);
    writeNode(buffer, data.outputFileNode);
    writeNode(buffer, data.buildCacheFilesDirPath);
}

void writeSourceFileBuildCache(vector<char> &buffer, const BuildCache::Cpp::SourceFile &data)
{
}

void writeSourceFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::SourceFile> &data)
{
    writeUint32(buffer, static_cast<uint32_t>(data.size()));
    for (auto &e : data)
    {
        writeSourceFileBuildCache(buffer, e);
    }
}

void writeSingleHeaderUnitDep(vector<char> &buffer,
                              const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &data)
{
}

void writeSingleModuleDep(vector<char> &buffer, const BuildCache::Cpp::ModuleFile::SmRules::SingleModuleDep &data)
{
    writeNode *(buffer, data.fullPath);
    writeStringView(buffer, data.logicalName);
}

void writeSMRules(vector<char> &buffer, const BuildCache::Cpp::ModuleFile::SmRules &data)
{
    writeStringView(buffer, data.exportName);
    writeBool(buffer, data.isInterface);

    writeUint32(buffer, static_cast<uint32_t>(data.headerUnitArray.size()));
    for (auto &h : data.headerUnitArray)
    {
        writeSingleHeaderUnitDep(buffer, h);
    }

    writeUint32(buffer, static_cast<uint32_t>(data.moduleArray.size()));
    for (auto &m : data.moduleArray)
    {
        writeSingleModuleDep(buffer, m);
    }
}

void writeModuleFileBuildCache(vector<char> &buffer, const BuildCache::Cpp::ModuleFile &data)
{
    writeSourceFileBuildCache(buffer, data.srcFile);
    writeNodeVector(buffer, data.headerFiles);
    writeSMRules(buffer, data.smRules);
    writeCCOrHash(buffer, data.compileCommandWithTool);
}

void writeModuleFileBuildCacheSpan(vector<char> &buffer, const span<BuildCache::Cpp::ModuleFile> &data)
{
    writeUint32(buffer, static_cast<uint32_t>(data.size()));
    for (auto &e : data)
    {
        writeModuleFileBuildCache(buffer, e);
    }
}

void writeCppBuildCache(vector<char> &buffer, const BuildCache::Cpp &data)
{
    writeSourceFileBuildCacheSpan(buffer, data.srcFiles);
    writeModuleFileBuildCacheSpan(buffer, data.modFiles);
    writeModuleFileBuildCacheSpan(buffer, data.headerUnits);
}

void writeLinkBuildCache(vector<char> &buffer, const BuildCache::Link &data)
{
    writeCCOrHash(buffer, data.commandWithoutArgumentsWithTools);
    writeNodeVector(buffer, data.objectFiles);
}
