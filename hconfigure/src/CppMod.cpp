
#include "CppMod.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "CppTarget.hpp"
#include "IPCManagerCompiler.hpp"
#include "JConsts.hpp"
#include "PointerPacking.h"
#include "TargetCache.hpp"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory_resource>
#include <utility>

using std::tie, std::ifstream, std::exception, std::lock_guard, P2978::IPCManagerBS;

bool CompareCppSrc::operator()(const CppSrc &lhs, const CppSrc &rhs) const
{
    return lhs.node < rhs.node;
}

bool CompareCppSrc::operator()(const Node *lhs, const CppSrc &rhs) const
{
    return lhs < rhs.node;
}

bool CompareCppSrc::operator()(const CppSrc &lhs, const Node *rhs) const
{
    return lhs.node < rhs;
}

#ifdef BUILD_MODE
CppSrc::CppSrc(CppTarget *target_, const Node *node_) : ObjectFile(true, false), target(target_), node{node_}
{
}
#else
CppSrc::CppSrc(CppTarget *target_, const Node *node_) : ObjectFile(false, false), target(target_), node{node_}
{
}
#endif

string CppSrc::getPrintName() const
{
    return node->filePath;
}

void CppSrc::initializeBuildCache(const uint32_t index, const uint64_t commandHash_)
{
    myBuildCacheIndex = index;
    commandHash = commandHash_;

    const BuildCache::Cpp::SourceFile &buildCache = target->cppBuildCache.srcFiles[index];
    if (buildCache.compileCommand != commandHash)
    {
        realBTargets[0].updateStatus = UpdateStatus::NEEDS_UPDATE;
        return;
    }

    objectNode->toBeChecked = true;

    if (target->ignoreHeaderDeps)
    {
        return;
    }

    const_cast<bool &>(node->toBeChecked) = true;
    for (Node *headerFile : buildCache.headerFiles)
    {
        headerFile->toBeChecked = true;
    }
}

void CppSrc::getCompileCommand(std::pmr::string &compileCommand) const
{
    const Compiler &compiler = target->configuration->compilerFeatures.compiler;
    if (sourceType == SourceType::CPP)
    {
        compileCommand = target->configuration->cppCompileCommand;
    }
    else if (sourceType == SourceType::C)
    {
        compileCommand = target->configuration->cCompileCommand;
    }
    else if (sourceType == SourceType::ASSEMBLY)
    {
        compileCommand = target->configuration->assemblyCompileCommand;
    }
    target->setCompileCommand(compileCommand);

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        compileCommand += "-c /nologo /showIncludes /TP \"" + node->filePath + "\" /Fo\"" + objectNode->filePath + "\"";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        compileCommand += "-c -MMD \"" + node->filePath + "\" -o \"" + objectNode->filePath + "\"";
    }
}

bool CppSrc::ignoreHeaderFile(const string_view child) const
{
    // It is assumed that both paths are normalized strings
    for (const InclNode &inclNode : target->reqIncls)
    {
        if (inclNode.ignoreHeaderDeps)
        {
            if (childInParentPathNormalized(inclNode.node->filePath, child))
            {
                return true;
            }
        }
    }
    return false;
}

void CppSrc::parseDepsFromMSVCTextOutput(string &output, const bool isClang)
{
    const string includeFileNote = "Note: including file:";

    if (target->ignoreHeaderDeps || realBTargets[0].exitStatus != EXIT_SUCCESS)
    {
        string str = output;
        output.clear();
        uint32_t start = 0;
        for (uint64_t i = str.find('\n', start); i != string::npos; start = i + 1, i = str.find('\n', start))
        {
            if (string_view line = str.substr(start, i - start + 1); !line.contains(includeFileNote))
            {
                output += string(line);
            }
        }
        return;
    }

    uint64_t startPos = 0;
    uint64_t lineEnd;
    string_view line;

    constexpr uint32_t stackSize = 128 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string treatedOutput(&alloc);
    treatedOutput.reserve(stackSize);

    if (!isClang)
    {
        // MSVC also prints the name of the file which is being skipped.

        lineEnd = output.find('\n');

        if (lineEnd == string::npos)
        {
            return;
        }

        line = {output.begin() + startPos, output.begin() + lineEnd + 1};

        startPos = lineEnd + 1;
    }

    if (output.size() == startPos)
    {
        output = std::move(treatedOutput);
        return;
    }

    lineEnd = output.find('\n', startPos);
    while (true)
    {

        line = string_view(output.begin() + startPos, output.begin() + lineEnd + 1);
        if (size_t pos = line.find(includeFileNote); pos != string::npos)
        {
            pos = line.find_first_not_of(' ', includeFileNote.size());

            if (line.size() >= pos + 1)
            {
                // Last character is \r for some reason with MSVC.
                const uint8_t sub = isClang ? 1 : 2;
                // MSVC compiler can output header-includes with / as path separator

                for (auto it = line.begin() + pos; it != line.end() - sub; ++it)
                {
                    if (*it == '/')
                    {
                        const_cast<char &>(*it) = '\\';
                    }
                }

                string_view headerView{line.begin() + pos, line.end() - sub};

                // TODO
                // If compile-command is all lower-cased, then this might not be needed
                // Some compilers can input same header-file twice, if that is the case, then we should first make
                // the array unique.
                lowerCaseOnWindows(const_cast<char *>(headerView.data()), headerView.size());
                if (!ignoreHeaderFile(headerView))
                {
                    if (Node *headerNode = Node::getHalfNode(headerView); !headerFiles.contains(headerNode))
                    {
                        headerFiles.emplace(headerNode);
                    }
                }
            }
            else
            {
                printErrorMessage(FORMAT("Empty Header Include {}\n", std::string(line)));
            }
        }
        else
        {
            treatedOutput.append(line);
        }

        startPos = lineEnd + 1;
        if (output.size() == startPos)
        {
            output = treatedOutput;
            break;
        }
        /*if (lineEnd > output.size() - 5)
        {
            bool breakpoint = true;
        }*/
        lineEnd = output.find('\n', startPos);
        if (lineEnd == -1)
        {
            bool breakpoint = true;
        }
    }
}

void CppSrc::parseDepsFromGCCDepsOutput(Builder &builder)
{
    if (target->ignoreHeaderDeps)
    {
        return;
    }
    string headerDepsFile = objectNode->filePath;
    // replacing .o ext with .d
    headerDepsFile[headerDepsFile.size() - 1] = 'd';

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string headerFileDeps(&alloc);
    headerFileDeps.reserve(stackSize);
    fileToString(headerDepsFile, headerFileDeps);

    const vector<string_view> headerDeps = split(headerFileDeps, '\n');

    // The First 2 lines are skipped as these are .o and .cpp file.
    // If the file is preprocessed, it does not generate the extra line
    const auto endIt = headerDeps.end() - 1;

    if (headerDeps.size() > 2)
    {
        for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
        {
            const size_t pos = iter->find_first_not_of(" ");
            const auto it = iter->begin() + pos;
            if (const string_view headerView{&*it, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos};
                !ignoreHeaderFile(headerView))
            {
                headerFiles.emplace(Node::getHalfNode(headerView));
            }
        }
    }
}

void CppSrc::parseHeaderDeps(string &output, Builder &builder)
{
    if (target->configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(output,
                                    target->configuration->compilerFeatures.compiler.btSubFamily == BTSubFamily::CLANG);
    }
    else
    {
        // in-case of MSVC header-deps are parsed even in case of compilation failure to clean the std output.
        if (realBTargets[0].exitStatus == EXIT_SUCCESS)
        {
            parseDepsFromGCCDepsOutput(builder);
        }
    }
}

// An invariant is that paths are lexically normalized.
bool pathContainsFile(string_view dir, const string_view file)
{
    string_view withoutFileName(file.data(), file.find_last_of(slashc));

    if (dir.size() > withoutFileName.size())
    {
        return false;
    }

    // This stops checking when it reaches dir.end(), so it's OK if file
    // has more dir components afterward. They won't be checked.
    return std::equal(dir.begin(), dir.end(), withoutFileName.begin());
}

void CppSrc::setCppSrcFileStatus()
{
    if (node->fileType == file_type::not_found)
    {
        printErrorMessage(FORMAT("Source-File {}\n of target {}\n not found.\n", node->filePath, target->name));
    }

    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
    {
        return;
    }

    rb.updateStatus = UpdateStatus::NEEDS_UPDATE;

    if (target->ignoreHeaderDeps)
    {
        if (objectNode->fileType == file_type::not_found)
        {
            return;
        }
        rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
        return;
    }

    const BuildCache::Cpp::SourceFile &myBuildCache = target->cppBuildCache.srcFiles[myBuildCacheIndex];
    if (objectNode->fileType == file_type::not_found || node->lastWriteTime > myBuildCache.launchTime)
    {
        return;
    }

    for (const vector<Node *> &headers = myBuildCache.headerFiles; const Node *headerNode : headers)
    {
        if (headerNode->fileType == file_type::not_found || headerNode->lastWriteTime > myBuildCache.launchTime)
        {
            return;
        }
    }

    rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
}

void CppSrc::updateBuildCache()
{
    BuildCache::Cpp::SourceFile &buildCache = target->cppBuildCache.srcFiles[myBuildCacheIndex];

    if (realBTargets[0].updateStatus != UpdateStatus::UPDATED)
    {
        return;
    }

    buildCache.compileCommand = commandHash;
    buildCache.launchTime = globalLaunchTime;
    buildCache.headerFiles.clear();
    for (Node *header : headerFiles)
    {
        buildCache.headerFiles.emplace_back(header);
    }
}

SourceType CppSrc::setSourceType()
{
    if (node->filePath.ends_with(".c"))
    {
        sourceType = SourceType::C;
    }
    else if (node->filePath.ends_with(".S") || node->filePath.ends_with(".s"))
    {
        sourceType = SourceType::ASSEMBLY;
    }
    return sourceType;
}

bool CppSrc::isEventRegistered(Builder &builder)
{
    if (!selectiveBuild)
    {
        return false;
    }

    setCppSrcFileStatus();
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::NEEDS_UPDATE)
    {
        return false;
    }

    rb.assignNeedsUpdateToDependents();

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);
    cppFullCompileCommand.reserve(stackSize);
    getCompileCommand(cppFullCompileCommand);
    if (dryRun)
    {
        cppFullCompileCommand += '\n';
        printMessage(cppFullCompileCommand);
        return false;
    }

    run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, false);
    return true;
}

bool CppSrc::isEventCompleted(Builder &builder, string_view)
{
    parseHeaderDeps(*run.output, builder);

    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        realBTargets[0].updateStatus = UpdateStatus::UPDATED;
        // This must be just before printing as this is asynchronousl accessed in savBuildCache
        target->buildCacheUpdated = true;
    }
    else
    {
        realBTargets[0].updateStatus = UpdateStatus::UPDATED_WITHOUT_BUILDING;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);
    std::pmr::string &outputStr = cppFullCompileCommand;
    alloc.release();
    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::cyan);
    }

    if (run.output->empty())
    {
        outputStr += FORMAT("[{}/{}]C++Source {} {}\n", builder.updatedCount, builder.updateBTargetsSizeGoal,
                            node->filePath, target->name);
    }
    else
    {
        getCompileCommand(outputStr);
        outputStr.push_back('\n');
    }

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    if (!run.output->empty())
    {
        outputStr += *run.output;
        outputStr.push_back('\n');
    }
    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
    return false;
}

bool operator<(const CppSrc &lhs, const CppSrc &rhs)
{
    return lhs.node < rhs.node;
}

CppMod::CppMod(CppTarget *target_, const Node *node_) : CppSrc(target_, node_)
{
}

void CppMod::initializeBuildCache(BuildCache::Cpp::ModuleFile &modCache, const uint32_t index,
                                  const uint64_t commandHash_)
{
    if (node->filePath.contains("public-lib3.hpp"))
    {
        bool breakpooint = true;
    }

    myBuildCacheIndex = index;
    myBuildCache = &modCache;
    commandHash = commandHash_;

    if (modCache.srcFile.compileCommand != commandHash)
    {
        realBTargets[0].updateStatus = UpdateStatus::NEEDS_UPDATE;
        return;
    }

    if (modCache.headerStatusChanged)
    {
        realBTargets[0].updateStatus = UpdateStatus::NEEDS_UPDATE;
        return;
    }

    if (type == CppModType::HEADER_UNIT)
    {
        interfaceNode->toBeChecked = true;
    }
    else
    {
        objectNode->toBeChecked = true;
        if (type == CppModType::PARTITION_EXPORT || type == CppModType::PRIMARY_EXPORT)
        {
            interfaceNode->toBeChecked = true;
        }
    }

    // if ignoreHeaderDeps is true, it is assumed that user won't delete the source-file, however, object-file, pcm can
    // be deleted.
    if (target->ignoreHeaderDeps)
    {
        return;
    }

    const_cast<bool &>(node->toBeChecked) = true;
    for (const vector<Node *> headers = modCache.srcFile.headerFiles; Node *headerFile : headers)
    {
        headerFile->toBeChecked = true;
    }

    for (const ModuleFile::SingleHeaderUnitDep &h : myBuildCache->headerUnitArray)
    {
        h.node->toBeChecked = true;
    }

    for (const ModuleFile::SingleModuleDep &m : myBuildCache->moduleArray)
    {
        m.node->toBeChecked = true;
    }
}

void CppMod::makeMemoryFileMapping()
{
    if (type == CppModType::PRIMARY_IMPLEMENTATION)
    {
        return;
    }

    if (memoryMappingCompleted)
    {
        return;
    }

    P2978::BMIFile file;
    file.filePath = interfaceNode->filePath;
    if (const auto &r = IPCManagerBS::createSharedMemoryBMIFile(file); !r)
    {
        printErrorMessage(FORMAT("Could not make mapping for the file {}\n of target {}\n. Error {}\n", node->filePath,
                                 target->name, r.error()));
    }
    interfaceFileSize = file.fileSize;
    memoryMappingCompleted = true;
}

void CppMod::makeAndSendBTCModule(CppMod &mod)
{
    // todo
    // write this buffer directly.

    mod.makeMemoryFileMapping();
    P2978::BTCModule btcModule;
    btcModule.requested.filePath = mod.interfaceNode->filePath;
    btcModule.requested.fileSize = mod.interfaceFileSize;
    btcModule.isSystem = mod.target->isSystem;

    for (CppMod *modDep : mod.allCppModDependencies)
    {
        if (allCppModDependencies.emplace(modDep).second)
        {
            modDep->makeMemoryFileMapping();
            P2978::ModuleDep dep;
            dep.isHeaderUnit = modDep->type == CppModType::HEADER_UNIT;
            dep.file.filePath = modDep->interfaceNode->filePath;
            dep.file.fileSize = modDep->interfaceFileSize;
            dep.isSystem = modDep->target->isSystem;
            for (const string &l : modDep->logicalNames)
            {
                dep.logicalNames.emplace_back(l);
            }
            btcModule.modDeps.emplace_back(std::move(dep));
        }
    }

    if (const auto &r2 = ipcManager->sendMessage(btcModule); !r2)
    {
        printErrorMessage(FORMAT("send-message fail of module-file {}\n for module-file {}\n of target {}\n.",
                                 mod.node->filePath, node->filePath, target->name));
    }
}

// For debugging purposes
P2978::BTCNonModule deserializeBTCNonModule(std::string_view buffer)
{
    P2978::BTCNonModule result;
    const char *ptr = buffer.data();
    uint32_t bytesRead = 0;

    // BTCNonModule::isHeaderUnit
    result.isHeaderUnit = readBool(ptr, bytesRead);

    // BTCNonModule::isSystem
    result.isSystem = readBool(ptr, bytesRead);

    // BTCNonModule::headerFiles
    uint32_t headerFilesCount = readUint32(ptr, bytesRead);
    result.headerFiles.reserve(headerFilesCount);
    for (uint32_t i = 0; i < headerFilesCount; ++i)
    {
        P2978::HeaderFile hf;

        // HeaderFile::logicalName
        std::string_view logicalNameView = readStringView(ptr, bytesRead);
        hf.logicalName = logicalNameView;

        // HeaderFile::filePath
        std::string_view filePathView = readStringView(ptr, bytesRead);
        hf.filePath = filePathView;

        // HeaderFile::isSystem
        hf.isSystem = readBool(ptr, bytesRead);

        result.headerFiles.emplace_back(hf);
    }

    // BTCNonModule::filePath
    std::string_view filePathView = readStringView(ptr, bytesRead);
    result.filePath = filePathView;

    if (!result.isHeaderUnit)
    {
        return result;
    }

    // BTCNonModule::fileSize
    result.fileSize = readUint32(ptr, bytesRead);

    // BTCNonModule::logicalNames
    uint32_t logicalNamesCount = readUint32(ptr, bytesRead);
    result.logicalNames.reserve(logicalNamesCount);
    for (uint32_t i = 0; i < logicalNamesCount; ++i)
    {
        std::string_view sv = readStringView(ptr, bytesRead);
        result.logicalNames.emplace_back(sv);
    }

    // BTCNonModule::huDeps
    uint32_t huDepsCount = readUint32(ptr, bytesRead);
    result.huDeps.reserve(huDepsCount);
    for (uint32_t i = 0; i < huDepsCount; ++i)
    {
        P2978::HuDep dep;

        // HuDep::file::filePath
        dep.file.filePath = readStringView(ptr, bytesRead);

        // HuDep::file::fileSize
        dep.file.fileSize = readUint32(ptr, bytesRead);

        // HuDep::logicalNames
        uint32_t depLogicalNamesCount = readUint32(ptr, bytesRead);
        dep.logicalNames.reserve(depLogicalNamesCount);
        for (uint32_t j = 0; j < depLogicalNamesCount; ++j)
        {
            std::string_view sv = readStringView(ptr, bytesRead);
            dep.logicalNames.emplace_back(sv);
        }

        // HuDep::isSystem
        dep.isSystem = readBool(ptr, bytesRead);

        result.huDeps.emplace_back(std::move(dep));
    }

    // Sanity check
    if (bytesRead + strlen(P2978::delimiter) != buffer.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        /*std::cerr << "WARNING: Deserialized " << bytesRead << " bytes but buffer size is " << buffer.size()
                  << " (difference: " << (int)buffer.size() - (int)bytesRead << ")\n";*/
    }

    return result;
}

void CppMod::makeAndSendBTCNonModule(CppMod &hu)
{
    hu.makeMemoryFileMapping();

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string toBeSend(&alloc);

    // BTCNonModule::isHeaderUnit
    writeBool(toBeSend, true);
    // BTCNonModule::isSystem
    writeBool(toBeSend, hu.target->isSystem);

    if (!firstMessageSent)
    {
        firstMessageSent = true;
        // BTCNonModule::headerFiles
        writeUint32(toBeSend, composingHeaders.size());
        for (auto &[str, composingNode] : composingHeaders)
        {
            // emplace in header-files to send
            // HeaderFile::logicalName
            writeStringView(toBeSend, str);
            // HeaderFile::filePath
            writeStringView(toBeSend, composingNode->filePath);
            toBeSend.push_back('\0');
            // HeaderFile::isSystem
            writeBool(toBeSend, target->isSystem);
        }
    }
    else
    {
        // BTCNonModule::headerFiles
        writeUint32(toBeSend, 0);
    }

    // BTCNonModule::filePath
    writeStringView(toBeSend, hu.interfaceNode->filePath);
    toBeSend.push_back('\0');
    // BTCNonModule::fileSize
    writeUint32(toBeSend, hu.interfaceFileSize);
    // BTCNonModule::logicalNames
    writeUint32(toBeSend, hu.logicalNames.size());
    for (const string &str : hu.logicalNames)
    {
        writeStringView(toBeSend, str);
    }

    // index of the place-holder size of huDeps
    uint32_t placeHolderIndex = toBeSend.size();

    // BTCNonModule::huDeps
    writeUint32(toBeSend, 0);

    uint32_t count = 0;
    for (CppMod *huDep : hu.allCppModDependencies)
    {
        if (allCppModDependencies.emplace(huDep).second)
        {
            ++count;
            huDep->makeMemoryFileMapping();

            // HuDep::file::filePath
            writeStringView(toBeSend, huDep->interfaceNode->filePath);
            toBeSend.push_back('\0');
            // HuDep::file::fileSize
            writeUint32(toBeSend, huDep->interfaceFileSize);
            // BTCNonModule::isSystem
            writeBool(toBeSend, huDep->target->isSystem);

            // HuDep::logicalNames
            writeUint32(toBeSend, huDep->logicalNames.size());
            for (const string &str : huDep->logicalNames)
            {
                writeStringView(toBeSend, str);
            }
        }
    }
    *reinterpret_cast<uint32_t *>(&toBeSend[placeHolderIndex]) = count;
    toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

    if (const auto &r2 = ipcManager->writeInternal(toBeSend); !r2)
    {
        printErrorMessage(FORMAT("send-message fail of header-unit {}\n for {} {}\n of target {}\n.", hu.node->filePath,
                                 type == CppModType::HEADER_UNIT ? "header-unit" : "module-filee", node->filePath,
                                 target->name));
    }
}

CppMod *CppMod::findModule(const string_view moduleName) const
{
    if (const auto it = target->imodNames.find(moduleName); it != target->imodNames.end())
    {
        return it->second;
    }

    if (!moduleName.contains(':'))
    {
        for (CppTarget *req : target->reqDeps)
        {
            if (auto it2 = req->imodNames.find(moduleName); it2 != req->imodNames.end())
            {
                return it2->second;
            }
        }
    }

    return nullptr;
}

HeaderFileOrUnit CppMod::findHeaderFileOrUnit(const string_view headerName) const
{
    if (const auto &it = target->reqHeaderNameMapping.find(headerName); it != target->reqHeaderNameMapping.end())
    {
        return it->second;
    }

    if (const auto &it = target->configuration->headerNameMapping.find(headerName);
        it != target->configuration->headerNameMapping.end())
    {
        if (target->configuration->evaluate(UseConfigurationScope::YES))
        {
            // If configuration scope is set, then there can't be more than one entry in
            // Configuration::headerNameMapping. This is ensured at config-time.
            return it->second[0];
        }

        for (const vector<HeaderFileOrUnit> &configHeaderFilesOrUnits = it->second;
             const HeaderFileOrUnit &headerFileOrUnit : configHeaderFilesOrUnits)
        {
            CppTarget *req = static_cast<CppTarget *>(fileTargetCaches[headerFileOrUnit.targetIndex].targetCache);
            if (auto it2 = target->reqDeps.find(req); it2 != target->reqDeps.end())
            {
                // this headerFileOrUnit is provided by one of our dependency cpp-target.
                return headerFileOrUnit;
            }
        }
    }

    printMessage(FORMAT("{}\n", target->configuration->headerNameMapping.size()));
    printMessage(FORMAT("{}\n", target->reqHeaderNameMapping.size()));
    fflush(stdout);
    return {static_cast<Node *>(nullptr), false};
}

bool CppMod::isEventRegistered(Builder &builder)
{
    // an optimization is to increase/decrease the activeEventCount for less stack.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        assert("CppMod::updateBTarget cannot be called in Configure Mode\n");
    }

    if (!selectiveBuild)
    {
        return false;
    }

    RealBTarget &rb = realBTargets[0];
    if (waitingFor)
    {
        return isEventCompleted(builder, string_view{});
    }

    isScheduled = true;

    if (rb.exitStatus != EXIT_SUCCESS)
    {
        return false;
    }

    if (huOnly && type != CppModType::HEADER_UNIT)
    {
        rb.exitStatus = EXIT_FAILURE;
        return false;
    }

    setFileStatusAndPopulateAllDependencies();

    if (rb.updateStatus == UpdateStatus::ALREADY_UPDATED)
    {
        rb.updateStatus = UpdateStatus::UPDATED_WITHOUT_BUILDING;
        return false;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);
    getCompileCommand(cppFullCompileCommand, target->useIPC ? CommandType::USE_IPC : CommandType::CONVENTIONAL, "");
    if (dryRun)
    {
        cppFullCompileCommand += '\n';
        printMessage(cppFullCompileCommand);
        return false;
    }

    if (!target->useIPC)
    {
        run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, false);
        return true;
    }

    rb.assignNeedsUpdateToDependents();
    headerFiles.clear();
    allCppModDependencies.clear();

    run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, true);
    ipcManager = new IPCManagerBS(run.writePipe);

    return true;
}

void CppMod::completeModuleCompilation(const Builder &builder)
{
    RealBTarget &rb = realBTargets[0];
    if (rb.exitStatus != EXIT_SUCCESS)
    {
        rb.updateStatus = UpdateStatus::UPDATED_WITHOUT_BUILDING;
        print(builder, *run.output);
        return;
    }

    if (type == CppModType::HEADER_UNIT || type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
    {
        P2978::BMIFile file{.filePath = interfaceNode->filePath};
        if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(file); !r2)
        {
            printErrorMessage(FORMAT("Failed to create shared-memory BMI-file {}\nof target {}\n",
                                     interfaceNode->filePath, target->name));
        }
        interfaceFileSize = file.fileSize;
        memoryMappingCompleted = true;
    }

    if (target->useIPC)
    {
        if (target->configuration->evaluate(DuplicationWarning::YES))
        {
            for (auto &[str, headerFile] : composingHeaders)
            {
                if (const auto &[it, ok] = headerNodeCppMod.emplace(headerFile, this); !ok)
                {
                    printMessage(FORMAT("Warning! In target {}\nIn CppMod {}\n, header-file {}\n is also being "
                                        "provided by CppMod.\n{}\n",
                                        target->name, node->filePath, headerFile->filePath,
                                        it->second->node->filePath));
                }
            }

            for (CppMod *dep : allCppModDependencies)
            {
                for (const auto &[headerFile, cppMod] : dep->headerNodeCppMod)
                {
                    if (const auto &[it, ok] = headerNodeCppMod.emplace(headerFile, cppMod); !ok)
                    {
                        printMessage(
                            FORMAT("Warning! In target {}\nIn CppMod {}\n, header-file {}\n is being included by "
                                   "2 CppMod.\n{}\n{}\n",
                                   target->name, node->filePath, headerFile->filePath, cppMod->node->filePath,
                                   it->second->node->filePath));
                    }
                }
            }
        }
        else
        {
            if (!target->ignoreHeaderDeps)
            {
                for (auto &[str, headerFile] : composingHeaders)
                {
                    headerFiles.emplace(headerFile);
                }

                for (CppMod *cppMod : allCppModDependencies)
                {
                    for (Node *headerFile : cppMod->headerFiles)
                    {
                        headerFiles.emplace(headerFile);
                    }
                }
            }
        }
    }

    rb.updateStatus = UpdateStatus::UPDATED;
    // This must be just before printing as this is asynchronousl accessed in savBuildCache
    target->buildCacheUpdated = true;
    print(builder, *run.output);
}

bool CppMod::isEventCompleted(Builder &builder, string_view message)
{
    // todo
    //  this is performance critical code. following improvements can be done.
    // 1) CppSrc::headerFiles can be made a vector or probable a pointer in a large list-buffer. this buffer can have
    // multiple lists of these header-files, and every CppSrc and CppMod having a pointer to the list. something like
    // Builder::updateBTargets.
    // 2) above string_view message can passed directly as part of output instead of first separating it out.
    // 3) Probably don't do colored output and copy from the Ninja. also take look for optimizing the header-file
    // parsing from msvc output and gcc .d files
    // 4) run.output can be reused to keep memory usage limited.

    RealBTarget &rb = realBTargets[0];
    if (!target->useIPC)
    {
        parseHeaderDeps(*run.output, builder);
        completeModuleCompilation(builder);
        return false;
    }

    if (waitingFor)
    {
        ++builder.activeEventCount;
        if (waitingFor->realBTargets[0].exitStatus != EXIT_SUCCESS)
        {
            waitingFor = nullptr;
            run.killModuleProcess(builder);
            rb.exitStatus = EXIT_FAILURE;
            rb.updateStatus = UpdateStatus::UPDATED_WITHOUT_BUILDING;
            return false;
        }

        if (waitingFor->type == CppModType::HEADER_UNIT)
        {
            makeAndSendBTCNonModule(*waitingFor);
        }
        else
        {
            makeAndSendBTCModule(*waitingFor);
        }

        waitingFor = nullptr;

        // Wait for more input
        return true;
    }

    if (message.empty())
    {
        completeModuleCompilation(builder);
        return false;
    }

    char buffer[320];
    P2978::CTB requestType;
    if (const auto &r = IPCManagerBS::receiveMessage(buffer, requestType, message); !r)
    {
        printErrorMessage(
            FORMAT("receive-message fail for module-file {}\n of target {}\n.", node->filePath, target->name));
    }

    CppMod *found;

    if (requestType == P2978::CTB::NON_MODULE)
    {
        auto &[isHURequested, headerName] = reinterpret_cast<P2978::CTBNonModule &>(buffer);

        HeaderFileOrUnit f = findHeaderFileOrUnit(headerName);
        if (!f.data.cppMod)
        {
            printErrorMessage(FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this "
                                     "header \n{}\n requested in {}\n",
                                     target->name, target->getDependenciesString(), headerName, node->filePath));
        }

        // Checking if this is a big header-unit with composing header-files. Composing headers should be
        // included in the big header with same logical-name as they are meant to be used in other files. So we
        // can use the same headerName to search whether we have a composing header specified. Otherwise, it
        // would be diagnosed as cyclic dependency.
        if (f.data.cppMod == this && !firstMessageSent)
        {
            if (const auto it = composingHeaders.find(headerName); it != composingHeaders.end())
            {
                f = HeaderFileOrUnit{(it->second), false};
            }
        }

        if (f.isUnit)
        {
            found = f.data.cppMod;
        }
        else
        {
            if (isHURequested)
            {
                printErrorMessage(FORMAT("Could not find the header-unit {} requested by file {}\n in target {}.\n",
                                         headerName, node->filePath, target->name));
            }

            constexpr uint32_t stackSize = 64 * 1024;
            char buffer[stackSize];
            std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
            std::pmr::string toBeSend(&alloc);

            // BTCNonModule::isHeaderUnit
            writeBool(toBeSend, false);
            // BTCNonModule::isSystem
            writeBool(toBeSend, f.isSystem);
            uint32_t placeHolderIndex = toBeSend.size();
            uint32_t count = 0;

            bool addedInComposingHeader = false;
            if (!firstMessageSent)
            {
                writeUint32(toBeSend, -1); // placeholder
                firstMessageSent = true;
                for (const auto &[str, composingNode] : composingHeaders)
                {
                    if (f.data.node == composingNode && headerName == str)
                    {
                        addedInComposingHeader = true;
                        continue;
                    }

                    ++count;

                    writeStringView(toBeSend, str);
                    // HeaderFile::filePath
                    writeStringView(toBeSend, composingNode->filePath);
                    toBeSend.push_back('\n');
                    // HeaderFile::isSystem
                    writeBool(toBeSend, target->isSystem);
                }
                *reinterpret_cast<uint32_t *>(&toBeSend[placeHolderIndex]) = count;
            }
            else
            {
                writeUint32(toBeSend, 0);
            }

            // BTCNonModule::filePath
            writeStringView(toBeSend, f.data.node->filePath);
            toBeSend.push_back('\n');
            toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

            if (const auto &r2 = ipcManager->writeInternal(toBeSend); !r2)
            {
                printErrorMessage(FORMAT("send-message fail of header-file {}\n for module-file {}\n of target {}\n.",
                                         f.data.node->filePath, node->filePath, target->name));
            }

            if (!addedInComposingHeader)
            {
                if (!composingHeaders.emplace(headerName, f.data.node).second)
                {
                    printErrorMessage(FORMAT("An already sent header-file \n{}\n re-requested in file.\n{}\n",
                                             f.data.node->filePath, node->filePath));
                }
            }

            return true;
        }
    }
    else
    {
        string_view moduleName = reinterpret_cast<P2978::CTBModule &>(buffer).moduleName;
        found = findModule(moduleName);

        if (!found)
        {
            if (moduleName.contains(':'))
            {
                printErrorMessage(FORMAT("No File in the target\n{}\n provides this module\n{}.\n requested in file {}",
                                         target->name, moduleName, node->filePath));
            }
            else
            {
                printErrorMessage(FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides "
                                         "this module\n{}.\n requested in file {}",
                                         target->name, target->getDependenciesString(), moduleName, node->filePath));
            }
        }
    }

    if (!allCppModDependencies.emplace(found).second)
    {
        printErrorMessage(FORMAT("Warning: already sent the module {}\n with logical-name{}\n requested in {}\n.",
                                 found->node->filePath, found->logicalNames[0], node->filePath));
    }

    RealBTarget &foundRb = found->realBTargets[0];

    if (foundRb.updateStatus != UpdateStatus::UPDATED && foundRb.updateStatus != UpdateStatus::UPDATED_WITHOUT_BUILDING)
    {
        waitingFor = found;
        foundRb.dependents.emplace(&rb, BTargetDepType::FULL);
        rb.dependencies.emplace(&foundRb, BTargetDepType::FULL);
        ++rb.dependenciesSize;

        // if its dependenciesSize is zero, it means that it is already in the list. We just bring it to the front.
        if (!found->isScheduled && foundRb.dependenciesSize == 0)
        {
            found->isScheduled = true;
            // Old index is reset and then we re-add
            builder.updateBTargets.array[foundRb.insertionIndex].value = nullptr;
            uint32_t insertionIndex = 0;
            builder.updateBTargets.emplace(&foundRb, insertionIndex);
            foundRb.insertionIndex = insertionIndex; // not needed probably
        }
        ++builder.updateBTargetsSizeGoal;
        // This process is going to idle. Build-system will automatically decrement when it launches a new process.
        ++builder.idleCount;
        --builder.activeEventCount;
        return true;
    }

    if (target->configuration->evaluate(StandAloneCommand::YES))
    {
        // we need this in standalone build to be able to generate the script for all the dependencies.
        foundRb.dependents.emplace(&rb, BTargetDepType::FULL);
        rb.dependencies.emplace(&foundRb, BTargetDepType::FULL);
    }

    if (foundRb.exitStatus != EXIT_SUCCESS)
    {
        run.killModuleProcess(builder);
        rb.exitStatus = EXIT_FAILURE;
        rb.updateStatus = UpdateStatus::UPDATED_WITHOUT_BUILDING;
        return false;
    }

    if (requestType == P2978::CTB::MODULE)
    {
        makeAndSendBTCModule(*found);
    }
    else
    {
        makeAndSendBTCNonModule(*found);
    }

    // Requested module or header-unit has been sent. Let's get the next message.
    return true;
}

void CppMod::print(const Builder &builder, const string &output) const
{
    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string outputStr(&alloc);
    if (isConsole)
    {
        outputStr += getColorCode(type == CppModType::HEADER_UNIT ? ColorIndex::hot_pink : ColorIndex::magenta);
    }

    if (output.empty())
    {
        outputStr += FORMAT("[{}/{}]C++{} {} {}", builder.updatedCount, builder.updateBTargetsSizeGoal,
                            type == CppModType::HEADER_UNIT ? "Header-Unit" : "Module", node->filePath, target->name);
    }
    else
    {
        getCompileCommand(outputStr, target->useIPC ? CommandType::USE_IPC : CommandType::CONVENTIONAL, "");
    }

    outputStr += ' ';

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += output;
    outputStr.push_back('\n');

    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
}

BTargetType CppMod::getBTargetType() const
{
    return BTargetType::CPPMOD;
}

void CppMod::updateBuildCache()
{
    if (realBTargets[0].updateStatus != UpdateStatus::UPDATED)
    {
        return;
    }

    // myBuildCache is only updated here only if the build has succeeded.

    myBuildCache->srcFile.compileCommand = commandHash;
    myBuildCache->srcFile.launchTime = globalLaunchTime;
    myBuildCache->srcFile.headerFiles.clear();
    myBuildCache->headerUnitArray.clear();
    myBuildCache->moduleArray.clear();

    if (target->configuration->evaluate(DuplicationWarning::YES))
    {
        // headerNodeCppMod is still populated as we want to warn for duplicate transitive header-file dependencies. So
        // we need to check for ignoreHeaderDeps
        if (target->ignoreHeaderDeps)
        {
            for (const auto &[header, cppMod] : headerNodeCppMod)
            {
                myBuildCache->srcFile.headerFiles.emplace_back(header);
            }
        }
    }
    else
    {
        for (Node *header : headerFiles)
        {
            myBuildCache->srcFile.headerFiles.emplace_back(header);
        }
    }

    myBuildCache->headerStatusChanged = false;
    for (const CppMod *dep : allCppModDependencies)
    {
        if (dep->type == CppModType::HEADER_UNIT)
        {
            BuildCache::Cpp::ModuleFile::SingleHeaderUnitDep huDep;
            huDep.node = const_cast<Node *>(dep->node);
            huDep.compileCommand = dep->commandHash;
            huDep.myIndex = dep->myBuildCacheIndex;
            huDep.targetIndex = dep->target->cacheIndex;
            myBuildCache->headerUnitArray.emplace_back(huDep);
        }
        else
        {
            BuildCache::Cpp::ModuleFile::SingleModuleDep modDep;
            modDep.node = dep->objectNode;
            modDep.compileCommand = dep->commandHash;
            modDep.myIndex = dep->myBuildCacheIndex;
            modDep.targetIndex = dep->target->cacheIndex;
            myBuildCache->moduleArray.emplace_back(modDep);
        }
    }
}

void CppMod::getCompileCommand(std::pmr::string &compileCommand, CommandType commandType,
                               const string_view mockFilePath) const
{
    if (sourceType != SourceType::CPP && type != CppModType::PRIMARY_IMPLEMENTATION)
    {
        printErrorMessage(FORMAT("Module-File {}\nof CppTarget {}\n is implementation-unit but not a C++ file\n",
                                 node->filePath, target->name));
    }

    if (sourceType == SourceType::CPP)
    {
        compileCommand = target->configuration->cppCompileCommand;
    }
    else if (sourceType == SourceType::C)
    {
        compileCommand = target->configuration->cCompileCommand;
    }
    else if (sourceType == SourceType::ASSEMBLY)
    {
        compileCommand = target->configuration->assemblyCompileCommand;
    }

    target->setCompileCommand(compileCommand);
    compileCommand += "-Wno-experimental-header-units ";

    // if addMockFile is true, then -fuseIPC="mock-file-path" is used instead of -useIPC
    string useIPCsTR;
    if (commandType == CommandType::USE_IPC_MOCK_FILE)
    {
        useIPCsTR = "-useIPC=\"";
        useIPCsTR += mockFilePath;
        useIPCsTR += "\" ";
    }
    else if (commandType == CommandType::USE_IPC)
    {
        useIPCsTR = "-useIPC ";
    }
    if (const Compiler &c = target->configuration->compilerFeatures.compiler;
        c.bTFamily == BTFamily::MSVC && c.btSubFamily == BTSubFamily::CLANG)
    {
        if (type == CppModType::HEADER_UNIT)
        {
            compileCommand +=
                (target->isSystem ? "-fmodule-header=system /clang:-o\"" : "-fmodule-header=user /clang:-o\"") +
                interfaceNode->filePath + "\" " + useIPCsTR + "-x c++-header \"" + node->filePath + '\"';
        }
        else if (type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c -x c++-module \"" +
                              node->filePath + "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand += "-o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c /TP \"" + node->filePath + '\"';
        }

        if (isConsole)
        {
            compileCommand += " -fdiagnostics-color=always";
        }
        else
        {
            compileCommand += " -fdiagnostics-color=never";
        }
    }
    else if (c.bTFamily == BTFamily::GCC && c.btSubFamily == BTSubFamily::CLANG)
    {
        compileCommand += commandType == CommandType::CONVENTIONAL ? "" : "-nostdinc -nostdinc++ ";
        if (type == CppModType::HEADER_UNIT)
        {
            compileCommand += (target->isSystem ? "-fmodule-header=system -o\"" : "-fmodule-header=user -o\"") +
                              interfaceNode->filePath + "\" " + useIPCsTR + "-x c++-header \"" + node->filePath + '\"';
        }
        else if (type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c -x c++-module \"" +
                              node->filePath + "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand += "-o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c \"" + node->filePath + '\"';
        }

        if (isConsole)
        {
            compileCommand += " -fdiagnostics-color=always ";
        }
        else
        {
            compileCommand += " -fdiagnostics-color=never ";
        }
    }

    if (commandType != CommandType::CONVENTIONAL)
    {
        return;
    }

    // Only for convention command-line approach if the compiler supports such.
    for (const CppMod *mod : allCppModDependencies)
    {
        compileCommand += "-fmodule-file=\"" + mod->interfaceNode->filePath + "\" ";
    }
}

void CppMod::setFileStatusAndPopulateAllDependencies()
{
    if (node->filePath.contains("public-lib3.hpp"))
    {
        bool breakpoint = true;
    }

    if (node->fileType == file_type::not_found)
    {
        printErrorMessage(FORMAT("Module-file {}\n of target {}\n not found.\n", node->filePath, target->name));
    }

    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
    {
        return;
    }

    rb.updateStatus = UpdateStatus::NEEDS_UPDATE;

    const Node *endNode = type == CppModType::HEADER_UNIT ? interfaceNode : objectNode;

    if (target->ignoreHeaderDeps)
    {
        if (endNode->fileType == file_type::not_found)
        {
            return;
        }

        for (const BuildCache::Cpp::ModuleFile::SingleHeaderUnitDep &h : myBuildCache->headerUnitArray)
        {
            CppMod *hu = nullptr;
            if (const CppTarget *t = static_cast<CppTarget *>(fileTargetCaches[h.targetIndex].targetCache))
            {
                hu = t->huDeps[h.myIndex];
            }

            if (!hu)
            {
                return;
            }

            if (hu->commandHash != h.compileCommand)
            {
                return;
            }

            allCppModDependencies.emplace(hu);
        }

        for (const BuildCache::Cpp::ModuleFile::SingleModuleDep &m : myBuildCache->moduleArray)
        {
            CppMod *cppMod = nullptr;
            if (const CppTarget *t = static_cast<CppTarget *>(fileTargetCaches[m.targetIndex].targetCache))
            {
                cppMod = t->imodFileDeps[m.myIndex];
            }

            if (!cppMod)
            {
                return;
            }

            if (cppMod->commandHash != m.compileCommand)
            {
                return;
            }

            allCppModDependencies.emplace(cppMod);
        }

        rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
        return;
    }

    const uint64_t cachedLaunchTime = myBuildCache->srcFile.launchTime;

    if (endNode->fileType == file_type::not_found || node->lastWriteTime > cachedLaunchTime)
    {
        return;
    }

    for (Node *headerNode : myBuildCache->srcFile.headerFiles)
    {
        if (headerNode->fileType == file_type::not_found || headerNode->lastWriteTime > cachedLaunchTime)
        {
            return;
        }
        headerFiles.emplace(headerNode);
    }

    for (const BuildCache::Cpp::ModuleFile::SingleHeaderUnitDep &h : myBuildCache->headerUnitArray)
    {
        CppMod *hu = nullptr;
        if (const CppTarget *t = static_cast<CppTarget *>(fileTargetCaches[h.targetIndex].targetCache))
        {
            hu = t->huDeps[h.myIndex];
        }

        if (!hu)
        {
            return;
        }

        if (hu->node->fileType == file_type::not_found)
        {
            printErrorMessage(
                FORMAT("Header-Unit {}\n of target {}\n not found.\n", hu->node->filePath, hu->target->name));
        }

        if (hu->commandHash != h.compileCommand)
        {
            return;
        }

        if (hu->node->lastWriteTime > cachedLaunchTime)
        {
            return;
        }

        allCppModDependencies.emplace(hu);
    }

    for (const BuildCache::Cpp::ModuleFile::SingleModuleDep &m : myBuildCache->moduleArray)
    {
        CppMod *cppMod = nullptr;
        if (const CppTarget *t = static_cast<CppTarget *>(fileTargetCaches[m.targetIndex].targetCache))
        {
            cppMod = t->imodFileDeps[m.myIndex];
        }

        if (!cppMod)
        {
            return;
        }

        if (cppMod->node->fileType == file_type::not_found)
        {
            printErrorMessage(
                FORMAT("Module-file {}\n of target {}\n not found.\n", cppMod->node->filePath, cppMod->target->name));
        }

        if (cppMod->commandHash != m.compileCommand)
        {
            return;
        }

        // for clang 2-phase compilation, following would be incorrect. Need more thinking.
        if (cppMod->node->lastWriteTime > cachedLaunchTime)
        {
            return;
        }

        allCppModDependencies.emplace(cppMod);
    }

    rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
}

void CppMod::generateStandAloneCommand()
{
    if (target->configuration->evaluate(StandAloneCommand::YES))
    {
        if (const RealBTarget &rb = realBTargets[0];
            rb.updateStatus == UpdateStatus::UPDATED ||
            (rb.updateStatus == UpdateStatus::UPDATED_WITHOUT_BUILDING && rb.exitStatus != EXIT_SUCCESS))
        {
            path scriptDirectory = target->myBuildDir->filePath;
            scriptDirectory /= node->getFileName() + toString(node->myId);
            std::filesystem::create_directory(scriptDirectory);
            string scriptContents =
                FORMAT("#!/bin/bash\n\nset -x\n\n# This script compiles {}. Run it in the build-dir with same "
                       "name as current build-dir.\n\n",
                       node->filePath);
            flat_hash_set<string> createdDirs;
            cppStandAloneCommand(createdDirs, scriptContents, scriptDirectory.string());
            std::ofstream(scriptDirectory / "script.sh") << scriptContents;
        }
    }
}

void CppMod::cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents, const string &scriptDir)
{
    btree_set<BTarget *, IndexInTopologicalSortComparatorRoundTwo> allDeps;

    for (const auto &[dep, depType] : realBTargets[0].dependencies)
    {
        allDeps.emplace(dep->bTarget);
    }

    for (CppMod *cppMod : allCppModDependencies)
    {
        allDeps.emplace(cppMod);
        for (const auto &[dep, depType] : cppMod->realBTargets[0].dependencies)
        {
            allDeps.emplace(dep->bTarget);
        }
    }

    // This step is done so that if there are custom code generation steps, then those could be added to the script file
    // as well.
    for (auto it = allDeps.rbegin(); it != allDeps.rend(); ++it)
    {
        (*it)->cppStandAloneCommand(createdDirs, scriptContents, scriptDir);
    }

    if (createdDirs.emplace(target->configuration->name).second)
    {
        scriptContents += "mkdir " + target->configuration->name + '\n';
    }

    if (createdDirs.emplace(target->name).second)
    {
        scriptContents += "mkdir " + target->name + '\n';
    }

    if (!target->useIPC)
    {
        constexpr uint32_t stackSize = 64 * 1024;
        char buffer[stackSize];
        std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
        std::pmr::string cppFullCompileCommand(&alloc);

        getCompileCommand(cppFullCompileCommand,
                          target->useIPC ? CommandType::USE_IPC_MOCK_FILE : CommandType::CONVENTIONAL, "");

        scriptContents += cppFullCompileCommand;
        scriptContents.push_back('\n');
        return;
    }

    const string mockFilePath = (path(scriptDir) / FORMAT("mock-file{}.bin", id)).string();
    {
        // New scope for mock-file.bin
        constexpr uint32_t stackSize = 256 * 1024;
        char buffer[stackSize];
        std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
        std::pmr::string mockFileContents(&alloc);

        flat_hash_set<string> ignoreNames;

        uint32_t count = 0;
        writeUint32(mockFileContents, count);

        for (const CppMod *cppMod : allCppModDependencies)
        {
            for (const string &logicalName : cppMod->logicalNames)
            {
                ignoreNames.emplace(logicalName);
                ++count;
                writeStringView(mockFileContents, logicalName);
                writeStringView(mockFileContents, cppMod->interfaceNode->filePath);
                mockFileContents.push_back('\0');
                P2978::FileType file;
                if (cppMod->type == CppModType::HEADER_UNIT)
                {
                    file = P2978::FileType::HEADER_UNIT;
                }
                else
                {
                    file = P2978::FileType::MODULE;
                }
                writeUint8(mockFileContents, static_cast<uint8_t>(file));
                writeBool(mockFileContents, cppMod->target->isSystem);
            }
        }

        for (auto &[str, headerFile] : composingHeaders)
        {
            if (ignoreNames.emplace(str).second)
            {
                ++count;
                writeStringView(mockFileContents, str);
                writeStringView(mockFileContents, headerFile->filePath);
                mockFileContents.push_back('\0');
                writeUint8(mockFileContents, static_cast<uint8_t>(FileType::HEADER_FILE));
                writeBool(mockFileContents, target->isSystem);
            }
        }

        *reinterpret_cast<uint32_t *>(mockFileContents.data()) = count;
        std::ofstream(mockFilePath) << mockFileContents;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);

    getCompileCommand(cppFullCompileCommand,
                      target->useIPC ? CommandType::USE_IPC_MOCK_FILE : CommandType::CONVENTIONAL, mockFilePath);

    scriptContents += cppFullCompileCommand;
    scriptContents.push_back('\n');
}
