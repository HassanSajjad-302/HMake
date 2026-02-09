
#include "CppMod.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "CppTarget.hpp"
#include "JConsts.hpp"
#include "TargetCache.hpp"

#include <fcntl.h>
#include <filesystem>
#include <memory_resource>
// #include <sys/wait.h>
#include "PointerPacking.h"
#include <utility>

using std::tie, std::ifstream, std::exception, std::lock_guard, N2978::IPCManagerBS;

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
    if (buildCache.compileCommand.hash != commandHash)
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
    string treatedOutput;

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
            output = std::move(treatedOutput);
            break;
        }
        /*if (lineEnd > output.size() - 5)
        {
            bool breakpoint = true;
        }*/
        lineEnd = output.find('\n', startPos);
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
                headerFiles.emplace(Node::getHalfNodeST(headerView));
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

    if (objectNode->fileType == file_type::not_found || node->lastWriteTime > objectNode->lastWriteTime)
    {
        return;
    }

    for (const vector<Node *> &headers = target->cppBuildCache.srcFiles[myBuildCacheIndex].headerFiles;
         const Node *headerNode : headers)
    {
        if (headerNode->fileType == file_type::not_found || headerNode->lastWriteTime > objectNode->lastWriteTime)
        {
            return;
        }
    }

    rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
}

void CppSrc::updateBuildCache()
{
    BuildCache::Cpp::SourceFile &buildCache = target->cppBuildCache.srcFiles[myBuildCacheIndex];

    if (atomic_ref(realBTargets[0].updateStatus).load(std::memory_order_acquire) != UpdateStatus::UPDATED ||
        realBTargets[0].exitStatus != EXIT_SUCCESS)
    {
        return;
    }

    buildCache.compileCommand.hash = commandHash;
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

bool CppSrc::launchBTarget(Builder &builder)
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

bool CppSrc::completeBTarget(Builder &builder, const uint64_t index, uint32_t &activeCount)
{
    if (!run.isReadCompleted(index).completed)
    {
        return true;
    }

    builder.unregisterEventDataAtIndex(index);
    run.reapProcess(false);
    realBTargets[0].exitStatus = run.exitStatus;

    parseHeaderDeps(run.output, builder);

    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        atomic_ref(realBTargets[0].updateStatus).store(UpdateStatus::UPDATED, std::memory_order_release);
        atomic_ref(target->buildCacheUpdated).store(true, std::memory_order_release);
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

    if (run.output.empty())
    {
        outputStr += FORMAT("C++Source {} {} ", node->filePath, target->name);
    }
    else
    {
        getCompileCommand(outputStr);
    }

    outputStr += threadIds[myThreadIndex];

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += run.output;
    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
    return false;
}

bool operator<(const CppSrc &lhs, const CppSrc &rhs)
{
    return lhs.node < rhs.node;
}

bool CppMod::launchBTarget(Builder &builder)
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

    if (waitingFor)
    {
        //
        return build(builder, -1, string_view{});
    }

    if (realBTargets[0].exitStatus != EXIT_SUCCESS)
    {
        return false;
    }

    if (huOnly && type != SM_FILE_TYPE::HEADER_UNIT)
    {
        realBTargets[0].exitStatus = EXIT_FAILURE;
        return false;
    }

    setFileStatusAndPopulateAllDependencies();

    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus == UpdateStatus::ALREADY_UPDATED)
    {
        atomic_ref(rb.updateStatus).store(UpdateStatus::UPDATED, std::memory_order_release);
        return false;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);
    getCompileCommand(cppFullCompileCommand);
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
    myBuildCache->headerUnitArray.clear();
    myBuildCache->moduleArray.clear();

    run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, true);
    ipcManager = new IPCManagerBS(run.writePipe);

    return true;
}

bool CppMod::wasIPCLaunchIncomplete(const uint64_t index)
{
#ifdef _WIN32
    if (processState == ProcessState::CONNECTED)
    {
        return false;
    }

    uint64_t bytesRead = 0;
    CompletionKey &k = eventData[index];
    const bool result =
        GetOverlappedResult((HANDLE)k.handle, &(OVERLAPPED &)k.overlappedBuffer, (LPDWORD)&bytesRead, false);
    if (result)
    {
        processState = ProcessState::CONNECTED;
        return true;
    }
    if (GetLastError() == ERROR_BROKEN_PIPE)
    {
        processState = ProcessState::COMPLETED;
        return true;
    }
    printErrorMessage(N2978::getErrorString());
#endif
    return false;
}

bool CppMod::completeBTarget(Builder &builder, const uint64_t index, uint32_t &activeCount)
{
    const ReadDataInfo &readDataInfo = run.isReadCompleted(index);
    if (!readDataInfo.completed)
    {
        return true;
    }

    RealBTarget &rb = realBTargets[0];
    if (!target->useIPC)
    {
        builder.unregisterEventDataAtIndex(index);
        run.reapProcess(false);
        rb.exitStatus = run.exitStatus;

        parseHeaderDeps(run.output, builder);
        if (rb.exitStatus == EXIT_SUCCESS)
        {
            if (type == SM_FILE_TYPE::HEADER_UNIT || type == SM_FILE_TYPE::PRIMARY_EXPORT ||
                type == SM_FILE_TYPE::PARTITION_EXPORT)
            {
                N2978::BMIFile file{.filePath = interfaceNode->filePath};
                if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(file); !r2)
                {
                    printErrorMessage(FORMAT("Failed to create shared-memory BMI-file {}\nof target {}\n",
                                             interfaceNode->filePath, target->name));
                }
                interfaceFileSize = file.fileSize;
                makeMemoryMappingCalled = true;
                makeMemoryMappingCompleted = true;
            }
        }

        atomic_ref(rb.updateStatus).store(UpdateStatus::UPDATED, std::memory_order_release);
        if (rb.exitStatus == EXIT_SUCCESS)
        {
            atomic_ref(target->buildCacheUpdated).store(true, std::memory_order_release);
        }

        print(run.output);
        return false;
    }

    return build(builder, index, readDataInfo.message);
}

CppMod::CppMod(CppTarget *target_, const Node *node_) : CppSrc(target_, node_)
{
}

void CppMod::initializeBuildCache(BuildCache::Cpp::ModuleFile &modCache, const uint32_t index,
                                  const uint64_t commandHash_)
{
    myBuildCacheIndex = index;
    myBuildCache = &modCache;
    commandHash = commandHash_;

    if (modCache.srcFile.compileCommand.hash != commandHash)
    {
        realBTargets[0].updateStatus = UpdateStatus::NEEDS_UPDATE;
        compileCommandChanged = true;
        return;
    }

    if (modCache.headerStatusChanged)
    {
        realBTargets[0].updateStatus = UpdateStatus::NEEDS_UPDATE;
        return;
    }

    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        interfaceNode->toBeChecked = true;
    }
    else
    {
        objectNode->toBeChecked = true;
        if (type == SM_FILE_TYPE::PARTITION_EXPORT || type == SM_FILE_TYPE::PRIMARY_EXPORT)
        {
            interfaceNode->toBeChecked = true;
        }
    }

    if (target->ignoreHeaderDeps)
    {
        return;
    }

    const_cast<bool &>(node->toBeChecked) = true;
    for (const vector<Node *> headers = modCache.srcFile.headerFiles; Node *headerFile : headers)
    {
        headerFile->toBeChecked = true;
    }
}

void CppMod::makeMemoryFileMapping()
{
    if (type == SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
    {
        return;
    }

    if (atomic_ref(makeMemoryMappingCompleted).load(std::memory_order_acquire))
    {
        return;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!atomic_ref(makeMemoryMappingCalled).exchange(true))
    {
        N2978::BMIFile file;
        file.filePath = interfaceNode->filePath;
        if (const auto &r = IPCManagerBS::createSharedMemoryBMIFile(file); !r)
        {
            printErrorMessage(FORMAT("Could not make mapping for the file {}\n of target {}\n. Error {}\n",
                                     node->filePath, target->name, r.error()));
        }
        interfaceFileSize = file.fileSize;
        atomic_ref(makeMemoryMappingCompleted).store(true, std::memory_order_release);
        return;
    }

    // systemCheck is being called for this node by another thread
    while (!atomic_ref(makeMemoryMappingCompleted).load(std::memory_order_acquire))
    {
    }
}

void CppMod::makeAndSendBTCModule(CppMod &mod)
{
    // todo
    // write this buffer directly.

    mod.makeMemoryFileMapping();
    N2978::BTCModule btcModule;
    btcModule.requested.filePath = mod.interfaceNode->filePath;
    btcModule.requested.fileSize = mod.interfaceFileSize;
    btcModule.isSystem = mod.target->isSystem;

    for (CppMod *modDep : mod.allCppModDependencies)
    {
        if (allCppModDependencies.emplace(modDep).second)
        {
            modDep->makeMemoryFileMapping();
            N2978::ModuleDep dep;
            dep.isHeaderUnit = modDep->type == SM_FILE_TYPE::HEADER_UNIT;
            dep.file.filePath = modDep->interfaceNode->filePath;
            dep.file.fileSize = modDep->interfaceFileSize;
            for (const string &l : modDep->logicalNames)
            {
                dep.logicalNames.emplace_back(l);
            }
            dep.isSystem = modDep->target->isSystem;
            btcModule.modDeps.emplace_back(std::move(dep));

            if (!target->ignoreHeaderDeps)
            {
                for (Node *headerFile : modDep->headerFiles)
                {
                    headerFiles.emplace(headerFile);
                }
            }
        }
    }

    if (const auto &r2 = ipcManager->sendMessage(btcModule); !r2)
    {
        printErrorMessage(FORMAT("send-message fail of module-file {}\n for module-file {}\n of target {}\n.",
                                 mod.node->filePath, node->filePath, target->name));
    }

    if (target->ignoreHeaderDeps)
    {
        return;
    }

    for (Node *headerFile : mod.headerFiles)
    {
        headerFiles.emplace(headerFile);
    }
}

// For debugging purposes
N2978::BTCNonModule deserializeBTCNonModule(std::string_view buffer)
{
    N2978::BTCNonModule result;
    const char *ptr = buffer.data();
    uint32_t bytesRead = 0;

    // BTCNonModule::isHeaderUnit
    result.isHeaderUnit = readBool(ptr, bytesRead);

    // BTCNonModule::isSystem
    result.isSystem = readBool(ptr, bytesRead);

    // BTCNonModule::filePath
    std::string_view filePathView = readStringView(ptr, bytesRead);
    result.filePath = std::string(filePathView);

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

    // BTCNonModule::headerFiles
    uint32_t headerFilesCount = readUint32(ptr, bytesRead);
    result.headerFiles.reserve(headerFilesCount);
    for (uint32_t i = 0; i < headerFilesCount; ++i)
    {
        N2978::HeaderFile hf;

        // HeaderFile::logicalName
        std::string_view logicalNameView = readStringView(ptr, bytesRead);
        hf.logicalName = std::string(logicalNameView);

        // HeaderFile::filePath
        std::string_view filePathView = readStringView(ptr, bytesRead);
        hf.filePath = std::string(filePathView);

        // HeaderFile::isSystem
        hf.isSystem = readBool(ptr, bytesRead);

        result.headerFiles.emplace_back(std::move(hf));
    }

    // BTCNonModule::huDeps
    uint32_t huDepsCount = readUint32(ptr, bytesRead);
    result.huDeps.reserve(huDepsCount);
    for (uint32_t i = 0; i < huDepsCount; ++i)
    {
        N2978::HuDep dep;

        // HuDep::file::filePath
        std::string_view filePathView = readStringView(ptr, bytesRead);
        dep.file.filePath = std::string(filePathView);

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
    if (bytesRead != buffer.size())
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
    // BTCNonModule::filePath
    writeStringView(toBeSend, hu.interfaceNode->filePath);
    // BTCNonModule::fileSize
    writeUint32(toBeSend, hu.interfaceFileSize);
    // BTCNonModule::logicalNames
    writeUint32(toBeSend, hu.logicalNames.size());
    for (const string &str : hu.logicalNames)
    {
        writeStringView(toBeSend, str);
    }

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
            // HeaderFile::isSystem
            writeBool(toBeSend, target->isSystem);

            if (!target->ignoreHeaderDeps)
            {
                headerFiles.emplace(composingNode);
            }
        }
    }
    else
    {
        // BTCNonModule::headerFiles
        writeUint32(toBeSend, 0);
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
            N2978::HuDep dep;

            // HuDep::file::filePath
            writeStringView(toBeSend, huDep->interfaceNode->filePath);
            // HuDep::file::fileSize
            writeUint32(toBeSend, huDep->interfaceFileSize);

            // HuDep::logicalNames
            writeUint32(toBeSend, huDep->logicalNames.size());
            for (const string &str : huDep->logicalNames)
            {
                dep.logicalNames.emplace_back(str);
                writeStringView(toBeSend, str);
            }

            // BTCNonModule::isSystem
            writeBool(toBeSend, huDep->target->isSystem);

            if (!target->ignoreHeaderDeps)
            {
                for (Node *headerFile : huDep->headerFiles)
                {
                    headerFiles.emplace(headerFile);
                }
            }
        }
    }
    *reinterpret_cast<uint32_t *>(&toBeSend[placeHolderIndex]) = count;

    if (const auto &r2 = ipcManager->writeInternal(toBeSend); !r2)
    {
        printErrorMessage(FORMAT("send-message fail of header-unit {}\n for {} {}\n of target {}\n.", hu.node->filePath,
                                 type == SM_FILE_TYPE::HEADER_UNIT ? "header-unit" : "module-filee", node->filePath,
                                 target->name));
    }

    if (target->ignoreHeaderDeps)
    {
        return;
    }

    for (Node *headerFile : hu.headerFiles)
    {
        headerFiles.emplace(headerFile);
    }
}

CppMod *CppMod::findModule(const string &moduleName) const
{
    if (const auto it = target->imodNames.find(moduleName); it != target->imodNames.end())
    {
        return it->second;
    }

    if (!moduleName.contains(':'))
    {
        for (const uint32_t index : target->reqDepsVecIndices)
        {
            CppTarget *req = static_cast<CppTarget *>(fileTargetCaches[index].targetCache);
            if (auto it2 = req->imodNames.find(moduleName); it2 != req->imodNames.end())
            {
                return it2->second;
            }
        }
    }

    return nullptr;
}

HeaderFileOrUnit CppMod::findHeaderFileOrUnit(const string &headerName) const
{
    if (const auto &it = target->reqHeaderNameMapping.find(headerName); it != target->reqHeaderNameMapping.end())
    {
        return it->second;
    }

    for (const uint32_t index : target->reqDepsVecIndices)
    {
        CppTarget *req = static_cast<CppTarget *>(fileTargetCaches[index].targetCache);
        if (const auto &it = req->useReqHeaderNameMapping.find(headerName); it != req->useReqHeaderNameMapping.end())
        {
            return it->second;
        }
    }

    return {static_cast<Node *>(nullptr), false};
}

void CppMod::print(const string &output) const
{
    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string outputStr(&alloc);
    if (isConsole)
    {
        outputStr += getColorCode(type == SM_FILE_TYPE::HEADER_UNIT ? ColorIndex::hot_pink : ColorIndex::magenta);
    }

    if (output.empty())
    {
        outputStr += FORMAT("C++{} {} {}", type == SM_FILE_TYPE::HEADER_UNIT ? "Header-Unit" : "Module", node->filePath,
                            target->name);
    }
    else
    {
        getCompileCommand(outputStr);
    }

    outputStr += ' ';
    outputStr += threadIds[myThreadIndex];

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += output;

    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
}

bool CppMod::build(Builder &builder, uint64_t index, string_view message)
{
    RealBTarget &rb = realBTargets[0];
    if (waitingFor)
    {
        if (waitingFor->realBTargets[0].exitStatus != EXIT_SUCCESS)
        {
            run.killModuleProcess(type == SM_FILE_TYPE::HEADER_UNIT ? interfaceNode->filePath : objectNode->filePath);
            rb.exitStatus = EXIT_FAILURE;
            atomic_ref(rb.updateStatus).store(UpdateStatus::UPDATED, std::memory_order_release);
            return false;
        }

        if (waitingFor->type == SM_FILE_TYPE::HEADER_UNIT)
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
        builder.unregisterEventDataAtIndex(index);
        run.reapProcess(true);
        rb.exitStatus = run.exitStatus;

        if (rb.exitStatus == EXIT_SUCCESS)
        {
            if (type == SM_FILE_TYPE::HEADER_UNIT || type == SM_FILE_TYPE::PRIMARY_EXPORT ||
                type == SM_FILE_TYPE::PARTITION_EXPORT)
            {
                N2978::BMIFile file{.filePath = interfaceNode->filePath};
                if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(file); !r2)
                {
                    printErrorMessage(FORMAT("Failed to create shared-memory BMI-file {}\nof target {}\n",
                                             interfaceNode->filePath, target->name));
                }
                interfaceFileSize = file.fileSize;
                makeMemoryMappingCalled = true;
                makeMemoryMappingCompleted = true;
            }
        }

        atomic_ref(rb.updateStatus).store(UpdateStatus::UPDATED, std::memory_order_release);
        if (rb.exitStatus == EXIT_SUCCESS)
        {
            atomic_ref(target->buildCacheUpdated).store(true, std::memory_order_release);
        }

        print(run.output);

        return false;
    }

    ipcManager->serverReadString = message;
    char buffer[320];
    N2978::CTB requestType;
    if (const auto &r = ipcManager->receiveMessage(buffer, requestType); !r)
    {
        printErrorMessage(
            FORMAT("receive-message fail for module-file {}\n of target {}\n.", node->filePath, target->name));
    }

    CppMod *found;

    if (requestType == N2978::CTB::NON_MODULE)
    {
        auto &[isHURequested, headerName] = reinterpret_cast<N2978::CTBNonModule &>(buffer);

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
            // BTCNonModule::filePath
            writeStringView(toBeSend, f.data.node->filePath);
            // BTCNonModule::fileSize
            writeUint32(toBeSend, 0);
            // BTCNonModule::logicalNames
            writeUint32(toBeSend, 0);

            uint32_t placeHolderIndex = toBeSend.size();
            uint32_t count = 0;

            bool addedInComposingHeader = false;
            if (!firstMessageSent)
            {
                writeUint32(toBeSend, -1); // placeholder
                firstMessageSent = true;
                for (const auto &[str, composingNode] : composingHeaders)
                {
                    if (!target->ignoreHeaderDeps)
                    {
                        headerFiles.emplace(composingNode);
                    }

                    if (f.data.node == composingNode && headerName == str)
                    {
                        addedInComposingHeader = true;
                        continue;
                    }

                    ++count;

                    writeStringView(toBeSend, str);
                    // HeaderFile::filePath
                    writeStringView(toBeSend, composingNode->filePath);
                    // HeaderFile::isSystem
                    writeBool(toBeSend, target->isSystem);
                }
                *reinterpret_cast<uint32_t *>(&toBeSend[placeHolderIndex]) = count;
            }
            else
            {
                writeUint32(toBeSend, 0);
            }
            // BTCNonModule::huDeps
            writeUint32(toBeSend, 0);

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

                if (!target->ignoreHeaderDeps)
                {
                    headerFiles.emplace(f.data.node);
                }
            }

            // Requested header-file has been sent. Let's receive the next message
            if constexpr (os == OS::NT)
            {
                // Only on Windows
                /*if (Builder::isRecurrentReadFromEventDataCompleted(ipcManagerIndex, message))
                {
                    printErrorMessage("IpcManager exited before the CTBLastMessage receival.\n");
                }*/
            }
            return true;
        }
    }
    else
    {
        const string &moduleName = reinterpret_cast<N2978::CTBModule &>(buffer).moduleName;
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

    if (foundRb.updateStatus != UpdateStatus::UPDATED)
    {
        waitingFor = found;
        // can be updated during the mutex locking
        if (foundRb.updateStatus != UpdateStatus::UPDATED)
        {
            foundRb.dependents.emplace(&rb, BTargetDepType::FULL);
            rb.dependencies.emplace(&foundRb, BTargetDepType::FULL);
            ++rb.dependenciesSize;
            ++builder.updateBTargetsSizeGoal;
            // This process is going to idle. Build-system will automatically decrement when it launches a new process.
            ++builder.idleCount;
            return true;
        }
        waitingFor = nullptr;
    }

    if (foundRb.exitStatus != EXIT_SUCCESS)
    {
        run.killModuleProcess(type == SM_FILE_TYPE::HEADER_UNIT ? interfaceNode->filePath : objectNode->filePath);
        rb.exitStatus = EXIT_FAILURE;
        atomic_ref(rb.updateStatus).store(UpdateStatus::UPDATED, std::memory_order_release);
        builder.unregisterEventDataAtIndex(index);
        return false;
    }

    if (requestType == N2978::CTB::MODULE)
    {
        makeAndSendBTCModule(*found);
    }
    else
    {
        makeAndSendBTCNonModule(*found);
    }

    /*if (eventData[ipcManagerIndex].firstCall == false)
    {
        bool breakpoint = true;
    }*/

    if constexpr (os == OS::NT)
    {
        // Only on Windows.
        /*if (Builder::isRecurrentReadFromEventDataCompleted(ipcManagerIndex, message))
        {
            printErrorMessage("IpcManager exited before the CTBLastMessage receival.\n");
        }*/
    }
    // Requested module or header-unit has been sent. Let's get the next message.
    return true;
}

BTargetType CppMod::getBTargetType() const
{
    return BTargetType::CPPMOD;
}

void CppMod::updateBuildCache()
{
    if (atomic_ref(realBTargets[0].updateStatus).load(std::memory_order_acquire) != UpdateStatus::UPDATED ||
        realBTargets[0].exitStatus != EXIT_SUCCESS)
    {
        return;
    }

    myBuildCache->srcFile.compileCommand.hash = commandHash;
    myBuildCache->srcFile.headerFiles.clear();
    for (Node *header : headerFiles)
    {
        myBuildCache->srcFile.headerFiles.emplace_back(header);
    }

    myBuildCache->headerStatusChanged = false;
    for (const CppMod *cppMod : allCppModDependencies)
    {
        if (cppMod->type == SM_FILE_TYPE::HEADER_UNIT)
        {
            BuildCache::Cpp::ModuleFile::SingleHeaderUnitDep huDep;
            huDep.node = const_cast<Node *>(cppMod->node);
            huDep.myIndex = cppMod->myBuildCacheIndex;
            huDep.targetIndex = cppMod->target->cacheIndex;
            myBuildCache->headerUnitArray.emplace_back(huDep);
        }
        else
        {
            BuildCache::Cpp::ModuleFile::SingleModuleDep modDep;
            modDep.node = cppMod->objectNode;
            modDep.myIndex = cppMod->myBuildCacheIndex;
            modDep.targetIndex = cppMod->target->cacheIndex;
            myBuildCache->moduleArray.emplace_back(modDep);
        }
    }
}

void CppMod::getCompileCommand(std::pmr::string &compileCommand) const
{
    if (sourceType != SourceType::CPP && type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
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

    const string useIPC = target->useIPC ? "-noScanIPC " : "";
    if (const Compiler &c = target->configuration->compilerFeatures.compiler;
        c.bTFamily == BTFamily::MSVC && c.btSubFamily == BTSubFamily::CLANG)
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            compileCommand +=
                (target->isSystem ? "-fmodule-header=system /clang:-o\"" : "-fmodule-header=user /clang:-o\"") +
                interfaceNode->filePath + "\" " + useIPC + "-xc++-header \"" + node->filePath + '\"';
        }
        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNode->filePath + "\" " + useIPC + "-c -xc++-module \"" + node->filePath +
                              "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand += "-o \"" + objectNode->filePath + "\" " + useIPC + "-c /TP \"" + node->filePath + '\"';
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
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            compileCommand += (target->isSystem ? "-fmodule-header=system -o\"" : "-fmodule-header=user -o\"") +
                              interfaceNode->filePath + "\" " + useIPC + "-xc++-header \"" + node->filePath + '\"';
        }
        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNode->filePath + "\" " + useIPC + "-c -xc++-module \"" + node->filePath +
                              "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand += "-o \"" + objectNode->filePath + "\" " + useIPC + "-c \"" + node->filePath + '\"';
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
}

void CppMod::setFileStatusAndPopulateAllDependencies()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
    {
        return;
    }

    rb.updateStatus = UpdateStatus::NEEDS_UPDATE;

    const Node *endNode = type == SM_FILE_TYPE::HEADER_UNIT ? interfaceNode : objectNode;

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
                if (h.myIndex < t->huDeps.size())
                {
                    hu = t->huDeps[h.myIndex];
                }
            }

            if (!hu)
            {
                return;
            }

            if (hu->compileCommandChanged)
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
                if (m.myIndex < t->imodFileDeps.size())
                {
                    cppMod = t->imodFileDeps[m.myIndex];
                }
            }

            if (!cppMod)
            {
                return;
            }

            if (cppMod->compileCommandChanged)
            {
                return;
            }

            allCppModDependencies.emplace(cppMod);
        }

        rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
        return;
    }

    if (node->fileType == file_type::not_found)
    {
        printErrorMessage(FORMAT("Module-file {}\n of target {}\n not found.\n", node->filePath, target->name));
    }

    if (endNode->fileType == file_type::not_found || node->lastWriteTime > endNode->lastWriteTime)
    {
        return;
    }

    const vector<Node *> *headerFilesCache = nullptr;
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        headerFilesCache = &target->cppBuildCache.headerUnits[myBuildCacheIndex].srcFile.headerFiles;
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        headerFilesCache = &target->cppBuildCache.imodFiles[myBuildCacheIndex].srcFile.headerFiles;
    }
    else
    {
        headerFilesCache = &target->cppBuildCache.modFiles[myBuildCacheIndex].srcFile.headerFiles;
    }

    for (const Node *headerNode : *headerFilesCache)
    {
        if (headerNode->fileType == file_type::not_found || headerNode->lastWriteTime > endNode->lastWriteTime)
        {
            return;
        }
    }

    for (const BuildCache::Cpp::ModuleFile::SingleHeaderUnitDep &h : myBuildCache->headerUnitArray)
    {
        CppMod *hu = nullptr;
        if (const CppTarget *t = static_cast<CppTarget *>(fileTargetCaches[h.targetIndex].targetCache))
        {
            if (h.myIndex < t->huDeps.size())
            {
                hu = t->huDeps[h.myIndex];
            }
        }

        if (!hu)
        {
            return;
        }

        if (hu->node->fileType == file_type::not_found)
        {
            printErrorMessage(
                FORMAT("Module-file {}\n of target {}\n not found.\n", hu->node->filePath, hu->target->name));
        }

        if (hu->compileCommandChanged)
        {
            return;
        }

        if (!hu->target->ignoreHeaderDeps)
        {
            if (hu->node->lastWriteTime > endNode->lastWriteTime)
            {
                return;
            }
        }

        allCppModDependencies.emplace(hu);
    }

    for (const BuildCache::Cpp::ModuleFile::SingleModuleDep &m : myBuildCache->moduleArray)
    {
        CppMod *cppMod = nullptr;
        if (const CppTarget *t = static_cast<CppTarget *>(fileTargetCaches[m.targetIndex].targetCache))
        {
            if (m.myIndex < t->imodFileDeps.size())
            {
                cppMod = t->imodFileDeps[m.myIndex];
            }
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

        if (cppMod->compileCommandChanged)
        {
            return;
        }

        if (!cppMod->target->ignoreHeaderDeps)
        {
            if (cppMod->node->lastWriteTime > objectNode->lastWriteTime)
            {
                return;
            }
        }

        allCppModDependencies.emplace(cppMod);
    }

    rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
}