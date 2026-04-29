
#include "TargetCache.hpp"
#include "BuildSystemFunctions.hpp"
#include "Node.hpp"

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

bool TargetCache::writeBuildCache(string &buffer)
{
    const string_view buildCache = fileTargetCaches[cacheIndex].buildCache;
    buffer.append(buildCache.begin(), buildCache.end());
    return false;
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
    return nodeIndices[readUint32(ptr, bytesRead)];
#else
    return Node::getHalfNode(readStringView(ptr, bytesRead));
#endif
}

void BuildCache::Cpp::SourceFile::serialize(string &buffer) const
{
    writeNode(buffer, node);
    writeUint64(buffer, compileCommand);
    writeUint64(buffer, launchTime);
    writeNodeVector(buffer, headerFiles);
}

void BuildCache::Cpp::SourceFile::deserialize(const char *ptr, uint32_t &bytesRead)
{
    node = readHalfNode(ptr, bytesRead);
    compileCommand = readUint64(ptr, bytesRead);
    launchTime = readUint64(ptr, bytesRead);
    const uint32_t headerSize = readUint32(ptr, bytesRead);
    headerFiles.reserve(headerSize);
    for (uint32_t i = 0; i < headerSize; ++i)
    {
        headerFiles.emplace_back(readHalfNode(ptr, bytesRead));
    }
}

void ModuleFile::SingleDep::serialize(string &buffer) const
{
    writeNode(buffer, node);
    writeUint64(buffer, compileCommand);
    writeUint32(buffer, targetIndex);
    writeUint32(buffer, myIndex);
}

void ModuleFile::SingleDep::deserialize(const char *ptr, uint32_t &bytesRead)
{
    node = readHalfNode(ptr, bytesRead);
    compileCommand = readUint64(ptr, bytesRead);
    targetIndex = readUint32(ptr, bytesRead);
    myIndex = readUint32(ptr, bytesRead);
}

void ModuleFile::serialize(string &buffer) const
{
    srcFile.serialize(buffer);
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
    writeBool(buffer, headerStatusChanged);
}

void ModuleFile::deserialize(const char *ptr, uint32_t &bytesRead)
{
    srcFile.deserialize(ptr, bytesRead);
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
    headerStatusChanged = readBool(ptr, bytesRead);
}

void BuildCache::Cpp::serialize(string &buffer) const
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

void writeBool(string &buffer, const bool &value)
{
    buffer.push_back(value);
}

void writeUint8(string &buffer, const uint8_t &data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

void writeUint32(string &buffer, const uint32_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

void writeUint64(string &buffer, const uint64_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

void writeStringView(string &buffer, const string_view &data)
{
    writeUint32(buffer, data.size());
    buffer.append(data.begin(), data.end());
}

void writeNode(string &buffer, const Node *node)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    writeUint32(buffer, node->myId);
#else
    writeStringView(buffer, node->filePath);
#endif
}

void writeNodeVector(string &buffer, const vector<Node *> &array)
{
    writeUint32(buffer, array.size());
    for (auto &e : array)
    {
        writeNode(buffer, e);
    }
}
