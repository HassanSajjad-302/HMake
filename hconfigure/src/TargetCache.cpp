
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
            cahceIndex = fileTargetCaches.size();
            fileTargetCaches.emplace_back().name = name;
        }
        else
        {
            cahceIndex = it->second;
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
        cahceIndex = it->second;
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

void ModuleFile::serialize(vector<char> &buffer) const
{
    srcFile.serialize(buffer);
    smRules.serialize(buffer);
    writeCCOrHash(buffer, compileCommandWithTool);
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

void BuildCache::Cpp::deserialize(const uint32_t targetCacheIndex)
{
    const string_view configCache = fileTargetCaches[targetCacheIndex].configCache;
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

void writeBool(vector<char> &buffer, const bool &value)
{
    buffer.emplace_back(static_cast<char>(value));
}

void writeUint8(vector<char> &buffer, const uint8_t &data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(data));
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

void writeNode(vector<char> &buffer, const Node *node)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    writeUint32(buffer, node->myId);
#else
    writeStringView(buffer, node->filePath);
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
