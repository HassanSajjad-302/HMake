
#include "SMFile.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CacheWriteManager.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include "Settings.hpp"
#include "TargetCache.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <mutex>
#include <utility>

using std::tie, std::ifstream, std::exception, std::lock_guard, N2978::IPCManagerBS;

bool CompareSourceNode::operator()(const SourceNode &lhs, const SourceNode &rhs) const
{
    return lhs.node < rhs.node;
}

bool CompareSourceNode::operator()(const Node *lhs, const SourceNode &rhs) const
{
    return lhs < rhs.node;
}

bool CompareSourceNode::operator()(const SourceNode &lhs, const Node *rhs) const
{
    return lhs.node < rhs;
}

SourceNode::SourceNode(CppSourceTarget *target_, const Node *node_)
    : ObjectFile(true, false), target(target_), node{node_}
{
}

SourceNode::SourceNode(CppSourceTarget *target_, const Node *node_, const bool add0, const bool add1)
    : ObjectFile(add0, add1), target(target_), node{node_}
{
}

string SourceNode::getPrintName() const
{
    return node->filePath;
}

void SourceNode::initializeBuildCache(const uint32_t index)
{
    indexInBuildCache = index;
    const BuildCache::Cpp::SourceFile &buildCache = target->cppBuildCache.srcFiles[index];
    if (buildCache.compileCommandWithTool.hash != target->compileCommandWithTool.getHash())
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

string SourceNode::getCompileCommand() const
{
    const Compiler &compiler = target->configuration->compilerFeatures.compiler;
    string compileCommand = target->compileCommand;

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        compileCommand += "-c /nologo /showIncludes /TP \"" + node->filePath + "\" /Fo\"" + objectNode->filePath + "\"";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        compileCommand += "-c -MMD \"" + node->filePath + "\" -o \"" + objectNode->filePath + "\"";
    }

    return compileCommand;
}

void SourceNode::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (!round && selectiveBuild)
    {
        setSourceNodeFileStatus();
        if (RealBTarget &rb = realBTargets[0]; rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
        {
            thrIndex = myThreadIndex;
            rb.assignFileStatusToDependents();

            RunCommand r;
            r.startProcess(getCompileCommand(), false);
            auto [output, exitStatus] = r.endProcess(false);
            realBTargets[0].exitStatus = exitStatus;
            compilationOutput = std::move(output);

            CacheWriteManager::addNewEntry(target, this);
        }
    }
}

bool SourceNode::ignoreHeaderFile(const string_view child) const
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

void SourceNode::parseDepsFromMSVCTextOutput(string &output, const bool isClang)
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

void SourceNode::parseDepsFromGCCDepsOutput()
{
    if (target->ignoreHeaderDeps)
    {
        return;
    }
    const string headerFileContents = fileToString(target->myBuildDir->filePath + slashc + node->getFileName() + ".d");
    vector<string> headerDeps = split(headerFileContents, "\n");

    // The First 2 lines are skipped as these are .o and .cpp file.
    // If the file is preprocessed, it does not generate the extra line
    const auto endIt = headerDeps.end() - 1;

    if (headerDeps.size() > 2)
    {
        for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
        {
            const size_t pos = iter->find_first_not_of(" ");
            auto it = iter->begin() + pos;
            if (const string_view headerView{&*it, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos};
                !ignoreHeaderFile(headerView))
            {
                headerFiles.emplace(Node::getHalfNode(headerView));
            }
        }
    }
}

void SourceNode::parseHeaderDeps(string &output)
{
    if (target->configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(output,
                                    target->configuration->compilerFeatures.compiler.btSubFamily == BTSubFamily::CLANG);
    }
    else
    {
        parseDepsFromGCCDepsOutput();
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

void SourceNode::setSourceNodeFileStatus()
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

    for (vector<Node *> &headers = target->cppBuildCache.srcFiles[indexInBuildCache].headerFiles;
         const Node *headerNode : headers)
    {
        if (headerNode->fileType == file_type::not_found || headerNode->lastWriteTime > objectNode->lastWriteTime)
        {
            return;
        }
    }

    rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
}

void SourceNode::updateBuildCache(string &outputStr, string &errorStr, bool &buildCacheModified)
{
    const Compiler &compiler = target->configuration->compilerFeatures.compiler;
    BuildCache::Cpp::SourceFile &buildCache = target->cppBuildCache.srcFiles[indexInBuildCache];

    // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed
    // because cached compile-command would be different
    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        buildCacheModified = true;
        buildCache.compileCommandWithTool.hash = target->compileCommandWithTool.getHash();
        parseHeaderDeps(compilationOutput);
        buildCache.headerFiles.clear();
        for (Node *header : headerFiles)
        {
            buildCache.headerFiles.emplace_back(header);
        }
    }
    else if (compiler.bTFamily == BTFamily::MSVC)
    {
        // MSVC print header-files even when compilation fails.
        parseHeaderDeps(compilationOutput);
    }

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::cyan);
    }

    if (compilationOutput.empty())
    {
        outputStr += FORMAT("C++Source {} {} ", node->filePath, target->name);
    }
    else
    {
        outputStr += getCompileCommand() + '\n';
    }

    outputStr += threadIds[thrIndex];

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += compilationOutput;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return lhs.node < rhs.node;
}

SMFile::SMFile(CppSourceTarget *target_, const Node *node_) : SourceNode(target_, node_, true, false)
{
}

void SMFile::initializeBuildCache(BuildCache::Cpp::ModuleFile &modCache, const uint32_t index)
{
    indexInBuildCache = index;

    if (modCache.srcFile.compileCommandWithTool.hash != target->compileCommandWithTool.getHash())
    {
        realBTargets[0].updateStatus = UpdateStatus::NEEDS_UPDATE;
        compileCommandChanged = true;
        return;
    }

    if (modCache.smRules.headerStatusChanged)
    {
        realBTargets[0].updateStatus = UpdateStatus::NEEDS_UPDATE;
        return;
    }

    smRulesCache = modCache.smRules;
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
    for (const vector<Node *> headers = modCache.srcFile.headerFiles; Node * headerFile : headers)
    {
        headerFile->toBeChecked = true;
    }

    headerFilesCache = &modCache.srcFile.headerFiles;
}

void SMFile::makeAndSendBTCModule(SMFile &mod)
{
    N2978::BTCModule btcModule;
    btcModule.requested.filePath = mod.interfaceNode->filePath;
    btcModule.isSystem = mod.target->isSystem;

    N2978::ModuleDep dep;
    for (SMFile *modDep : mod.allSMFileDependencies)
    {
        if (allSMFileDependencies.emplace(modDep).second)
        {
            dep.isHeaderUnit = modDep->type == SM_FILE_TYPE::HEADER_UNIT;
            dep.file.filePath = modDep->interfaceNode->filePath;
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

void SMFile::makeAndSendBTCNonModule(SMFile &hu)
{
    if (node->filePath.ends_with("public-10.hpp"))
    {
        bool breakpoint = true;
    }

    N2978::BTCNonModule btcNonModule;
    btcNonModule.isHeaderUnit = true;
    btcNonModule.isSystem = hu.target->isSystem;
    btcNonModule.filePath = hu.interfaceNode->filePath;
    for (const string &str : hu.logicalNames)
    {
        btcNonModule.logicalNames.emplace_back(str);
    }

    if (!firstMessageSent)
    {
        firstMessageSent = true;
        for (auto &[str, node] : composingHeaders)
        {
            // emplace in header-files to send
            N2978::HeaderFile h{.logicalName = str, .filePath = node->filePath, .isSystem = target->isSystem};
            btcNonModule.headerFiles.emplace_back(std::move(h));

            if (!target->ignoreHeaderDeps)
            {
                headerFiles.emplace(node);
            }
        }
    }

    for (SMFile *huDep : hu.allSMFileDependencies)
    {
        if (allSMFileDependencies.emplace(huDep).second)
        {
            N2978::HuDep dep;

            dep.file.filePath = huDep->interfaceNode->filePath;
            for (const string &str : huDep->logicalNames)
            {
                dep.logicalNames.emplace_back(str);
            }
            btcNonModule.huDeps.emplace_back(std::move(dep));
            btcNonModule.isSystem = huDep->target->isSystem;

            if (!target->ignoreHeaderDeps)
            {
                for (Node *headerFile : huDep->headerFiles)
                {
                    headerFiles.emplace(headerFile);
                }
            }
        }
    }

    if (node->filePath.ends_with("public-10.hpp"))
    {
        bool brekapoint = true;
    }

    if (const auto &r2 = ipcManager->sendMessage(btcNonModule); !r2)
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

void SMFile::duplicateHeaderFileOrUnitError(const string &headerName, HeaderFileOrUnit &first, HeaderFileOrUnit &second,
                                            CppSourceTarget *firstTarget, CppSourceTarget *secondTarget)
{
    string str = FORMAT("For CTBNonModule {} received from module-file {} of target {}, "
                        "there are duplicate entries.\n",
                        headerName, node->filePath, target->name);
    if (first.isUnit)
    {
        str += FORMAT("Header-Unit {} of target {}\n", first.data.smFile->node->filePath, firstTarget->name);
    }
    else
    {
        str += FORMAT("Header-File {} of target {}\n", first.data.node->filePath, firstTarget->name);
    }

    if (second.isUnit)
    {
        str += FORMAT("Header-Unit {} of target {}\n", second.data.smFile->node->filePath, secondTarget->name);
    }
    else
    {
        str += FORMAT("Header-File {} of target {}\n", second.data.node->filePath, secondTarget->name);
    }

    printErrorMessage(str);
}

SMFile *SMFile::findModule(const string &moduleName) const
{
    SMFile *found = nullptr;

    if (const auto it = target->imodNames.find(moduleName); it != target->imodNames.end())
    {
        found = it->second;
    }

    if (!moduleName.contains(':'))
    {
        for (CppSourceTarget *req : target->reqDeps)
        {
            if (auto it2 = req->imodNames.find(moduleName); it2 != req->imodNames.end())
            {
                if (found)
                {
                    printErrorMessage(FORMAT("Module name:\n {}\n Is Being Provided By 2 different files:\n1){}\n"
                                             "from target\n{}\n2){}\n from target\n{}\n",
                                             moduleName, found->node->filePath, found->target->name,
                                             it2->second->node->filePath, it2->second->target->name));
                }
                else
                {
                    found = it2->second;
                }
            }
        }
    }

    return found;
}

HeaderFileOrUnit SMFile::findHeaderFileOrUnit(const string &headerName)
{
    HeaderFileOrUnit found;
    CppSourceTarget *foundTarget = nullptr;
    if (const auto &it = target->reqHeaderNameMapping.find(headerName); it != target->reqHeaderNameMapping.end())
    {
        found = it->second;
        foundTarget = target;
    }

    for (CppSourceTarget *t : target->reqDeps)
    {
        if (const auto &it = t->useReqHeaderNameMapping.find(headerName); it != t->useReqHeaderNameMapping.end())
        {
            if (found.data.smFile)
            {
                duplicateHeaderFileOrUnitError(headerName, found, it->second, foundTarget, t);
            }
            found = it->second;
            foundTarget = t;
        }
    }

    // Checking if this is a big header-unit with composing header-files. Composing headers should be included in the
    // big header with same logical-name as they are meant to be used in other files. So we can use the same headerName
    // to search whether we have a composing header specified. Otherwise, it would be a cyclic dependency.
    if (found.data.smFile == this && !firstMessageSent)
    {
        if (const auto it = composingHeaders.find(headerName); it != composingHeaders.end())
        {
            return {const_cast<Node *>(it->second), false};
        }
    }

    return found;
}

bool SMFile::build(Builder &builder)
{
    if (node->filePath.ends_with("public-10.hpp"))
    {
        bool breakpoint = true;
    }
    RealBTarget &rb = realBTargets[0];
    if (waitingFor)
    {
        if (waitingFor->realBTargets[0].exitStatus != EXIT_SUCCESS)
        {
            run.killModuleProcess(type == SM_FILE_TYPE::HEADER_UNIT ? interfaceNode->filePath : objectNode->filePath);
            rb.exitStatus = EXIT_FAILURE;
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
    }

    while (true)
    {
        char buffer[320];
        N2978::CTB requestType;
        if (const auto &r = ipcManager->receiveMessage(buffer, requestType); r)
        {
            if (requestType == N2978::CTB::LAST_MESSAGE)
            {
                auto &lastMessage = reinterpret_cast<N2978::CTBLastMessage &>(buffer);

                ipcManager->closeConnection();
                auto [_, exitStatus] = run.endProcess(true);
                rb.exitStatus = exitStatus;
                assert(rb.exitStatus == lastMessage.errorOccurred && "error-status mismatch");
                compilationOutput = std::move(lastMessage.errorOutput);

                CacheWriteManager::addNewEntry(target, this);
                return false;
            }

            SMFile *found;

            if (requestType == N2978::CTB::NON_MODULE)
            {
                auto &[isHURequested, headerName] = reinterpret_cast<N2978::CTBNonModule &>(buffer);

                if (headerName == "windows.h")
                {
                    bool breakpoint = true;
                }

                if (node->filePath.ends_with("public-10.hpp"))
                {
                    N2978::CTBNonModule non_module = static_cast<N2978::CTBNonModule>(buffer);
                    bool breakpoint = true;
                }

                const HeaderFileOrUnit f = findHeaderFileOrUnit(headerName);
                if (f.data.smFile == this)
                {
                    bool breakpoint = true;
                }
                if (!f.data.smFile)
                {
                    printErrorMessage(FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this "
                                             "header \n{}.\n requested in {}\n",
                                             target->name, target->getDependenciesString(), headerName,
                                             node->filePath));
                }

                if (f.isUnit)
                {
                    found = f.data.smFile;
                }
                else
                {
                    if (isHURequested)
                    {
                        printErrorMessage(
                            FORMAT("Could not find the header-unit {} requested by file {}\n in target {}.\n",
                                   headerName, node->filePath, target->name));
                    }
                    N2978::BTCNonModule response;

                    response.filePath = f.data.node->filePath;
                    response.isHeaderUnit = false;
                    response.isSystem = f.isSystem;

                    bool addedInComposingHeader = false;
                    if (!firstMessageSent)
                    {
                        firstMessageSent = true;
                        for (const auto &[str, node] : composingHeaders)
                        {
                            if (!target->ignoreHeaderDeps)
                            {
                                headerFiles.emplace(f.data.node);
                            }

                            if (f.data.node == node && headerName == str)
                            {
                                addedInComposingHeader = true;
                                continue;
                            }

                            // emplace in header-files to send
                            N2978::HeaderFile h{
                                .logicalName = str, .filePath = node->filePath, .isSystem = target->isSystem};
                            response.headerFiles.emplace_back(std::move(h));
                        }
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

                    if (const auto &r2 = ipcManager->sendMessage(response); !r2)
                    {
                        printErrorMessage(
                            FORMAT("send-message fail of header-file {}\n for module-file {}\n of target {}\n.",
                                   f.data.node->filePath, node->filePath, target->name));
                    }

                    continue;
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
                        printErrorMessage(
                            FORMAT("No File in the target\n{}\n provides this module\n{}.\n requested in file {}",
                                   target->name, moduleName, node->filePath));
                    }
                    else
                    {
                        printErrorMessage(FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides "
                                                 "this module\n{}.\n requested in file {}",
                                                 target->name, target->getDependenciesString(), moduleName,
                                                 node->filePath));
                    }
                }
            }

            if (!allSMFileDependencies.emplace(found).second)
            {
                printErrorMessage(
                    FORMAT("Warning: already sent the module {}\n with logical-name{}\n requested in {}\n.",
                           found->node->filePath, found->logicalNames[0], node->filePath));
            }

            RealBTarget &foundRb = found->realBTargets[0];

            if (node->filePath.ends_with("public-10.hpp"))
            {
                N2978::CTBNonModule non_module = static_cast<N2978::CTBNonModule>(buffer);
                bool breakpoint = true;
            }
            if (foundRb.updateStatus != UpdateStatus::UPDATED)
            {
                waitingFor = found;
                builder.executeMutex.lock();
                // can be updated during the mutex locking
                if (foundRb.updateStatus != UpdateStatus::UPDATED)
                {
                    fflush(stdout);
                    foundRb.dependents.emplace(&rb, BTargetDepType::FULL);
                    rb.dependencies.emplace(&foundRb, BTargetDepType::FULL);
                    ++rb.dependenciesSize;
                    ++builder.updateBTargetsSizeGoal;
                    return true;
                }
                builder.executeMutex.unlock();
            }

            if (foundRb.exitStatus != EXIT_SUCCESS)
            {
                run.killModuleProcess(type == SM_FILE_TYPE::HEADER_UNIT ? interfaceNode->filePath
                                                                        : objectNode->filePath);
                rb.exitStatus = EXIT_FAILURE;
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
        }
        else
        {
            printErrorMessage(
                FORMAT("receive-message fail for module-file {}\n of target {}\n.", node->filePath, target->name));
        }
    }
}

void SMFile::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (!round && selectiveBuild)
    {
        if (waitingFor)
        {
            isComplete = build(builder);
            return;
        }

        if (realBTargets[0].exitStatus == EXIT_SUCCESS)
        {
            setFileStatusAndPopulateAllDependencies();
            if (RealBTarget &rb = realBTargets[0]; rb.updateStatus == UpdateStatus::NEEDS_UPDATE)
            {
                rb.assignFileStatusToDependents();
                headerFiles.clear();
                allSMFileDependencies.clear();
                rb.assignFileStatusToDependents();

                const Node *endNode = type == SM_FILE_TYPE::HEADER_UNIT ? interfaceNode : objectNode;
                if (auto r = N2978::makeIPCManagerBS(endNode->filePath); r)
                {
                    ipcManager = new IPCManagerBS(*r);
                }
                else
                {
                    printErrorMessage(FORMAT("Could not make the build-system manager {}\n", endNode->filePath));
                }

                const string compileCommand = target->compileCommand + getCompileCommand();
                if (node->filePath.ends_with("public-10.hpp"))
                {
                    bool breakpoint = true;
                    run.startProcess(compileCommand, true);
                }
                else
                {
                    run.startProcess(compileCommand, true);
                    bool breakpoint = false;
                }
                isComplete = build(builder);
            }
        }
    }
}

string SMFile::getOutputFileName() const
{
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        // node->getFileName() is not used to prevent error in case header-file with same fileName exists in 2
        // different include directories. But is being included by different logicalName.
        string str = logicalNames[0];
        for (char &c : str)
        {
            if (c == '/')
            {
                c = '-';
            }
        }
        return str;
    }
    return node->getFileName();
}

BTargetType SMFile::getBTargetType() const
{
    return BTargetType::SMFILE;
}

void SMFile::updateBuildCache(string &outputStr, string &errorStr, bool &buildCacheModified)
{
    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        buildCacheModified = true;
        smRulesCache = BuildCache::Cpp::ModuleFile::SmRules{};
        smRulesCache.headerStatusChanged = false;
        for (const SMFile *smFile : allSMFileDependencies)
        {
            if (smFile->type == SM_FILE_TYPE::HEADER_UNIT)
            {
                BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep huDep;
                huDep.node = const_cast<Node *>(smFile->node);
                huDep.myIndex = smFile->indexInBuildCache;
                huDep.targetIndex = smFile->target->cacheIndex;
                smRulesCache.headerUnitArray.emplace_back(huDep);
            }
            else
            {
                BuildCache::Cpp::ModuleFile::SmRules::SingleModuleDep modDep;
                modDep.node = smFile->objectNode;
                modDep.logicalName = smFile->logicalNames[0];
                smRulesCache.moduleArray.emplace_back(std::move(modDep));
            }
        }

        BuildCache::Cpp::ModuleFile *modFile;
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            modFile = &target->cppBuildCache.headerUnits[indexInBuildCache];
        }
        else if (type == SM_FILE_TYPE::PARTITION_EXPORT || type == SM_FILE_TYPE::PRIMARY_EXPORT)
        {
            modFile = &target->cppBuildCache.imodFiles[indexInBuildCache];
        }
        else
        {
            modFile = &target->cppBuildCache.modFiles[indexInBuildCache];
        }

        auto &[srcFile, smRules] = *modFile;
        srcFile.compileCommandWithTool.hash = target->compileCommandWithTool.getHash();
        srcFile.headerFiles.clear();
        for (Node *header : headerFiles)
        {
            srcFile.headerFiles.emplace_back(header);
        }
        smRules = std::move(smRulesCache);
    }

    if (isConsole)
    {
        outputStr += getColorCode(type == SM_FILE_TYPE::HEADER_UNIT ? ColorIndex::hot_pink : ColorIndex::magenta);
    }

    if (compilationOutput.empty())
    {
        outputStr += FORMAT("C++{} {} {}", type == SM_FILE_TYPE::HEADER_UNIT ? "Header-Unit" : "Module", node->filePath,
                            target->name);
    }
    else
    {
        outputStr += target->compileCommand + getCompileCommand();
    }

    outputStr += ' ';
    outputStr += threadIds[thrIndex];

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += compilationOutput;
}

string SMFile::getCompileCommand() const
{
    string s = "-Wno-experimental-header-units ";
    if (const Compiler &c = target->configuration->compilerFeatures.compiler;
        c.bTFamily == BTFamily::MSVC && c.btSubFamily == BTSubFamily::CLANG)
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            s += (target->isSystem ? "-fmodule-header=system /clang:-o\"" : "-fmodule-header=user /clang:-o\"") +
                 interfaceNode->filePath + "\" -noScanIPC -xc++-header \"" + node->filePath + '\"';
        }
        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            s += " -o \"" + objectNode->filePath + "\" -noScanIPC -c -xc++-module \"" + node->filePath +
                 "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            s += "-o \"" + objectNode->filePath + "\" -noScanIPC -c /TP \"" + node->filePath + '\"';
        }
    }

    return s;
}

void SMFile::setFileStatusAndPopulateAllDependencies()
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

        for (auto &[node, depModName] : smRulesCache.moduleArray)
        {
            SMFile *f = findModule(string(depModName));

            if (!f)
            {
                if (depModName.contains(':'))
                {
                    printErrorMessage(FORMAT("No File in the target\n{}\n provides this module\n{}.\n", target->name,
                                             string(depModName)));
                }
                else
                {
                    printErrorMessage(
                        FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this module\n{}.\n",
                               target->name, target->getDependenciesString(), string(depModName)));
                }
            }

            if (f->compileCommandChanged)
            {
                return;
            }

            allSMFileDependencies.emplace(f);
        }

        for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &h : smRulesCache.headerUnitArray)
        {
            SMFile *hu = nullptr;
            if (const CppSourceTarget *t = cppSourceTargets[h.targetIndex])
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

            allSMFileDependencies.emplace(hu);
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

    for (const Node *headerNode : *headerFilesCache)
    {
        if (headerNode->fileType == file_type::not_found || headerNode->lastWriteTime > endNode->lastWriteTime)
        {
            return;
        }
    }

    for (auto &[node, depModName] : smRulesCache.moduleArray)
    {
        SMFile *f = findModule(string(depModName));

        if (!f)
        {
            if (depModName.contains(':'))
            {
                printErrorMessage(FORMAT("No File in the target\n{}\n provides this module\n{}.\n", target->name,
                                         string(depModName)));
            }
            else
            {
                printErrorMessage(
                    FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this module\n{}.\n",
                           target->name, target->getDependenciesString(), string(depModName)));
            }
        }

        // some other file is providing this module. so this needs to be rebuilt.
        if (f->objectNode != node)
        {
            return;
        }

        if (f->node->fileType == file_type::not_found)
        {
            printErrorMessage(
                FORMAT("Module-file {}\n of target {}\n not found.\n", f->node->filePath, f->target->name));
        }

        if (f->compileCommandChanged)
        {
            return;
        }

        if (f->node->lastWriteTime > objectNode->lastWriteTime)
        {
            return;
        }

        allSMFileDependencies.emplace(f);
    }

    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &h : smRulesCache.headerUnitArray)
    {
        SMFile *hu = nullptr;
        if (const CppSourceTarget *t = cppSourceTargets[h.targetIndex])
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

        if (hu->node->lastWriteTime > endNode->lastWriteTime)
        {
            return;
        }

        allSMFileDependencies.emplace(hu);
    }

    rb.updateStatus = UpdateStatus::ALREADY_UPDATED;
}