
#ifdef USE_HEADER_UNITS
import "SMFile.hpp";

import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "CppSourceTarget.hpp";
import "JConsts.hpp";
import "PostBasic.hpp";
import "Settings.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <mutex>;
import <utility>;
#else
#include "SMFile.hpp"

#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include "PostBasic.hpp"
#include "Settings.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>
#endif

using std::filesystem::directory_entry, std::filesystem::file_type, std::tie, std::ifstream,
    std::filesystem::file_time_type, std::exception, std::lock_guard;

pstring getStatusPString(const path &p)
{
    switch (status(p).type())
    {
    case file_type::none:
        return " has `not-evaluated-yet` type";
    case file_type::not_found:
        return " does not exist";
    case file_type::regular:
        return " is a regular file";
    case file_type::directory:
        return " is a directory";
    case file_type::symlink:
        return " is a symlink";
    case file_type::block:
        return " is a block device";
    case file_type::character:
        return " is a character device";
    case file_type::fifo:
        return " is a named IPC pipe";
    case file_type::socket:
        return " is a named IPC socket";
    case file_type::unknown:
        return " has `unknown` type";
    default:
        return " has `implementation-defined` type";
    }
}

bool CompareNode::operator()(const Node &lhs, const Node &rhs) const
{
    return lhs.filePath < rhs.filePath;
}

bool CompareNode::operator()(const pstring &lhs, const Node &rhs) const
{
    return lhs < rhs.filePath;
}

bool CompareNode::operator()(const Node &lhs, const pstring &rhs) const
{
    return lhs.filePath < rhs;
}

Node::Node(const path &filePath_)
{
    filePath = (filePath_.*toPStr)();
}

std::mutex fileTimeUpdateMutex;
std::filesystem::file_time_type Node::getLastUpdateTime() const
{
    lock_guard<mutex> lk(fileTimeUpdateMutex);
    if (!isUpdated)
    {
        const_cast<std::filesystem::file_time_type &>(lastUpdateTime) = last_write_time(path(filePath));
        const_cast<bool &>(isUpdated) = true;
    }
    return lastUpdateTime;
}

/*path Node::getFinalNodePathFromString(const pstring &str)
{
    path filePath{str};
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = (filePath.lexically_normal().*toPStr)();

    if constexpr (os == OS::NT)
    {
        // Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        // actual paths. In Windows paths could be case-insensitive. Just another wrinkle hahaha.
        pstring lowerCase (= filePath*toPStr)());
        for (char &c : lowerCase)
        {
            // Warning: assuming paths to be ASCII
            c = tolower(c);
        }
        filePath = lowerCase;
    }
    return filePath;
}

static mutex nodeInsertMutex;
Node *Node::getNodeFromPath(const pstring &str, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromPath(str);

    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.

    {
        lock_guard<mutex> lk(nodeInsertMutex);
        if (auto it = allFiles.find((filePath*toPStr)())); it != allFiles.end())
        {
            return const_cast<Node *>(it.operator->());
        }
    }

    lock_guard<mutex> lk(nodeInsertMutex);
    return const_cast<Node *>(allFiles.emplace(filePath, isFile, mayNotExist).first.operator->());
}*/

path Node::getFinalNodePathFromPath(path filePath)
{
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = filePath.lexically_normal();

    if constexpr (os == OS::NT)
    {
        // Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        // actual paths. In Windows paths could be case-insensitive. Just another wrinkle hahaha.
        auto it = const_cast<path::value_type *>(filePath.c_str());
        for (; *it != '\0'; ++it)
        {
            *it = std::tolower(*it);
        }
    }
    return filePath;
}

static mutex nodeInsertMutex;
Node *Node::getNodeFromPath(const path &p, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromPath(p);

    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.

    Node *node = nullptr;
    {
        lock_guard<mutex> lk{nodeInsertMutex};
        if (auto it = allFiles.find((filePath.*toPStr)()); it != allFiles.end())
        {
            node = const_cast<Node *>(it.operator->());
        }
        else
        {
            node = const_cast<Node *>(allFiles.emplace(filePath).first.operator->());
        }
    }

    if (node->systemCheckCompleted.load(std::memory_order_relaxed))
    {
        return node;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!node->systemCheckCalled.exchange(true))
    {
        node->performSystemCheck(isFile, mayNotExist);
        node->systemCheckCompleted.store(true, std::memory_order_relaxed);
    }

    // systemCheck is being called for this node by another thread
    while (!node->systemCheckCompleted.load(std::memory_order_relaxed))
        ;

    return node;
}

void Node::performSystemCheck(bool isFile, bool mayNotExist)
{
    std::filesystem::file_type nodeType = directory_entry(filePath).status().type();
    if (nodeType == (isFile ? file_type::regular : file_type::directory))
    {
    }
    else
    {
        if (!mayNotExist || nodeType != file_type::not_found)
        {
            printErrorMessage(fmt::format("{} is not a {} file. File Type is {}\n", filePath,
                                          isFile ? "regular" : "directory", getStatusPString(filePath)));
            throw std::exception();
        }
        doesNotExist = true;
    }
}

bool operator<(const Node &lhs, const Node &rhs)
{
    return lhs.filePath < rhs.filePath;
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
}

LibDirNode::LibDirNode(Node *node_, bool isStandard_) : node{node_}, isStandard{isStandard_}
{
}

void LibDirNode::emplaceInList(list<LibDirNode> &libDirNodes, LibDirNode &libDirNode)
{
    for (LibDirNode &libDirNode_ : libDirNodes)
    {
        if (libDirNode_.node == libDirNode.node)
        {
            return;
        }
    }
    libDirNodes.emplace_back(libDirNode);
}

void LibDirNode::emplaceInList(list<LibDirNode> &libDirNodes, Node *node_, bool isStandard_)
{
    for (LibDirNode &libDirNode : libDirNodes)
    {
        if (libDirNode.node == node_)
        {
            return;
        }
    }
    libDirNodes.emplace_back(node_, isStandard_);
}

InclNode::InclNode(Node *node_, bool isStandard_, bool ignoreHeaderDeps_)
    : LibDirNode(node_, isStandard_), ignoreHeaderDeps{ignoreHeaderDeps_}
{
}

bool InclNode::emplaceInList(list<InclNode> &includes, InclNode &libDirNode)
{
    for (InclNode &include : includes)
    {
        if (include.node == libDirNode.node)
        {
            return false;
        }
    }
    includes.emplace_back(libDirNode);
    return true;
}

bool InclNode::emplaceInList(list<InclNode> &includes, Node *node_, bool isStandard_, bool ignoreHeaderDeps_)
{
    for (InclNode &include : includes)
    {
        if (include.node == node_)
        {
            return false;
        }
    }
    includes.emplace_back(node_, isStandard_, ignoreHeaderDeps_);
    return true;
}

bool operator<(const InclNode &lhs, const InclNode &rhs)
{
    return std::tie(lhs.node, lhs.isStandard, lhs.ignoreHeaderDeps) <
           std::tie(rhs.node, rhs.isStandard, rhs.ignoreHeaderDeps);
}

bool CompareSourceNode::operator()(const SourceNode &lhs, const SourceNode &rhs) const
{
    return lhs.node < rhs.node;
}

bool CompareSourceNode::operator()(Node *lhs, const SourceNode &rhs) const
{
    return lhs < rhs.node;
}

bool CompareSourceNode::operator()(const SourceNode &lhs, Node *rhs) const
{
    return lhs.node < rhs;
}

SourceNode::SourceNode(CppSourceTarget *target_, Node *node_) : target(target_), node{node_}
{
}

pstring SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".o",
                          pathPrint);
}

pstring SourceNode::getTarjanNodeName() const
{
    return node->filePath;
}

void SourceNode::updateBTarget(Builder &, unsigned short round)
{
    if (!round && fileStatus.load(std::memory_order_acquire) && selectiveBuild)
    {
        RealBTarget &realBTarget = getRealBTarget(round);
        assignFileStatusToDependents(realBTarget);
        PostCompile postCompile = target->updateSourceNodeBTarget(*this);
        postCompile.parseHeaderDeps(*this, round);
        realBTarget.exitStatus = postCompile.exitStatus;
        // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
        // cached compile-command would be different
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            (*sourceJson)[1].SetString(PTOREF(target->compileCommandWithTool));
        }
        lock_guard<mutex> lk(printMutex);
        postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
        fflush(stdout);
    }
}

void SourceNode::setSourceNodeFileStatus()
{
    Node *objectFileNode = Node::getNodeFromPath(objectFileOutputFilePath, true, true);

    if (sourceJson->operator[](1) != PValue(PTOREF(target->compileCommandWithTool)))
    {
        fileStatus.store(true, std::memory_order_release);
        return;
    }

    if (objectFileNode->doesNotExist)
    {
        fileStatus.store(true, std::memory_order_release);
        return;
    }

    if (node->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
    {
        fileStatus.store(true, std::memory_order_release);
        return;
    }

    for (PValue &str : sourceJson->operator[](2).GetArray())
    {
        Node *headerNode = Node::getNodeFromPath(str.GetString(), true, true);
        if (headerNode->doesNotExist)
        {
            fileStatus.store(true, std::memory_order_release);
            return;
        }

        if (headerNode->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
        {
            fileStatus.store(true, std::memory_order_release);
            return;
        }
    }
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    // j[JConsts::srcFile] = *(sourceNode.sourceJson);
}

void to_json(Json &j, const SourceNode *sourceNode)
{
    j = *sourceNode;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return lhs.node < rhs.node;
}

HeaderUnitConsumer::HeaderUnitConsumer(bool angle_, pstring logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
}

static mutex totalSMRuleFileCountMutex; // TODO: atomics should be used instead of this

void SMFile::decrementTotalSMRuleFileCount(Builder &builder)
{
    lock_guard<mutex> lk(totalSMRuleFileCountMutex);
    --target->moduleScopeData->totalSMRuleFileCount;
    if (!target->moduleScopeData->totalSMRuleFileCount)
    {
        // A smrule file was updated. And all module scope smrule files have been checked. Following is done to
        // write the CppSourceTarget .cache files. So, if because of an error during smrule generation of a file,
        // hmake is exiting after round 1, in next invocation, it won't generate the smrule of successfully
        // generated files.
        builder.addCppSourceTargetsInFinalBTargets(target->moduleScopeData->targets);
    }
}

void SMFile::updateBTarget(Builder &builder, unsigned short round)
{
    // Danger Following is executed concurrently
    RealBTarget &realBTarget = getRealBTarget(round);
    if (round == 1 && realBTarget.exitStatus == EXIT_SUCCESS)
    {
        if (generateSMFileInRoundOne())
        {
            // TODO
            //  Expose setting for printOnlyOnError
            PostCompile postCompile = target->GenerateSMRulesFile(*this, true);
            {
                lock_guard<mutex> lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                fflush(stdout);
            }
            postCompile.parseHeaderDeps(*this, round);
            realBTarget.exitStatus = postCompile.exitStatus;
        }
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            {
                saveRequiresJsonAndInitializeHeaderUnits(builder);
                assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
            }
            target->getRealBTarget(0).addDependency(*this);
        }
        decrementTotalSMRuleFileCount(builder);
    }
    else if (!round && realBTarget.exitStatus == EXIT_SUCCESS && selectiveBuild)
    {
        setFileStatusAndPopulateAllDependencies();

        // TODO
        // Add a different compile-command for smrules generation, so that smrules is not regenerated if e.g. the
        // compile command is changed because module compile-command is saved only when module is recompiled, while
        // smrule compile-command is saved when smrule is recompiled. The hash will be used and same hash can be used.

        // TODO
        // Here we will also check whether the module compile-command is changed to decide whether to recompile or not.
        // Tests might be reverted to older state.
        if (fileStatus.load(std::memory_order_acquire))
        {
            assignFileStatusToDependents(realBTarget);
            PostCompile postCompile = target->CompileSMFile(*this);
            realBTarget.exitStatus = postCompile.exitStatus;

            {
                lock_guard<mutex> lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                fflush(stdout);
            }

            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
                // cached compile-command would be different
                sourceJson->FindMember(PTOREF(JConsts::compileCommand))
                    ->value.SetString(PTOREF(target->compileCommandWithTool));
            }
        }
    }
}

void SMFile::saveRequiresJsonAndInitializeHeaderUnits(Builder &builder)
{
    if (readJsonFromSMRulesFile)
    {
        pstring smFilePath = target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".smrules";
        // readPValueFromFile(smFilePath.c_str(), sourceJson->FindMember(PTOREF(JConsts::smrules))->value);
    }

    PValue &rule = sourceJson->FindMember(PTOREF(JConsts::smrules))->value.FindMember(PTOREF(JConsts::rules))->value[0];
    if (rule.HasMember(PTOREF(JConsts::provides)))
    {
        hasProvide = true;
        // There can be only one provides but can be multiple requires.
        // logicalName = rule.at(JConsts::provides).get<Json>()[0].at(JConsts::logicalName).get<pstring>();

        modulescopedata_requirePaths.lock();
        auto [pos, ok] = target->moduleScopeData->requirePaths.try_emplace(logicalName, this);
        modulescopedata_requirePaths.unlock();

        if (!ok)
        {
            const auto &[key, val] = *pos;
            printErrorMessageColor(
                fmt::format(
                    "In Module-Scope:\n{}\nModule:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                    target->moduleScope->targetSubDir, logicalName, node->filePath, val->node->filePath),
                settings.pcSettings.toolErrorOutput);
            decrementTotalSMRuleFileCount(builder);
            throw std::exception();
        }
    }
    iterateRequiresJsonToInitializeNewHeaderUnits(builder);
}

bool pathContainsFile(const path &dir, path file)
{

    file.remove_filename();

    // If dir has more components than file, then file can't possibly
    // reside in dir.
    auto dir_len = std::distance(dir.begin(), dir.end());
    auto file_len = std::distance(file.begin(), file.end());
    if (dir_len > file_len)
    {
        return false;
    }

    // This stops checking when it reaches dir.end(), so it's OK if file
    // has more directory components afterward. They won't be checked.
    return std::equal(dir.begin(), dir.end(), file.begin());
}

void SMFile::initializeNewHeaderUnit(const Json &requireJson, Builder &builder)
{
    ModuleScopeData *moduleScopeData = target->moduleScopeData;
    pstring requireLogicalName = requireJson.at(JConsts::logicalName).get<pstring>();
    if (requireLogicalName == logicalName)
    {
        printErrorMessageColor(fmt::format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                           target->moduleScope->targetSubDir, node->filePath),
                               settings.pcSettings.toolErrorOutput);
        decrementTotalSMRuleFileCount(builder);
        throw std::exception();
    }

    Node *headerUnitNode = Node::getNodeFromPath(requireJson.at("source-path").get<pstring>(), true);

    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const InclNode *nodeDir = nullptr;

    // Iterating over all header-unit-directories of the module-scope to find out which header-unit
    // directory this header-unit comes from and which target that header-unit-directory belongs to
    // if any
    for (const auto &[inclNode, targetLocal] : moduleScopeData->huDirTarget)
    {
        const InclNode *dirNode = inclNode;
        if (pathContainsFile(dirNode->node->filePath, headerUnitNode->filePath))
        {
            if (huDirTarget)
            {
                printErrorMessageColor(
                    fmt::format(
                        "Module Header Unit\n{}\n belongs to two Target Header Unit Includes\n{}\n{}\nof Module "
                        "Scope\n{}\n",
                        headerUnitNode->filePath, nodeDir->node->filePath, dirNode->node->filePath,
                        target->moduleScope->getTargetPointer()),
                    settings.pcSettings.toolErrorOutput);
                decrementTotalSMRuleFileCount(builder);
                throw std::exception();
            }
            else
            {
                huDirTarget = targetLocal;
                nodeDir = inclNode;
            }
        }
    }

    if (!huDirTarget)
    {
        for (const auto &[inclNode, targetLocal] : moduleScopeData->huDirTarget)
        {
            const InclNode *dirNode = inclNode;
            if (pathContainsFile(dirNode->node->filePath, headerUnitNode->filePath))
            {
                if (huDirTarget)
                {
                    printErrorMessageColor(
                        fmt::format(
                            "Module Header Unit\n{}\n belongs to two Target Header Unit Includes\n{}\n{}\nof Module "
                            "Scope\n{}\n",
                            headerUnitNode->filePath, nodeDir->node->filePath, dirNode->node->filePath,
                            target->moduleScope->getTargetPointer()),
                        settings.pcSettings.toolErrorOutput);
                    decrementTotalSMRuleFileCount(builder);
                    throw std::exception();
                }
                else
                {
                    huDirTarget = targetLocal;
                    nodeDir = inclNode;
                }
            }
        }
        printErrorMessageColor(
            fmt::format(
                "Module Header Unit\n{}\n does not belongs to any Target Header Unit Includes of Module Scope\n{}\n",
                headerUnitNode->filePath, target->moduleScope->getTargetPointer()),
            settings.pcSettings.toolErrorOutput);
        decrementTotalSMRuleFileCount(builder);
        throw std::exception();
    }

    set<SMFile, CompareSourceNode>::const_iterator headerUnitIt;

    modulescopedata_headerUnits.lock();
    if (auto it = moduleScopeData->headerUnits.find(headerUnitNode); it == moduleScopeData->headerUnits.end())
    {
        headerUnitIt = moduleScopeData->headerUnits.emplace(huDirTarget, headerUnitNode).first;
        modulescopedata_headerUnits.unlock();
        {
            lock_guard<mutex> lk{huDirTarget->headerUnitsMutex};
            huDirTarget->headerUnits.emplace(&(*headerUnitIt));
        }

        auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

        if (nodeDir->ignoreHeaderDeps)
        {
            headerUnit.ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
        }

        /*        Json &headerUnitsJson = huDirTarget->targetBuildCache.at(JConsts::headerUnits);

                const auto &[pos, Ok] = headerUnitsJson.emplace(headerUnit.node->filePath, Json::object_t{});
                if (Ok)
                {
                    Json &moduleJson = pos.value();
                    moduleJson.emplace(JConsts::compileCommand, Json::string_t{});
                    moduleJson.emplace(JConsts::headerDependencies, Json::array_t{});
                    moduleJson.emplace(JConsts::smrules, Json::object_t{});
                }
                headerUnit.sourceJson = pos.operator->();*/

        headerUnit.type = SM_FILE_TYPE::HEADER_UNIT;
        {
            lock_guard<mutex> lk(totalSMRuleFileCountMutex);
            ++target->moduleScopeData->totalSMRuleFileCount;
        }

        builder.addNewBTargetInFinalBTargets(&headerUnit);
    }
    else
    {
        modulescopedata_headerUnits.unlock();
        headerUnitIt = it;
    }

    auto &headerUnit = const_cast<SMFile &>(*headerUnitIt);

    headerUnitsConsumptionMethods[&headerUnit].emplace(
        requireJson.at(JConsts::lookupMethod).get<pstring>() == "include-angle", requireLogicalName);
    getRealBTarget(0).addDependency(headerUnit);
}

void SMFile::iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder)
{
    /*    if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            Json &rules = sourceJson->at(JConsts::smrules).at(JConsts::rules)[0];
            if (auto it = rules.find(JConsts::requires_); it != rules.end())
            {
                for (const Json &requireJson : *it)
                {
                    initializeNewHeaderUnit(requireJson, builder);
                }
            }
        }
        else
        {
            // If following remains false then source is GlobalModuleFragment.
            bool hasLogicalNameRequireDependency = false;
            // If following is true then smFile is PartitionImplementation.
            bool hasPartitionExportDependency = false;
            Json &rules = sourceJson->at(JConsts::smrules).at(JConsts::rules)[0];
            if (auto it = rules.find(JConsts::requires_); it != rules.end())
            {
                for (const Json &requireJson : *it)
                {
                    pstring requireLogicalName = requireJson.at(JConsts::logicalName).get<pstring>();
                    if (requireLogicalName == logicalName)
                    {
                        printErrorMessageColor(fmt::format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                                           target->moduleScope->targetSubDir, node->filePath),
                                               settings.pcSettings.toolErrorOutput);
                        decrementTotalSMRuleFileCount(builder);
                        throw std::exception();
                    }
                    if (requireJson.contains(JConsts::lookupMethod))
                    {
                        initializeNewHeaderUnit(requireJson, builder);
                    }
                    else
                    {
                        hasLogicalNameRequireDependency = true;
                        if (requireLogicalName.contains(':'))
                        {
                            hasPartitionExportDependency = true;
                        }
                    }
                }
            }
            if (hasProvide)
            {
                type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
            }
            else
            {
                if (hasLogicalNameRequireDependency)
                {
                    type = hasPartitionExportDependency ? SM_FILE_TYPE::PARTITION_IMPLEMENTATION
                                                        : SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
                }
                else
                {
                    type = SM_FILE_TYPE::GLOBAL_MODULE_FRAGMENT;
                }
            }
        }*/
}

bool SMFile::generateSMFileInRoundOne()
{
    /*if (sourceJson->at(JConsts::compileCommand) != target->compileCommandWithTool)
    {
        readJsonFromSMRulesFile = true;
        // This is the only access in round 1. Maybe change to relaxed
        fileStatus.store(true, std::memory_order_release);
        return true;
    }

    bool needsUpdate = false;
    Node *objectFileNode = Node::getNodeFromPath(
        objectFileOutputFilePath, true, true);

    if (objectFileNode->doesNotExist)
    {
        needsUpdate = true;
    }
    else
    {
        if (node->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
        {
            needsUpdate = true;
        }
        else
        {
            for (const Json &str : sourceJson->at(JConsts::headerDependencies))
            {
                Node *headerNode = Node::getNodeFromPath(str, true, true);
                if (headerNode->doesNotExist)
                {
                    needsUpdate = true;
                    break;
                }

                if (headerNode->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
                {
                    needsUpdate = true;
                    break;
                }
            }
        }
    }

    if (needsUpdate)
    {
        // This is the only access in round 1. Maybe change to relaxed
        fileStatus.store(true, std::memory_order_release);

        readJsonFromSMRulesFile = true;

        // If smrules file exists, and is updated, then it won't be updated. This can happen when, because of
        // selectiveBuild, previous invocation of hbuild has updated target smrules file but didn't update the
        // .m.o file.

        Node *smRuleFileNode = Node::getNodeFromPath(
            target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + (".smrules"), true, true);

        if (smRuleFileNode->doesNotExist)
        {
            return true;
        }
        else
        {
            if (node->getLastUpdateTime() > smRuleFileNode->getLastUpdateTime())
            {
                return true;
            }
            else
            {
                for (const Json &str : sourceJson->at(JConsts::headerDependencies))
                {
                    Node *headerNode = Node::getNodeFromPath(str, true, true);
                    if (headerNode->doesNotExist)
                    {
                        return true;
                    }

                    if (headerNode->getLastUpdateTime() > smRuleFileNode->getLastUpdateTime())
                    {
                        return true;
                    }
                }
            }
        }
    }*/
    return false;
}

pstring SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".m.o",
                          pathPrint);
}

BTargetType SMFile::getBTargetType() const
{
    return BTargetType::SMFILE;
}

void SMFile::setFileStatusAndPopulateAllDependencies()
{
    path objectFilePath;

    auto emplaceInAll = [&](SMFile *smFile) {
        if (const auto &[pos, Ok] = allSMFileDependenciesRoundZero.emplace(smFile); Ok)
        {
            for (auto &[headerUnitSMFile, headerUnitConsumerSet] : smFile->headerUnitsConsumptionMethods)
            {
                for (const HeaderUnitConsumer &headerUnitConsumer : headerUnitConsumerSet)
                {
                    headerUnitsConsumptionMethods.emplace(headerUnitSMFile, set<HeaderUnitConsumer>{})
                        .first->second.emplace(headerUnitConsumer);
                }
            }

            if (!fileStatus.load(std::memory_order_acquire))
            {
                Node *objectFileNode = Node::getNodeFromPath(objectFilePath, true);

                pstring depFileName = (path(smFile->node->filePath).filename().*toPStr)();
                Node *depObjectFileNode = Node::getNodeFromPath(smFile->objectFileOutputFilePath, true, true);

                if (depObjectFileNode->getLastUpdateTime() > objectFileNode->getLastUpdateTime())
                {
                    fileStatus.store(true, std::memory_order_release);
                    return;
                }
            }
        }
    };

    for (auto &[dependency, ignore] : getRealBTarget(0).dependencies)
    {
        if (dependency->getBTargetType() == BTargetType::SMFILE)
        {
            auto *smFile = static_cast<SMFile *>(dependency);
            emplaceInAll(smFile);
            for (SMFile *smFileDep : smFile->allSMFileDependenciesRoundZero)
            {
                emplaceInAll(smFileDep);
            }
        }
    }
}

pstring SMFile::getFlag(const string &outputFilesWithoutExtension) const
{
    pstring str = "/ifcOutput" + addQuotes(outputFilesWithoutExtension + ".ifc") + " " + "/Fo" +
                  addQuotes(outputFilesWithoutExtension + ".m.o");
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED", settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return "/interface " + str;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        pstring angleStr = angle ? "angle " : "quote ";
        return "/exportHeader " + str;
    }
    else
    {
        str = "/Fo" + addQuotes(outputFilesWithoutExtension + ".m.o");
        if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            return "/internalPartition " + str;
        }
        else
        {
            return str;
        }
    }
}

pstring SMFile::getFlagPrint(const pstring &outputFilesWithoutExtension) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    bool infra = ccpSettings.infrastructureFlags;

    pstring str = infra ? "/ifcOutput" : "";
    if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
    {
        str += getReducedPath(outputFilesWithoutExtension + ".ifc", ccpSettings.ifcOutputFile) + " ";
    }
    str += infra ? "/Fo" : "";
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        str += getReducedPath(outputFilesWithoutExtension + ".m.o", ccpSettings.objectFile);
    }

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessageColor("Error! In getRequireFlag() type is NOT_ASSIGNED", settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return (infra ? "/interface " : "") + str;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        return (infra ? "/exportHeader " : "") + str;
    }
    else
    {
        str = infra ? "/Fo" : "" + getReducedPath(outputFilesWithoutExtension + ".m.o", ccpSettings.objectFile);
        if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            return (infra ? "/internalPartition " : "") + str;
        }
        else
        {
            return str;
        }
    }
}

pstring SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{
    pstring ifcFilePath =
        addQuotes(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".ifc");

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return "/reference " + logicalName + "=" + ifcFilePath;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        pstring str;
        for (const HeaderUnitConsumer &headerUnitConsumer :
             dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
        {
            pstring angleStr = headerUnitConsumer.angle ? "angle" : "quote";
            str += "/headerUnit:";
            str += angleStr + " ";
            str += headerUnitConsumer.logicalName + "=" + ifcFilePath + " ";
        }
        return str;
    }
    printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    throw std::exception();
}

pstring SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    pstring ifcFilePath = target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)() + ".ifc";
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&](const pstring &logicalName_) {
        return ccpSettings.onlyLogicalNameOfRequireIFC
                   ? logicalName_
                   : logicalName_ + "=" + getReducedPath(ifcFilePath, ccpSettings.requireIFCs);
    };
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        throw std::exception();
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            return "";
        }
        else
        {
            pstring str;
            if (ccpSettings.infrastructureFlags)
            {
                str += "/reference ";
            }
            return str + getRequireIFCPathOrLogicalName(logicalName) + " ";
        }
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            return "";
        }
        else
        {
            pstring str;
            for (const HeaderUnitConsumer &headerUnitConsumer :
                 dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
            {
                if (ccpSettings.infrastructureFlags)
                {
                    pstring angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                    str += "/headerUnit:" + angleStr + " ";
                }
                str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
            }
            return str;
        }
    }
    printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    throw std::exception();
}

pstring SMFile::getModuleCompileCommandPrintLastHalf()
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    pstring moduleCompileCommandPrintLastHalf;
    if (ccpSettings.requireIFCs.printLevel != PathPrintLevel::NO)
    {
        for (const SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            moduleCompileCommandPrintLastHalf += smFile->getRequireFlagPrint(*this);
        }
    }

    moduleCompileCommandPrintLastHalf += ccpSettings.infrastructureFlags ? target->getInfrastructureFlags(false) : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf +=
        getFlagPrint(target->buildCacheFilesDirPath + (path(node->filePath).filename().*toPStr)());
    return moduleCompileCommandPrintLastHalf;
}