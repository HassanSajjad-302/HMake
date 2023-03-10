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

using std::filesystem::directory_entry, std::filesystem::file_type, std::tie, std::ifstream,
    std::filesystem::file_time_type;

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
            print(stderr, "{} is not a {} file. File Type is {}\n", filePath_.generic_string(),
                  isFile ? "regular" : "directory", getStatusString(filePath_));
            exit(EXIT_FAILURE);
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

static std::mutex nodeInsertMutex;
const Node *Node::getNodeFromString(const string &str, bool isFile, bool mayNotExist)
{
    path filePath{str};
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = filePath.lexically_normal();

    if (auto it = allFiles.find(filePath.string()); it != allFiles.end())
    {
        return it.operator->();
    }

    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.
    std::lock_guard<std::mutex> lk(nodeInsertMutex);
    return allFiles.emplace(filePath, isFile, mayNotExist).first.operator->();
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
        if (realBTarget.dependenciesExitStatus == EXIT_SUCCESS)
        {
            PostCompile postCompile = target->updateSourceNodeBTarget(*this);
            postCompile.parseHeaderDeps(*this);
            realBTarget.exitStatus = postCompile.exitStatus;
            realBTarget.fileStatus = FileStatus::UPDATED;

            std::lock_guard<std::mutex> lk(printMutex);
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
            fflush(stdout);
        }
        else
        {
            realBTarget.exitStatus = EXIT_FAILURE;
        }
    }
}

void SourceNode::setSourceNodeFileStatus(const string &ex, RealBTarget &realBTarget)
{
    path objectFilePath = path(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ex);

    if (!std::filesystem::exists(objectFilePath))
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }
    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    if (node->getLastUpdateTime() > objectFileLastEditTime)
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
    }
    else
    {

        for (const Json &str : headerFilesJson)
        {
            Node *headerNode = const_cast<Node *>(Node::getNodeFromString(str, true, true));
            if (node->doesNotExist)
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                break;
            }
            else
            {
                if (headerNode->getLastUpdateTime() > objectFileLastEditTime)
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                    break;
                }
            }
        }
    }
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    j[JConsts::srcFile] = sourceNode.node->filePath;
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
        if (generateSMFileInRoundOne)
        {
            // TODO
            //  Expose setting for printOnlyOnError
            PostCompile postCompile = target->GenerateSMRulesFile(*this, true);
            realBTarget.exitStatus = postCompile.exitStatus;
            {
                std::lock_guard<std::mutex> lk(printMutex);
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                fflush(stdout);
            }
            if (realBTarget.exitStatus == EXIT_SUCCESS)
            {
                postCompile.parseHeaderDeps(*this);
            }
            else
            {
                // In-case of error in .o file generation, build system continues, so other .o files could be compiled
                // and those aren't recompiled in next-run. But side effects of allowing build-system to continue from
                // here are too complex to evaluate, so exiting.

                // TODO
                // Maybe have exitBeforeCompletion in Builder set to false and true it here. If true next round is not
                // executed and a new function is called for all BTargets. CppSourceTarget in that function removes
                // erroneous smrule files and saves the cache. so next run only
                exit(EXIT_FAILURE);
            }
        }
        {
            // Maybe fine-grain this mutex by using multiple mutexes in following functions. And first use set::contain
            // and then set::emplace. Because set::contains is thread-safe while set::emplace isn't
            std::lock_guard<std::mutex> lk(smFilesInternalMutex);
            saveRequiresJsonAndInitializeHeaderUnits(builder);
            assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");
        }
        setSMFileStatusRoundZero();
    }
    else if (!round && selectiveBuild)
    {
        if (realBTarget.dependenciesExitStatus == EXIT_SUCCESS)
        {
            PostCompile postCompile = target->CompileSMFile(*this);
            realBTarget.exitStatus = postCompile.exitStatus;

            std::lock_guard<std::mutex> lk(printMutex);
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
            fflush(stdout);
        }
        else
        {
            realBTarget.exitStatus = EXIT_FAILURE;
        }
    }
    realBTarget.fileStatus = FileStatus::UPDATED;
}

string SMFile::getObjectFileOutputFilePath()
{
    return standardHeaderUnit ? target->getSHUSPath() + path(node->filePath).filename().string() + ".o"
                              : SourceNode::getObjectFileOutputFilePath();
}

string SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint)
{
    return standardHeaderUnit
               ? getReducedPath(target->getSHUSPath() + path(node->filePath).filename().string() + ".o", pathPrint)
               : SourceNode::getObjectFileOutputFilePathPrint(pathPrint);
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
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "In Module-Scope:\n{}\nModule:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                  target->moduleScope->getSubDirForTarget(), logicalName, node->filePath, val->node->filePath);
            exit(EXIT_FAILURE);
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
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", node->filePath);
        exit(EXIT_FAILURE);
    }

    string headerUnitPath = path(requireJson.at("source-path").get<string>()).lexically_normal().generic_string();

    bool isStandardHeaderUnit = false;
    bool isApplicationHeaderUnit = false;

    // The target from which this header-unit comes from or this target's moduleScope if header-unit is
    // a standard header-unit
    CppSourceTarget *ahuDirTarget = nullptr;
    if (isSubDirPathStandard(headerUnitPath, target->standardIncludes))
    {
        isStandardHeaderUnit = true;
        ahuDirTarget = target->moduleScope;
    }
    if (!isStandardHeaderUnit)
    {
        // Iterating over all header-unit-directories of the module-scope to find out which header-unit
        // directory this header-unit comes from and which target that header-unit-directory belongs to
        // if any
        for (auto &dir : target->moduleScopeData->appHUDirTarget)
        {
            const Node *dirNode = dir.first;
            path result = path(headerUnitPath).lexically_relative(dirNode->filePath).generic_string();
            if (!result.empty() && !result.generic_string().starts_with(".."))
            {
                isApplicationHeaderUnit = true;
                ahuDirTarget = dir.second;
            }
        }
    }
    if (!isStandardHeaderUnit && !isApplicationHeaderUnit)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Module Header Unit\n{}\n is neither a Standard Header Unit nor belongs to any Target "
              "Header Unit Includes of Module Scope\n{}\n",
              headerUnitPath, target->moduleScope->getTargetPointer());
        exit(EXIT_FAILURE);
    }

    SMFile &headerUnit =
        target->addNodeInHeaderUnits(const_cast<Node *>(Node::getNodeFromString(headerUnitPath, true)));
    if (!headerUnit.presentInSource)
    {
        headerUnit.presentInSource = true;
        headerUnit.standardHeaderUnit = true;
        headerUnit.type = SM_FILE_TYPE::HEADER_UNIT;
        headerUnitsConsumptionMethods[&headerUnit].emplace(
            requireJson.at("lookup-method").get<string>() == "include-angle", requireLogicalName);
        ahuDirTarget->applicationHeaderUnits.emplace(&headerUnit);
        RealBTarget &realBTarget = headerUnit.getRealBTarget(1);
        if (!headerUnit.presentInCache)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        }
        else
        {
            headerUnit.setSourceNodeFileStatus(".smrules", realBTarget);
        }
        if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
        {
            headerUnit.generateSMFileInRoundOne = true;
            // To save the updated .cache file
            target->getRealBTarget(0).fileStatus = FileStatus::NEEDS_UPDATE;
        }
        getRealBTarget(0).addDependency(headerUnit);
        builder.addNewBTargetInFinalBTargets(&headerUnit);
    }
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
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                      "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", node->filePath);
                exit(EXIT_FAILURE);
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
        path objectFilePath;
        if (type == SM_FILE_TYPE::HEADER_UNIT && standardHeaderUnit)
        {
            objectFilePath = path(target->moduleScope->getSubDirForTarget() + "shus/" +
                                  path(node->filePath).filename().string() + ".o");
            if (!exists(objectFilePath))
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            }
            return;
        }

        string fileName = path(node->filePath).filename().string();
        objectFilePath = path(target->buildCacheFilesDirPath + fileName + ".o");
        Node *smRuleNode =
            const_cast<Node *>(Node::getNodeFromString(target->buildCacheFilesDirPath + fileName + ".smrules", true));
        if (!exists(path(smRuleNode->filePath)))
        {
            print(stderr,
                  "Warning. Following smrules not found while checking the object-file-status which must had been "
                  "generated in round 1.\n{}\n",
                  smRuleNode->filePath);
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }
        if (!exists(objectFilePath))
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            return;
        }
        file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
        if (smRuleNode->getLastUpdateTime() > objectFileLastEditTime)
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
        }
    }
}

bool SMFile::isSubDirPathStandard(const path &headerUnitPath, set<const Node *> &standardIncludes)
{
    string headerUnitPathSMallCase = headerUnitPath.generic_string();
    for (auto &c : headerUnitPathSMallCase)
    {
        c = (char)::tolower(c);
    }
    for (const Node *stdInclude : standardIncludes)
    {
        string directoryPathSmallCase = stdInclude->filePath;
        for (auto &c : directoryPathSmallCase)
        {
            c = (char)::tolower(c);
        }
        path result = path(headerUnitPathSMallCase).lexically_relative(directoryPathSmallCase).generic_string();
        if (!result.empty() && !result.generic_string().starts_with(".."))
        {
            return true;
        }
    }
    return false;
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
                 addQuotes(outputFilesWithoutExtension + ".o");
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
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
        str = "/Fo" + addQuotes(outputFilesWithoutExtension + ".o");
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
        str += getReducedPath(outputFilesWithoutExtension + ".o", ccpSettings.objectFile);
    }

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
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
        str = infra ? "/Fo" : "" + getReducedPath(outputFilesWithoutExtension + ".o", ccpSettings.objectFile);
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
    string ifcFilePath;
    if (standardHeaderUnit)
    {
        ifcFilePath = addQuotes(target->getSHUSPath() + path(node->filePath).filename().string() + ".ifc");
    }
    else
    {
        ifcFilePath = addQuotes(target->buildCacheFilesDirPath + path(node->filePath).filename().string() + ".ifc");
    }
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        print(stderr, "HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
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
    print(stderr, "HMake Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
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
        print(stderr, "HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
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
    print(stderr, "HMake Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
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