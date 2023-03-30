
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
    std::filesystem::file_time_type, std::exception;

string getStatusString(const path &p)
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

bool CompareNode::operator()(const string &lhs, const Node &rhs) const
{
    return lhs < rhs.filePath;
}

bool CompareNode::operator()(const Node &lhs, const string &rhs) const
{
    return lhs.filePath < rhs;
}

Node::Node(const path &filePath_, bool isFile, bool mayNotExist)
{
    std::filesystem::file_type nodeType = directory_entry(filePath_).status().type();
    if (nodeType == (isFile ? file_type::regular : file_type::directory))
    {
        filePath = filePath_.generic_string();
    }
    else
    {
        if (!mayNotExist || nodeType != file_type::not_found)
        {
            printErrorMessage(format("{} is not a {} file. File Type is {}\n", filePath_.generic_string(),
                                     isFile ? "regular" : "directory", getStatusString(filePath_)));
            throw std::exception();
        }
        doesNotExist = true;
    }
}

std::filesystem::file_time_type Node::getLastUpdateTime() const
{
    if (!isUpdated)
    {
        const_cast<std::filesystem::file_time_type &>(lastUpdateTime) = last_write_time(path(filePath));
        const_cast<bool &>(isUpdated) = true;
    }
    return lastUpdateTime;
}

path Node::getFinalNodePathFromString(const string &str)
{
    path filePath{str};
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = filePath.lexically_normal();

    if constexpr (os == OS::NT)
    {
        // Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        // actual paths. In Windows paths could be case-insensitive. Just another wrinkle hahaha.
        string lowerCase = filePath.string();
        for (char &c : lowerCase)
        {
            // Warning: assuming paths to be ASCII
            c = tolower(c);
        }
        filePath = lowerCase;
    }
    return filePath;
}

static std::mutex nodeInsertMutex;
const Node *Node::getNodeFromString(const string &str, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromString(str);
    if (auto it = allFiles.find(filePath.string()); it != allFiles.end())
    {
        return it.operator->();
    }

    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.
    std::lock_guard<std::mutex> lk(nodeInsertMutex);
    Node node(filePath, isFile, mayNotExist);
    return allFiles.emplace(std::move(node)).first.operator->();
}

bool operator<(const Node &lhs, const Node &rhs)
{
    return lhs.filePath < rhs.filePath;
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
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

string SourceNode::getObjectFileOutputFilePath()
{
    return target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".o";
}

string SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint)
{
    return getReducedPath(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".o", pathPrint);
}

void SourceNode::updateBTarget(unsigned short round, Builder &)
{
    if (!round && selectiveBuild)
    {
        RealBTarget &realBTarget = getRealBTarget(round);
        PostCompile postCompile = target->updateSourceNodeBTarget(*this);
        postCompile.parseHeaderDeps(*this);
        realBTarget.exitStatus = postCompile.exitStatus;
        realBTarget.fileStatus = FileStatus::UPDATED;
        // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
        // cached compile-command would be different
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            compileCommandJson = target->compiler.bTPath.generic_string() + " " + target->compileCommand;
        }
        std::lock_guard<std::mutex> lk(printMutex);
        postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
        fflush(stdout);
    }
}

void SourceNode::setSourceNodeFileStatus(const string &ex, RealBTarget &realBTarget)
{
    path objectFilePath = path(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ex);

    if (!presentInCache)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }

    if (compileCommandJson != target->compiler.bTPath.generic_string() + " " + target->compileCommand)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }

    if (!std::filesystem::exists(objectFilePath))
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }

    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    if (node->getLastUpdateTime() > objectFileLastEditTime)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }

    for (const Json &str : headerFilesJson)
    {
        Node *headerNode = const_cast<Node *>(Node::getNodeFromString(str, true, true));
        if (node->doesNotExist)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }

        if (headerNode->getLastUpdateTime() > objectFileLastEditTime)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }
    }
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    j[JConsts::srcFile] = sourceNode.node->filePath;
    j[JConsts::compileCommand] = sourceNode.compileCommandJson;
    j[JConsts::headerDependencies] = sourceNode.headerDependencies;
}

void to_json(Json &j, const SourceNode *smFile)
{
    j = *smFile;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return lhs.node < rhs.node;
}

HeaderUnitConsumer::HeaderUnitConsumer(bool angle_, string logicalName_)
    : angle{angle_}, logicalName{std::move(logicalName_)}
{
}

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
}

static std::mutex smFilesInternalMutex;
void SMFile::updateBTarget(unsigned short round, Builder &builder)
{
    // Danger Following is executed concurrently
    RealBTarget &realBTarget = getRealBTarget(round);
    if (round == 1)
    {
        realBTarget.exitStatus = EXIT_SUCCESS;
        if (generateSMFileInRoundOne)
        {
            // TODO
            //  Expose setting for printOnlyOnError
            PostCompile postCompile = target->GenerateSMRulesFile(*this, true);
            {
                std::lock_guard<std::mutex> lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                fflush(stdout);
            }
            postCompile.parseHeaderDeps(*this);
            realBTarget.exitStatus = postCompile.exitStatus;
        }
        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            {
                // Maybe fine-grain this mutex by using multiple mutexes in following functions. And first use
                // set::contain and then set::emplace. Because set::contains is thread-safe while set::emplace isn't
                std::lock_guard<std::mutex> lk(smFilesInternalMutex);
                saveRequiresJsonAndInitializeHeaderUnits(builder);
                assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
            }
            target->getRealBTarget(0).addDependency(*this);
            smrulesFileParsed = true;
            // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed because
            // cached compile-command would be different
            compileCommandJson = target->compiler.bTPath.generic_string() + " " + target->compileCommand;
        }
    }
    else if (!round && selectiveBuild)
    {
        PostCompile postCompile = target->CompileSMFile(*this);
        realBTarget.exitStatus = postCompile.exitStatus;

        std::lock_guard<std::mutex> lk(printMutex);
        postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
        fflush(stdout);
    }
    realBTarget.fileStatus = FileStatus::UPDATED;
}

void SMFile::saveRequiresJsonAndInitializeHeaderUnits(Builder &builder)
{
    string smFilePath = target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".smrules";
    Json smFileJson;
    ifstream(smFilePath) >> smFileJson;
    Json rule = smFileJson.at("rules").get<Json>()[0];
    if (rule.contains("provides"))
    {
        hasProvide = true;
        // There can be only one provides but can be multiple requires.
        logicalName = rule.at("provides").get<Json>()[0].at("logical-name").get<string>();
        if (auto [pos, ok] = target->moduleScopeData->requirePaths.try_emplace(logicalName, this); !ok)
        {
            const auto &[key, val] = *pos;
            // TODO
            // Mention the module scope too.
            printErrorMessageColor(
                format("In Module-Scope:\n{}\nModule:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                       target->moduleScope->getSubDirForTarget(), logicalName, node->filePath, val->node->filePath),
                settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }
    }
    requiresJson = std::move(rule.at("requires"));
    iterateRequiresJsonToInitializeNewHeaderUnits(builder);
}

void SMFile::initializeNewHeaderUnit(const Json &requireJson, Builder &builder)
{
    string requireLogicalName = requireJson.at("logical-name").get<string>();
    if (requireLogicalName == logicalName)
    {
        printErrorMessageColor(format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                      target->moduleScope->getSubDirForTarget(), node->filePath),
                               settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }

    string headerUnitPath = path(requireJson.at("source-path").get<string>()).lexically_normal().generic_string();

    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const Node *nodeDir;

    // Iterating over all header-unit-directories of the module-scope to find out which header-unit
    // directory this header-unit comes from and which target that header-unit-directory belongs to
    // if any
    for (auto &dir : target->moduleScopeData->huDirTarget)
    {
        const Node *dirNode = dir.first;
        path result = path(headerUnitPath).lexically_relative(dirNode->filePath).generic_string();
        if (!result.empty() && !result.generic_string().starts_with(".."))
        {
            if (huDirTarget)
            {
                printErrorMessageColor(
                    format("Module Header Unit\n{}\n belongs to two Target Header Unit Includes\n{}\n{}\nof Module "
                           "Scope\n{}\n",
                           headerUnitPath, nodeDir->filePath, dirNode->filePath,
                           target->moduleScope->getTargetPointer()),
                    settings.pcSettings.toolErrorOutput);
                throw std::exception();
            }
            else
            {
                huDirTarget = dir.second;
                nodeDir = dir.first;
            }
        }
    }

    if (!huDirTarget)
    {
        printErrorMessageColor(
            format("Module Header Unit\n{}\n does not belongs to any Target Header Unit Includes of Module Scope\n{}\n",
                   headerUnitPath, target->moduleScope->getTargetPointer()),
            settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }

    SMFile &headerUnit =
        huDirTarget->addNodeInHeaderUnits(const_cast<Node *>(Node::getNodeFromString(headerUnitPath, true)));
    if (!headerUnit.presentInSource)
    {
        headerUnit.presentInSource = true;
        if (nodeDir->ignoreHeaderDeps)
        {
            headerUnit.ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
        }
        headerUnit.type = SM_FILE_TYPE::HEADER_UNIT;
        RealBTarget &realBTarget = headerUnit.getRealBTarget(1);
        headerUnit.setSourceNodeFileStatus(".smrules", realBTarget);

        if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
        {
            headerUnit.generateSMFileInRoundOne = true;
        }
        builder.addNewBTargetInFinalBTargets(&headerUnit);
    }
    headerUnitsConsumptionMethods[&headerUnit].emplace(requireJson.at("lookup-method").get<string>() == "include-angle",
                                                       requireLogicalName);
    getRealBTarget(0).addDependency(headerUnit);
}

void SMFile::iterateRequiresJsonToInitializeNewHeaderUnits(Builder &builder)
{
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        for (const Json &requireJson : requiresJson)
        {
            initializeNewHeaderUnit(requireJson, builder);
        }
    }
    else
    {
        // If following remains false then source is GlobalModuleFragment.
        bool hasLogicalNameRequireDependency = false;
        // If following is true then smFile is PartitionImplementation.
        bool hasPartitionExportDependency = false;
        for (const Json &requireJson : requiresJson)
        {
            string requireLogicalName = requireJson.at("logical-name").get<string>();
            if (requireLogicalName == logicalName)
            {
                printErrorMessageColor(format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                              target->moduleScope->getSubDirForTarget(), node->filePath),
                                       settings.pcSettings.toolErrorOutput);
                throw std::exception();
            }
            if (requireJson.contains("lookup-method"))
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
    }
}

void SMFile::setSMFileStatusRoundZero()
{
    RealBTarget &realBTarget = getRealBTarget(0);
    if (realBTarget.fileStatus != FileStatus::NEEDS_UPDATE)
    {
        string fileName = path(node->filePath).filename().string();
        path objectFilePath = path(target->buildCacheFilesDirPath + fileName + ".m.o");
        Node *smRuleNode =
            const_cast<Node *>(Node::getNodeFromString(target->buildCacheFilesDirPath + fileName + ".smrules", true));
#ifndef NDEBUG
        if (!exists(path(smRuleNode->filePath)))
        {
            printErrorMessage(
                format("Warning. Following smrules not found while checking the object-file-status which must had been "
                       "generated in round 1.\n{}\n",
                       smRuleNode->filePath));
            throw std::exception();
        }
#endif
        if (!exists(objectFilePath))
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }
        file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
        if (smRuleNode->getLastUpdateTime() > objectFileLastEditTime)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }
        file_time_type ifcFileLastEditTime;
        bool hasIfcFile = false;
        if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION && type != SM_FILE_TYPE::PARTITION_IMPLEMENTATION &&
            type != SM_FILE_TYPE::GLOBAL_MODULE_FRAGMENT)
        {
            hasIfcFile = true;
            path ifcFilePath = path(target->buildCacheFilesDirPath + fileName + ".ifc");
            if (!exists(ifcFilePath))
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                return;
            }
            ifcFileLastEditTime = last_write_time(ifcFilePath);
            if (smRuleNode->getLastUpdateTime() > ifcFileLastEditTime)
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                return;
            }
        }

        for (SMFile *dep : allSMFileDependenciesRoundZero)
        {
            string depFileName = path(dep->node->filePath).filename().string();
            path depIfcFilePath = path(dep->target->buildCacheFilesDirPath + depFileName + ".ifc");
            file_time_type depIfcFileLastEditTime = last_write_time(depIfcFilePath);
            if (depIfcFileLastEditTime > objectFileLastEditTime)
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                return;
            }
            if (hasIfcFile)
            {
                if (depIfcFileLastEditTime > ifcFileLastEditTime)
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                    return;
                }
            }
        }
    }
}

string SMFile::getObjectFileOutputFilePath()
{
    return target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".m.o";
}

string SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint)
{
    return getReducedPath(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".m.o",
                          pathPrint);
}

void SMFile::duringSort(Builder &, unsigned short round, unsigned int)
{
    if (round)
    {
        return;
    }
    for (BTarget *dependency : getRealBTarget(0).dependencies)
    {
        if (auto *smFile = dynamic_cast<SMFile *>(dependency); smFile)
        {
            allSMFileDependenciesRoundZero.emplace(smFile);
            for (BTarget *dep : smFile->allSMFileDependenciesRoundZero)
            {
                if (auto *smFile1 = dynamic_cast<SMFile *>(dep); smFile1)
                {
                    allSMFileDependenciesRoundZero.emplace(smFile1);
                }
            }
        }
    }
    setSMFileStatusRoundZero();
    for (SMFile *smFile : allSMFileDependenciesRoundZero)
    {
        for (auto &[headerUnitSMFile, headerUnitConsumerSet] : smFile->headerUnitsConsumptionMethods)
        {
            for (const HeaderUnitConsumer &headerUnitConsumer : headerUnitConsumerSet)
            {
                headerUnitsConsumptionMethods.emplace(headerUnitSMFile, set<HeaderUnitConsumer>{})
                    .first->second.emplace(headerUnitConsumer);
            }
        }
    }
}

string SMFile::getFlag(const string &outputFilesWithoutExtension) const
{
    string str = "/ifcOutput" + addQuotes(outputFilesWithoutExtension + ".ifc") + " " + "/Fo" +
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
        string angleStr = angle ? "angle " : "quote ";
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

string SMFile::getFlagPrint(const string &outputFilesWithoutExtension) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    bool infra = ccpSettings.infrastructureFlags;

    string str = infra ? "/ifcOutput" : "";
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

string SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{
    string ifcFilePath = addQuotes(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc");

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
        string str;
        for (const HeaderUnitConsumer &headerUnitConsumer :
             dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
        {
            string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
            str += "/headerUnit:";
            str += angleStr + " ";
            str += headerUnitConsumer.logicalName + "=" + ifcFilePath + " ";
        }
        return str;
    }
    printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    throw std::exception();
}

string SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    string ifcFilePath = target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc";
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&](const string &logicalName_) {
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
        return ccpSettings.infrastructureFlags ? "/reference " : "" + getRequireIFCPathOrLogicalName(logicalName) + " ";
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionMethods.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        string str;
        for (const HeaderUnitConsumer &headerUnitConsumer :
             dependentSMFile.headerUnitsConsumptionMethods.find(this)->second)
        {
            if (!ccpSettings.infrastructureFlags)
            {
                string angleStr = headerUnitConsumer.angle ? "angle" : "quote";
                str += "/headerUnit:" + angleStr + " ";
            }
            str += getRequireIFCPathOrLogicalName(headerUnitConsumer.logicalName) + " ";
        }
        return str;
    }
    printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    throw std::exception();
}

string SMFile::getModuleCompileCommandPrintLastHalf()
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string moduleCompileCommandPrintLastHalf;
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
        getFlagPrint(target->buildCacheFilesDirPath + path(node->filePath).filename().string());
    return moduleCompileCommandPrintLastHalf;
}
