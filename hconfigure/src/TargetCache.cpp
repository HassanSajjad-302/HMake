
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

Node *readNode(const char *ptr, uint32_t &bytesRead)
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
    cfg.buildCacheFilesDirPath = readNode(ptr, bytesRead);
    return cfg;
}

ConfigCache::Link readLinkConfigCache(const char *ptr, uint32_t &bytesRead)
{
    ConfigCache::Link ln;
    ln.reqLibraryDirsArray = readNodeSpan(ptr, bytesRead);
    ln.useReqLibraryDirsArray = readNodeSpan(ptr, bytesRead);
    ln.outputFileNode = readNode(ptr, bytesRead);
    ln.buildCacheFilesDirPath = readNode(ptr, bytesRead);
    return ln;
}

BuildCache::Cpp::SourceFile readSourceFileBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::SourceFile sf;
    sf.fullPath = readNode(ptr, bytesRead);
    sf.compileCommandWithTool = readCCOrHash(ptr, bytesRead);
    sf.headerFiles = readNodeSpan(ptr, bytesRead);
    return sf;
}

void BuildCache::Cpp::SourceFile::initialize(const char *ptr, uint32_t &bytesRead)
{
    *this = readSourceFileBuildCache(ptr, bytesRead);
}

void ModuleFileCache::SmRules::SingleHeaderUnitDep::initialize(const char *ptr, uint32_t &bytesRead)
{
    *this = readSingleHeaderUnitDep(ptr, bytesRead);
}
void ModuleFileCache::SmRules::SingleModuleDep::initialize(const char *ptr, uint32_t &bytesRead)
{
    *this = readSingleModuleDep(ptr, bytesRead);
}
void ModuleFileCache::SmRules::initialize(const char *ptr, uint32_t &bytesRead)
{
    *this = readSMRules(ptr, bytesRead);
}
void BuildCache::Cpp::initialize(uint32_t targetCacheIndex)
{

    // TODO
    // just receive targetCacheIndex
    // also check if the bytesRead size == cppConfigTargets[targetCacheIndex].buildCache.size().
    // also assign BuildCache::Cpp::SourceFile ptr and size.
}

void ModuleFileCache::initialize(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    vector<BuildCache::Cpp::SourceFile> for (uint32_t i = 0; i < count; ++i)
    {
        BuildCache::Cpp::SourceFile sf;
        sf.fullPath = readNode(ptr, bytesRead);
        sf.compileCommandWithTool = readCCOrHash(ptr, bytesRead);
        sf.headerFiles = readNodeSpan(ptr, bytesRead);
    }
    bytesRead += count * sizeof(BuildCache::Cpp::SourceFile);
}

BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep readSingleHeaderUnitDep(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep huDep;
    huDep.fullPath = readNode(ptr, bytesRead);
    huDep.angle = readBool(ptr, bytesRead);
    huDep.targetIndex = readUint32(ptr, bytesRead);
    huDep.myIndex = readUint32(ptr, bytesRead);
    return huDep;
}

span<ModuleFileCache::SmRules::SingleHeaderUnitDep> readSingleHeaderUnitDepSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(ModuleFileCache::SmRules::SingleHeaderUnitDep);
    return {(ModuleFileCache::SmRules::SingleHeaderUnitDep *)(ptr + offset), count};
}

BuildCache::Cpp::ModuleFile::SmRules::SingleModuleDep readSingleModuleDep(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFile::SmRules::SingleModuleDep d;
    d.fullPath = readNode(ptr, bytesRead);
    d.logicalName = string(readStringView(ptr, bytesRead));
    return d;
}

span<ModuleFileCache::SmRules::SingleModuleDep> readSingleModuleDepSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(ModuleFileCache::SmRules::SingleModuleDep);
    return {(ModuleFileCache::SmRules::SingleModuleDep *)(ptr + offset), count};
}

BuildCache::Cpp::ModuleFile::SmRules readSMRules(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFile::SmRules rules;
    rules.exportName = string(readStringView(ptr, bytesRead));
    rules.isInterface = readBool(ptr, bytesRead);
    rules.headerUnitArray = readSingleHeaderUnitDepSpan(ptr, bytesRead);
    rules.moduleArray = readSingleModuleDepSpan(ptr, bytesRead);
    return rules;
}

BuildCache::Cpp::ModuleFile readModuleFileBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp::ModuleFile mf;
    mf.srcFile = readSourceFileBuildCache(ptr, bytesRead);
    mf.headerFiles = readNodeSpan(ptr, bytesRead);
    mf.smRules = readSMRules(ptr, bytesRead);
    mf.compileCommandWithTool = readCCOrHash(ptr, bytesRead);
    return mf;
}

span<ModuleFileCache> readModuleFilesBuildCacheSpan(const char *ptr, uint32_t &bytesRead)
{
    uint32_t count = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += count * sizeof(ModuleFileCache);
    return {(ModuleFileCache *)(ptr + offset), count};
}

BuildCache::Cpp readCppBuildCache(const char *ptr, uint32_t &bytesRead)
{
    BuildCache::Cpp bc;
    bc.srcFiles = readSourceFilesBuildCacheSpan(ptr, bytesRead);
    bc.modFiles = readModuleFilesBuildCacheSpan(ptr, bytesRead);
    bc.headerUnits = readModuleFilesBuildCacheSpan(ptr, bytesRead);
    return bc;
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

void writeUint32(vector<char> &buffer, const uint32_t &data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(data));
}

void writeStringView(vector<char> &buffer, const string_view &data)
{
    writeUint32(buffer, static_cast<uint32_t>(data.size()));
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

void writeNodeSpan(vector<char> &buffer, span<Node *> array)
{
    writeUint32(buffer, static_cast<uint32_t>(array.size()));
    for (auto &e : array)
    {
        writeNode *(buffer, e);
    }
}

void writeCppConfigCache(vector<char> &buffer, const ConfigCache::Cpp &data)
{
    writeNodeSpan(buffer, data.reqInclsArray);
    writeNodeSpan(buffer, data.useReqInclsArray);
    writeNodeSpan(buffer, data.reqHUDirsArray);
    writeNodeSpan(buffer, data.useReqHUDirsArray);
    writeNodeSpan(buffer, data.sourceFiles);
    writeNodeSpan(buffer, data.moduleFiles);
    writeNodeSpan(buffer, data.headerUnits);
    writeNode *(buffer, data.buildCacheFilesDirPath);
}

void writeLinkConfigCache(vector<char> &buffer, const ConfigCache::Link &data)
{
    writeNodeSpan(buffer, data.reqLibraryDirsArray);
    writeNodeSpan(buffer, data.useReqLibraryDirsArray);
    writeNode(buffer, data.outputFileNode);
    writeNode(buffer, data.buildCacheFilesDirPath);
}

void writeSourceFileBuildCache(vector<char> &buffer, const BuildCache::Cpp::SourceFile &data)
{
    writeNode(buffer, data.fullPath);
    writeCCOrHash(buffer, data.compileCommandWithTool);
    writeNodeSpan(buffer, data.headerFiles);
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
    writeNode *(buffer, data.fullPath);
    writeBool(buffer, data.angle);
    writeUint32(buffer, data.targetIndex);
    writeUint32(buffer, data.myIndex);
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
    writeNodeSpan(buffer, data.headerFiles);
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
    writeNodeSpan(buffer, data.objectFiles);
}
