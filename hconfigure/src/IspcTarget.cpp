#include "IspcTarget.hpp"

#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CppTarget.hpp"
#include "rapidhash/rapidhash.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

uint64_t IspcTarget::getCacheName(const CppTarget *cppTarget)
{
    return rapidhash_withSeed(&cppTarget->cacheName, sizeof(cppTarget->cacheName),
                              0x4953504354415247ULL); // "ISPCTARG"
}

IspcTarget::IspcTarget(CppTarget *cppTarget_)
    : ObjectFileProducer(cppTarget_->name + "/ispc", getCacheName(cppTarget_), BTargetType::ISPC_TARGET, false, false),
      cppTarget(cppTarget_), myBuildDir(cppTarget_->myBuildDir)
{
    validate();
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        create_directories(myBuildDir->filePath);
    }
    else
    {
        readConfigCacheAtBuildTime();
    }
    initializeGraph();
    if constexpr (bsMode == BSMode::BUILD)
    {
        for (Node *source : sourceNodes)
        {
            initializeSource(source);
        }
    }
}

void IspcTarget::validate() const
{
    const IspcCompilerFeatures &features = cppTarget->configuration->ispcCompilerFeatures;
    if (features.compiler == nullptr || myBuildDir == nullptr)
    {
        printErrorMessage(FORMAT("ISPC target requires a C++ target, compiler, and output directory.\nTarget: {}",
                                 cppTarget->name));
    }
    if (features.targets.empty())
    {
        printErrorMessage(
            FORMAT("ISPC target has an incomplete toolchain specification.\nTarget: {}", cppTarget->name));
    }

    flat_hash_set<string> outputNames;
    for (const string &targetName : features.targets)
    {
        if (targetName.empty())
        {
            printErrorMessage(FORMAT("ISPC target list contains an empty entry.\nTarget: {}", cppTarget->name));
        }
        const string shortName = targetName.substr(0, targetName.find('-'));
        if (!outputNames.emplace(shortName).second)
        {
            printErrorMessage(FORMAT("ISPC targets map to the same output suffix.\nTarget: {}\nSuffix: {}",
                                     cppTarget->name, shortName));
        }
    }
}

void IspcTarget::initializeGraph()
{
    cppTarget->getOrCreateBeforeTarget();

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        // Treat ISPC output as a private raw-object dependency. With no PLOAT on this DSC, deps() exports the objects
        // through useReq so a LIBRARY_OBJECT module can carry them to its eventual linker. A real static/shared PLOAT
        // absorbs that private dependency and prevents it from propagating beyond the library boundary.
        DSC dep{this, nullptr};
        DSC{cppTarget, nullptr}.deps(DepType::PRIVATE, false, true, dep);
    }
}

IspcTarget &IspcTarget::addSource(Node *source)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        printErrorMessage(FORMAT("ISPC sources must be registered while configuring.\nTarget: {}\nSource: {}",
                                 cppTarget->name,
                                 source == nullptr ? string_view{"<null>"} : string_view{source->filePath}));
    }
    if (std::ranges::find(sourceNodes, source) != sourceNodes.end())
    {
        printErrorMessage(FORMAT("ISPC source was registered more than once.\nTarget: {}\nSource: {}", cppTarget->name,
                                 source->filePath));
    }
    if (std::ranges::any_of(sourceNodes, [source](const Node *registered) {
            return registered->getFileName() == source->getFileName();
        }))
    {
        // The generated header name is part of the C++ include contract and therefore remains basename-based.
        printErrorMessage(FORMAT("ISPC sources in one target have the same filename.\nTarget: {}\nSource: {}",
                                 cppTarget->name, source->filePath));
    }

    sourceNodes.emplace_back(source);
    initializeSource(source);
    return *this;
}

void IspcTarget::initializeSource(Node *source)
{
    IspcHeader *header = new IspcHeader(this, source);
    IspcObject *object = new IspcObject(this, header, source);
    headerTargets.emplace_back(header);
    objectTargets.emplace_back(object);
    // Object actions are materialized before Builder constructs the round-one graph. Publish their availability now
    // so every eventual PLOAT can establish the ordinary round-zero producer edge from its flattened dependency cache.
    hasObjectFiles = true;

    object->realBTargets[0].addDep<BTargetType::ISPC_HEADER>(&header->realBTargets[0]);
    realBTargets[0].addDep<BTargetType::ISPC_OBJECT>(&object->realBTargets[0]);
    cppTarget->beforeTarget->realBTargets[0].addDep<BTargetType::ISPC_HEADER>(&header->realBTargets[0]);
}

void IspcTarget::getObjectFiles(std::pmr::vector<Node *> &objectNodes_, const bool includeRequiredProducers) const
{
    ObjectFileProducer::getObjectFiles(objectNodes_, includeRequiredProducers);
    for (const IspcObject *object : objectTargets)
    {
        objectNodes_.insert(objectNodes_.end(), object->objectNodes.begin(), object->objectNodes.end());
    }
}

void IspcTarget::initializeCommands()
{
    if (commandsInitialized)
    {
        return;
    }

    vector<const Define *> definitions;
    definitions.reserve(cppTarget->reqCompileDefinitions.size());
    for (const Define &definition : cppTarget->reqCompileDefinitions)
    {
        definitions.emplace_back(&definition);
    }
    std::ranges::sort(definitions, [](const Define *lhs, const Define *rhs) {
        return lhs->name < rhs->name || (lhs->name == rhs->name && lhs->value < rhs->value);
    });

    const Configuration *configuration = cppTarget->configuration;
    if (configuration->ispcCompileCommand.empty())
    {
        printErrorMessage(FORMAT("ISPC command was not initialized.\nConfiguration: {}\nTarget: {}",
                                 configuration->name, cppTarget->name));
    }

    STACK_PMR_STRING(command, 256 * 1024)
    command.append(configuration->ispcCompileCommand);
    for (const InclNode &include : cppTarget->reqIncls)
    {
        command += "-I\"";
        command += include.node->filePath;
        command += "\" ";
    }
    for (const Define *definition : definitions)
    {
        if (definition->value.contains("\\\\U") || definition->value.contains("\\\\u"))
        {
            // Matches UBT's guard against an ISPC universal-character warning for these escaped values.
            continue;
        }
        command += "-D";
        command += definition->name;
        command.push_back('=');
        command += definition->value;
        command.push_back(' ');
    }
    headerCommand.assign(command.data(), command.size());
    headerCommandHash = rapidhash(command.data(), command.size());

    command.append(configuration->ispcObjectCommandSuffix);
    objectCommand.assign(command.data(), command.size());
    objectCommandHash = rapidhash(command.data(), command.size());
    commandsInitialized = true;
}

string IspcTarget::getPrintName() const
{
    return "ISPC target " + cppTarget->name;
}

void IspcTarget::writeConfigCacheAtConfigTime(string &buffer)
{
    ObjectFileProducer::writeConfigCacheAtConfigTime(buffer);
    writeUint32(buffer, sourceNodes.size());
    for (const Node *source : sourceNodes)
    {
        writeNode(buffer, source);
    }
}

void IspcTarget::readConfigCacheAtBuildTime()
{
    const string_view configCache = bTargetCaches[cacheIndex].configCache;
    const char *ptr = configCache.data();
    uint32_t bytesRead = configCacheRead;
    const uint32_t sourceCount = readUint32(ptr, bytesRead);
    sourceNodes.reserve(sourceCount);
    for (uint32_t index = 0; index < sourceCount; ++index)
    {
        sourceNodes.emplace_back(readHalfNode(ptr, bytesRead));
    }
    if (bytesRead != configCache.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    configCacheRead = bytesRead;
}
namespace
{
uint64_t getIspcActionCacheName(const IspcTarget *target, const Node *source, const bool isIspcHeader)
{
    return static_cast<uint64_t>(source->myId) << 32 | static_cast<uint64_t>(target->cacheIndex) << 2 |
           static_cast<uint64_t>(isIspcHeader);
}

string getIspcActionName(const IspcTarget *target, const Node *source, const string_view action)
{
    return target->name + '/' + string(action) + '-' + source->getFileName() + toString(source->myId);
}

string getHeaderOutputBase(const IspcTarget *target, const Node *source)
{
    return target->myBuildDir->filePath + slashc + source->getFileName();
}

string getObjectOutputBase(const IspcTarget *target, const Node *source)
{
    return getHeaderOutputBase(target, source) + toString(source->myId);
}

string getDummyHeaderPath(const IspcTarget *target, const Node *source)
{
    return getHeaderOutputBase(target, source) + ".generated.dummy.h";
}

string getDependencyListPath(const IspcTarget *target, const Node *source)
{
    return getHeaderOutputBase(target, source) + ".txt";
}

bool filesHaveSameContents(const string &lhsPath, const string &rhsPath)
{
    constexpr size_t bufferSize = 64 * 1024;
    alignas(64) std::array<char, bufferSize> lhsBuffer;
    alignas(64) std::array<char, bufferSize> rhsBuffer;

#ifdef _WIN32
    const HANDLE lhs = CreateFileA(lhsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (lhs == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    const HANDLE rhs = CreateFileA(rhsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (rhs == INVALID_HANDLE_VALUE)
    {
        CloseHandle(lhs);
        return false;
    }

    LARGE_INTEGER lhsSize;
    LARGE_INTEGER rhsSize;
    if (!GetFileSizeEx(lhs, &lhsSize) || !GetFileSizeEx(rhs, &rhsSize) || lhsSize.QuadPart < 0 ||
        lhsSize.QuadPart != rhsSize.QuadPart)
    {
        CloseHandle(rhs);
        CloseHandle(lhs);
        return false;
    }

    const auto readFully = [](const HANDLE file, char *destination, DWORD bytes) {
        while (bytes != 0)
        {
            DWORD bytesRead;
            if (!ReadFile(file, destination, bytes, &bytesRead, nullptr) || bytesRead == 0)
            {
                return false;
            }
            destination += bytesRead;
            bytes -= bytesRead;
        }
        return true;
    };
    uint64_t remaining = lhsSize.QuadPart;
    bool same = true;
    while (remaining != 0)
    {
        const DWORD bytes = static_cast<DWORD>(remaining < bufferSize ? remaining : bufferSize);
        if (!readFully(lhs, lhsBuffer.data(), bytes) || !readFully(rhs, rhsBuffer.data(), bytes) ||
            std::memcmp(lhsBuffer.data(), rhsBuffer.data(), bytes) != 0)
        {
            same = false;
            break;
        }
        remaining -= bytes;
    }
    CloseHandle(rhs);
    CloseHandle(lhs);
    return same;
#else
    const int lhs = open(lhsPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (lhs == -1)
    {
        return false;
    }
    const int rhs = open(rhsPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (rhs == -1)
    {
        close(lhs);
        return false;
    }

    struct stat lhsStat;
    struct stat rhsStat;
    if (fstat(lhs, &lhsStat) == -1 || fstat(rhs, &rhsStat) == -1 || lhsStat.st_size < 0 ||
        lhsStat.st_size != rhsStat.st_size)
    {
        close(rhs);
        close(lhs);
        return false;
    }

    const auto readFully = [](const int file, char *destination, size_t bytes) {
        while (bytes != 0)
        {
            const ssize_t bytesRead = read(file, destination, bytes);
            if (bytesRead > 0)
            {
                destination += bytesRead;
                bytes -= bytesRead;
            }
            else if (bytesRead != -1 || errno != EINTR)
            {
                return false;
            }
        }
        return true;
    };
    uint64_t remaining = lhsStat.st_size;
    bool same = true;
    while (remaining != 0)
    {
        const size_t bytes = remaining < bufferSize ? remaining : bufferSize;
        if (!readFully(lhs, lhsBuffer.data(), bytes) || !readFully(rhs, rhsBuffer.data(), bytes) ||
            std::memcmp(lhsBuffer.data(), rhsBuffer.data(), bytes) != 0)
        {
            same = false;
            break;
        }
        remaining -= bytes;
    }
    close(rhs);
    close(lhs);
    return same;
#endif
}

void printIspcResult(const BTarget &action, const Builder &builder, const string_view actionName,
                     const Node *sourceNode, const string_view ownerName, const std::pmr::string &command)
{
    string output;
    if (isConsole)
    {
        output += getColorCode(ColorIndex::hot_pink);
    }
    if (action.run.output->empty())
    {
        output += FORMAT("[{}/{}]{} {} {}\n", builder.updatedCount, builder.readyBTargetsSizeGoal, actionName,
                         sourceNode->filePath, ownerName);
    }
    else
    {
        output.append(command.data(), command.size());
        output.push_back('\n');
    }
    if (isConsole)
    {
        output += getColorCode(ColorIndex::reset);
    }
    output += *action.run.output;
    if (!action.run.output->empty())
    {
        output.push_back('\n');
    }
    fwrite(output.data(), 1, output.size(), stdout);
}

} // namespace

IspcHeader::IspcHeader(IspcTarget *target_, Node *sourceNode_)
    : BTarget(getIspcActionName(target_, sourceNode_, "header"), getIspcActionCacheName(target_, sourceNode_, true),
              true, BTargetType::ISPC_HEADER, true, false, true, false),
      target(target_), sourceNode(sourceNode_)
{
    const string outputBase = getHeaderOutputBase(target, sourceNode);
    finalHeader = Node::getHalfNode(outputBase + ".generated.h");

    if constexpr (bsMode == BSMode::BUILD)
    {
        sourceNode->doHashFile = true;
        finalHeader->doStatFile = true;

        const string_view buildCache = bTargetCaches[cacheIndex].getBuildCache();
        uint32_t bytesRead = 0;
        const uint32_t dependencyCount = readUint32(buildCache.data(), bytesRead);
        cachedDependencies = span{reinterpret_cast<const uint32_t *>(buildCache.data() + bytesRead), dependencyCount};
        bytesRead += dependencyCount * sizeof(uint32_t);
        if (bytesRead != buildCache.size())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
        for (const uint32_t dependency : cachedDependencies)
        {
            Node::getHalfNode(dependency)->doHashFile = true;
        }
    }
}

void IspcHeader::getCommand(std::pmr::string &command) const
{
    target->initializeCommands();
    command.append(target->headerCommand.data(), target->headerCommand.size());
    command.push_back('"');
    command += sourceNode->filePath;
    command += "\" -h \"";
    command += getDummyHeaderPath(target, sourceNode);
    command += "\" -MMM \"";
    command += getDependencyListPath(target, sourceNode);
    command.push_back('"');
}

uint64_t IspcHeader::getDependencyHash(const uint64_t modifiedAfter) const
{
    const size_t dependencyCount = dependenciesRefreshed ? discoveredDependencies.size() : cachedDependencies.size();
    STACK_PMR_VECTOR(uint64_t, hashes, 256)
    hashes.reserve(dependencyCount * 2 + 1);
    hashes.emplace_back(dependencyCount);
    if (dependenciesRefreshed)
    {
        for (const Node *dependency : discoveredDependencies)
        {
            hashes.emplace_back(dependency->myId);
            hashes.emplace_back(
                modifiedAfter != 0 && dependency->lastWriteTime > modifiedAfter ? 0 : dependency->contentHash);
        }
    }
    else
    {
        for (const uint32_t dependency : cachedDependencies)
        {
            const Node *node = Node::getHalfNode(dependency);
            hashes.emplace_back(node->myId);
            hashes.emplace_back(modifiedAfter != 0 && node->lastWriteTime > modifiedAfter ? 0 : node->contentHash);
        }
    }
    return rapidhash(hashes.data(), hashes.size() * sizeof(uint64_t));
}

void IspcHeader::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
        return;
    }
    target->initializeCommands();
    const IspcCompilerFeatures &features = target->cppTarget->configuration->ispcCompilerFeatures;
    if (sourceNode->fileType == file_type::not_found || features.compiler->fileType == file_type::not_found)
    {
        printErrorMessage(FORMAT("ISPC header input does not exist.\nTarget: {}\nSource: {}\nCompiler: {}",
                                 target->cppTarget->name, sourceNode->filePath, features.compiler->filePath));
    }
    if (finalHeader->fileType == file_type::not_found ||
        !std::filesystem::is_regular_file(getDependencyListPath(target, sourceNode)))
    {
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }
    const uint64_t hashes[] = {target->headerCommandHash, sourceNode->contentHash, getDependencyHash()};
    rb.cumulativeHash = rapidhash(hashes, sizeof(hashes));
    BTarget::setUpdateStatus();
}

bool IspcHeader::isEventRegistered(Builder &builder)
{
    if (realBTargets[0].exitStatus != EXIT_SUCCESS || !selectiveBuild || !refreshUpdateStatus())
    {
        return false;
    }

    STACK_PMR_STRING(fullCommand, 256 * 1024)
    getCommand(fullCommand);
    if (dryRun)
    {
        fullCommand.push_back('\n');
        printMessage(string(fullCommand.data(), fullCommand.size()));
        return false;
    }
    commandWithResponseFile(fullCommand, getObjectOutputBase(target, sourceNode) + ".header.rsp",
                            target->cppTarget->configuration->responseFileThreshold);
    run.startAsyncProcess(fullCommand.data(), builder, this, false);
    return true;
}

void IspcHeader::parseDependencyList()
{
    const string dependencyListPath = getDependencyListPath(target, sourceNode);
    if (!std::filesystem::is_regular_file(dependencyListPath))
    {
        printErrorMessage(FORMAT("ISPC did not produce its dependency list.\nTarget: {}\nSource: {}\nFile: {}",
                                 target->cppTarget->name, sourceNode->filePath, dependencyListPath));
    }

    discoveredDependencies.clear();
    flat_hash_set<Node *> uniqueDependencies;
    const string contents = fileToString(dependencyListPath);
    for (string_view entry : split(contents, '\n'))
    {
        while (!entry.empty() && (entry.back() == '\r' || entry.back() == ' ' || entry.back() == '\t'))
        {
            entry.remove_suffix(1);
        }
        while (!entry.empty() && (entry.front() == ' ' || entry.front() == '\t'))
        {
            entry.remove_prefix(1);
        }
        if (entry.empty())
        {
            continue;
        }

        string dependency(entry);
        for (size_t escaped = dependency.find("\\\\"); escaped != string::npos;
             escaped = dependency.find("\\\\", escaped + 1))
        {
            dependency.erase(escaped, 1);
        }
        path dependencyPath(dependency);
        if (dependencyPath.is_relative())
        {
            printErrorMessage(FORMAT("ISPC emitted a relative dependency; a full path is required.\n"
                                     "Target: {}\nSource: {}\nDependency: {}",
                                     target->cppTarget->name, sourceNode->filePath, dependency));
        }
        Node *node = Node::getHalfNode(dependencyPath.string());
        if (isPathInConfigureDirectory(node->filePath))
        {
            continue;
        }
        if (uniqueDependencies.emplace(node).second)
        {
            node->doHashFile = true;
            discoveredDependencies.emplace_back(node);
        }
    }
    std::ranges::sort(discoveredDependencies, {}, [](const Node *node) { return node->filePath; });
    dependenciesRefreshed = true;
}

bool IspcHeader::isEventCompleted(Builder &builder, string_view)
{
    bool headerChanged = true;
    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        const string dummyHeaderPath = getDummyHeaderPath(target, sourceNode);
        parseDependencyList();
        headerChanged = !filesHaveSameContents(dummyHeaderPath, finalHeader->filePath);
        if (headerChanged)
        {
            std::filesystem::copy_file(dummyHeaderPath, finalHeader->filePath,
                                       std::filesystem::copy_options::overwrite_existing);
        }

        const uint64_t sourceHash =
            initiationTime != 0 && sourceNode->lastWriteTime > initiationTime ? 0 : sourceNode->contentHash;
        const uint64_t hashes[] = {target->headerCommandHash, sourceHash, getDependencyHash(initiationTime)};
        realBTargets[0].cumulativeHash = rapidhash(hashes, sizeof(hashes));
        buildCacheUpdated = true;
        buildFooterUpdated = true;
        if (!headerChanged)
        {
            // The ISPC implementation may have changed while its C++ interface remained byte-for-byte identical.
            // Preserve the previous completion time so ordinary C++ consumers are cut off. IspcObject observes that
            // this action refreshed its dependencies and still rebuilds through its direct graph dependency.
            realBTargets[0].updateStatus = UpdateStatus::UPDATE_NOT_NEEDED;
        }
    }

    STACK_PMR_STRING(fullCommand, 256 * 1024)
    getCommand(fullCommand);
    printIspcResult(*this, builder, "ISPC Header", sourceNode, target->cppTarget->name, fullCommand);
    return false;
}

string IspcHeader::getPrintName() const
{
    return "ISPC header " + sourceNode->filePath;
}

void IspcHeader::writeBuildCacheAtConfigTime(string &buffer)
{
    writeUint32(buffer, 0);
}

void IspcHeader::writeBuildCacheAtBuildTime(string &buffer)
{
    const uint64_t sourceHash =
        initiationTime != 0 && sourceNode->lastWriteTime > initiationTime ? 0 : sourceNode->contentHash;
    const uint64_t hashes[] = {target->headerCommandHash, sourceHash, getDependencyHash(initiationTime)};
    realBTargets[0].cumulativeHash = rapidhash(hashes, sizeof(hashes));
    writeUint32(buffer, discoveredDependencies.size());
    for (const Node *dependency : discoveredDependencies)
    {
        writeNode(buffer, dependency);
    }
}

void IspcHeader::verifyBuildCache(const string_view buildCache) const
{
    uint32_t bytesRead = 0;
    const uint32_t dependencyCount = readUint32(buildCache.data(), bytesRead);
    for (uint32_t index = 0; index < dependencyCount; ++index)
    {
        readHalfNode(buildCache.data(), bytesRead);
    }
    verifyBTargetHeader(buildCache, bytesRead);
    if (bytesRead != buildCache.size())
    {
        printErrorMessage(FORMAT("Build cache verification failed: ISPC-header entry size mismatch.\n"
                                 "Target: {}\nEntry size: {} bytes\nBytes consumed: {}",
                                 getPrintName(), buildCache.size(), bytesRead));
    }
}

IspcObject::IspcObject(IspcTarget *target_, IspcHeader *headerTarget_, Node *sourceNode_)
    : ObjectFile(getIspcActionCacheName(target_, sourceNode_, false), BTargetType::ISPC_OBJECT, true, false),
      target(target_), headerTarget(headerTarget_), sourceNode(sourceNode_)
{
    const string outputBase = getObjectOutputBase(target, sourceNode);
    const IspcCompilerFeatures &features = target->cppTarget->configuration->ispcCompilerFeatures;
    if (features.targets.size() > 1)
    {
        objectNodes.reserve(features.targets.size() + 1);
        for (const string &targetName : features.targets)
        {
            objectNodes.emplace_back(Node::getNode(outputBase + '_' + targetName.substr(0, targetName.find('-')) +
                                                       string(features.getObjectSuffix()),
                                                   true, true));
        }
    }
    objectNodes.emplace_back(Node::getNode(outputBase + string(features.getObjectSuffix()), true, true));

    if constexpr (bsMode == BSMode::BUILD)
    {
        for (Node *output : objectNodes)
        {
            output->doStatFile = true;
        }
    }
}

void IspcObject::getCommand(std::pmr::string &command) const
{
    target->initializeCommands();
    command.append(target->objectCommand.data(), target->objectCommand.size());
    command.push_back('"');
    command += sourceNode->filePath;
    command += "\" -o \"";
    command += objectNodes.back()->filePath;
    command.push_back('"');
}

void IspcObject::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
        return;
    }
    target->initializeCommands();
    if (std::ranges::any_of(objectNodes, [](const Node *output) { return output->fileType == file_type::not_found; }))
    {
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }
    if (headerTarget->dependenciesRefreshed)
    {
        // IspcHeader can prove its generated C++ interface unchanged and cut off ordinary C++ consumers. Its ISPC
        // object must still be rebuilt when that header action ran because an included implementation may have changed.
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }
    const uint64_t hashes[] = {target->objectCommandHash, sourceNode->contentHash};
    rb.cumulativeHash = rapidhash(hashes, sizeof(hashes));
    ObjectFile::setUpdateStatus();
}

bool IspcObject::isEventRegistered(Builder &builder)
{
    if (realBTargets[0].exitStatus != EXIT_SUCCESS || !selectiveBuild || !refreshUpdateStatus())
    {
        return false;
    }

    STACK_PMR_STRING(fullCommand, 256 * 1024)
    getCommand(fullCommand);
    if (dryRun)
    {
        fullCommand.push_back('\n');
        printMessage(string(fullCommand.data(), fullCommand.size()));
        return false;
    }
    commandWithResponseFile(fullCommand, getObjectOutputBase(target, sourceNode) + ".object.rsp",
                            target->cppTarget->configuration->responseFileThreshold);
    run.startAsyncProcess(fullCommand.data(), builder, this, false);
    return true;
}

bool IspcObject::isEventCompleted(Builder &builder, string_view)
{
    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        const uint64_t sourceHash =
            initiationTime != 0 && sourceNode->lastWriteTime > initiationTime ? 0 : sourceNode->contentHash;
        const uint64_t hashes[] = {target->objectCommandHash, sourceHash};
        realBTargets[0].cumulativeHash = rapidhash(hashes, sizeof(hashes));
        buildCacheUpdated = true;
        buildFooterUpdated = true;
    }

    STACK_PMR_STRING(fullCommand, 256 * 1024)
    getCommand(fullCommand);
    printIspcResult(*this, builder, "ISPC Object", sourceNode, target->cppTarget->name, fullCommand);
    return false;
}

string IspcObject::getPrintName() const
{
    return "ISPC object " + sourceNode->filePath;
}

void IspcObject::writeBuildCacheAtBuildTime(string &)
{
    const uint64_t sourceHash =
        initiationTime != 0 && sourceNode->lastWriteTime > initiationTime ? 0 : sourceNode->contentHash;
    const uint64_t hashes[] = {target->objectCommandHash, sourceHash};
    realBTargets[0].cumulativeHash = rapidhash(hashes, sizeof(hashes));
}
