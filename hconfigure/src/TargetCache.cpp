
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
            cacheIndex = fileTargetCaches.size();
            fileTargetCaches.emplace_back().name = name;
        }
        else
        {
            cacheIndex = it->second;
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
        cacheIndex = it->second;
    }
    fileTargetCaches[cacheIndex].targetCache = this;
}

void TargetCache::updateBuildCache(void *ptr)
{
    // Should not be called if a target has not this overridden
    HMAKE_HMAKE_INTERNAL_ERROR
}

void TargetCache::writeBuildCache(vector<char> &buffer)
{
    string_view buildCache = fileTargetCaches[cacheIndex].buildCache;
    buffer.insert(buffer.end(), buildCache.begin(), buildCache.end());
}

void CCOrHash::serialize(vector<char> &buffer) const
{
#ifdef USE_COMMAND_HASH
    writeUint64(buffer, hash);
#else
    writeStringView(buffer, hash);
#endif
}

void CCOrHash::deserialize(const char *ptr, uint32_t &bytesRead)
{
#ifdef USE_COMMAND_HASH
    hash = readUint64(ptr, bytesRead);
#else
    hash = readStringView(ptr, bytesRead);
#endif
}

bool readBool(const char *ptr, uint32_t &bytesRead)
{
    bool result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint8_t readUint8(const char *ptr, uint32_t &bytesRead)
{
    uint8_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint32_t readUint32(const char *ptr, uint32_t &bytesRead)
{
    uint32_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint64_t readUint64(const char *ptr, uint32_t &bytesRead)
{
    uint64_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

string_view readStringView(const char *ptr, uint32_t &bytesRead)
{
    uint32_t strSize = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
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

void BuildCache::Cpp::SourceFile::serialize(vector<char> &buffer) const
{
    writeNode(buffer, node);
    compileCommandWithTool.serialize(buffer);
    writeNodeVector(buffer, headerFiles);
}

void BuildCache::Cpp::SourceFile::deserialize(const char *ptr, uint32_t &bytesRead)
{
    node = readHalfNode(ptr, bytesRead);
    compileCommandWithTool.deserialize(ptr, bytesRead);
    const uint32_t headerSize = readUint32(ptr, bytesRead);
    headerFiles.reserve(headerSize);
    for (uint32_t i = 0; i < headerSize; ++i)
    {
        headerFiles.emplace_back(readHalfNode(ptr, bytesRead));
    }
}

void ModuleFile::SmRules::SingleHeaderUnitDep::serialize(vector<char> &buffer) const
{
    writeNode(buffer, node);
    writeUint32(buffer, targetIndex);
    writeUint32(buffer, myIndex);
}

void ModuleFile::SmRules::SingleHeaderUnitDep::deserialize(const char *ptr, uint32_t &bytesRead)
{
    node = readHalfNode(ptr, bytesRead);
    targetIndex = readUint32(ptr, bytesRead);
    myIndex = readUint32(ptr, bytesRead);
}

void ModuleFile::SmRules::SingleModuleDep::serialize(vector<char> &buffer) const
{
    writeNode(buffer, node);
    writeStringView(buffer, logicalName);
}

void ModuleFile::SmRules::SingleModuleDep::deserialize(const char *ptr, uint32_t &bytesRead)
{
    node = readHalfNode(ptr, bytesRead);
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
}

void ModuleFile::deserialize(const char *ptr, uint32_t &bytesRead)
{
    srcFile.deserialize(ptr, bytesRead);
    smRules.deserialize(ptr, bytesRead);
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

    writeUint32(buffer, imodFiles.size());
    for (const ModuleFile &mf : imodFiles)
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
    const string_view str = fileTargetCaches[targetCacheIndex].buildCache;
    if (str.empty())
    {
        // can happen at config-time when the target is newly added.
        return;
    }
    uint32_t bytesRead = 0;
    srcFiles.resize(readUint32(str.data(), bytesRead));
    for (SourceFile &source : srcFiles)
    {
        source.deserialize(str.data(), bytesRead);
    }
    modFiles.resize(readUint32(str.data(), bytesRead));
    for (ModuleFile &modFile : modFiles)
    {
        modFile.deserialize(str.data(), bytesRead);
    }
    imodFiles.resize(readUint32(str.data(), bytesRead));
    for (ModuleFile &modFile : imodFiles)
    {
        modFile.deserialize(str.data(), bytesRead);
    }
    headerUnits.resize(readUint32(str.data(), bytesRead));
    for (ModuleFile &hud : headerUnits)
    {
        hud.deserialize(str.data(), bytesRead);
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

void writeUint64(vector<char> &buffer, const uint64_t data)
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

void writeNodeVector(vector<char> &buffer, const vector<Node *> &array)
{
    writeUint32(buffer, array.size());
    for (auto &e : array)
    {
        writeNode(buffer, e);
    }
}
