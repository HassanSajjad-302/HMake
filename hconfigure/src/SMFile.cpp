
#ifdef USE_HEADER_UNITS
import "SMFile.hpp";

import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "IPCManagerBS.hpp";
import "JConsts.hpp";
import "RunCommand.hpp";
import "Settings.hpp";
import "StaticVector.hpp";
import "CacheWriteManager.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <mutex>;
import <utility>;
#else
#include "SMFile.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CacheWriteManager.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "IPCManagerBS.hpp"
#include "JConsts.hpp"
#include "RunCommand.hpp"
#include "Settings.hpp"
#include "StaticVector.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <mutex>
#include <utility>
#endif

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

SourceNode::SourceNode(CppSourceTarget *target_, Node *node_) : ObjectFile(true, false), target(target_), node{node_}
{
}

SourceNode::SourceNode(CppSourceTarget *target_, const Node *node_, const bool add0, const bool add1)
    : ObjectFile(add0, add1), target(target_), node{node_}
{
}

string SourceNode::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->myBuildDir->filePath + slashc + node->getFileName() + ".o", pathPrint);
}

string SourceNode::getPrintName() const
{
    return node->filePath;
}

void SourceNode::initializeBuildCache(const uint32_t index)
{
    indexInBuildCache = index;
    buildCache = target->cppBuildCache.srcFiles[index];
    if (buildCache.compileCommandWithTool.hash != target->compileCommandWithTool.getHash())
    {
        fileStatus = true;
    }
    else
    {
        const_cast<bool &>(node->toBeChecked) = true;
        objectNode->toBeChecked = true;
        for (Node *headerFile : buildCache.headerFiles)
        {
            headerFile->toBeChecked = true;
        }
    }
}

void SourceNode::completeCompilation()
{
    const Compiler &compiler = target->configuration->compilerFeatures.compiler;
    string compileCommand = "\"" + compiler.bTPath.generic_string() + "\" " + target->compileCommand;
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        compileCommand += "-c /nologo /showIncludes \"" + node->filePath + "\" /Fo\"" + objectNode->filePath + "\"";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        compileCommand += "-c -MMD \"" + node->filePath + "\" -o \"" + objectNode->filePath + "\"";
    }

    const RunCommand r(compileCommand);
    auto [output, exitStatus] = r.endProcess();
    realBTargets[0].exitStatus = exitStatus;
    // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed
    // because cached compile-command would be different
    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        buildCache.compileCommandWithTool.hash = target->compileCommandWithTool.getHash();
        parseHeaderDeps(output);
    }

    const string printCommand =
        target->getSourceCompileCommandPrintFirstHalf() + target->getCompileCommandPrintSecondPart(*this);
    CacheWriteManager::addNewEntry(exitStatus, target, this, settings.pcSettings.compileCommandColor, printCommand,
                                   output);
}

void SourceNode::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (!round && selectiveBuild)
    {
        setSourceNodeFileStatus();
        if (fileStatus)
        {
            assignFileStatusToDependents(0);
            completeCompilation();
        }
    }
}

bool SourceNode::ignoreHeaderFile(const string_view child) const
{
    //  Premature Optimization Hahacd
    // TODO:
    //  Add a key in hconfigure that informs hbuild that the library isn't to be
    //  updated, so includes from the dirs coming from it aren't mentioned
    //  in targetCache and neither are those libraries checked for an edit for
    //  faster startup times.

    // If a file is in environment includes, it is not marked as dependency as an
    // optimization. If a file is in subdir of environment include, it is
    // still marked as dependency. It is not checked if any of environment
    // includes is related(equivalent, subdir) with any of normal includes
    // or vice versa.

    // std::path::equivalent is not used as it is slow
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

    if (ignoreHeaderDeps && settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
    {
        // TODO
        //  Merge this if in the following else.
        vector<string> outputLines = split(output, "\n");
        for (auto iter = outputLines.begin(); iter != outputLines.end();)
        {
            if (iter->contains(includeFileNote))
            {
                iter = outputLines.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
        return;
    }
    string treatedOutput;

    uint64_t startPos = 0;
    uint64_t lineEnd = output.find('\n');
    if (lineEnd == string::npos)
    {
        return;
    }

    string_view line(output.begin() + startPos, output.begin() + lineEnd + 1);

    if (!settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
    {
        treatedOutput.append(line);
    }

    startPos = lineEnd + 1;
    if (output.size() == startPos)
    {
        output = std::move(treatedOutput);
        return;
    }
    if (lineEnd > output.size() - 5)
    {
        bool breakpoint = true;
    }
    lineEnd = output.find('\n', startPos);

    buildCache.headerFiles.clear();
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
                if (!ignoreHeaderFile(headerView))
                {
                    lowerCasePStringOnWindows(const_cast<char *>(headerView.data()), headerView.size());

                    if (Node *headerNode = Node::getHalfNode(headerView);
                        !std::ranges::contains(buildCache.headerFiles, headerNode))
                    {
                        buildCache.headerFiles.emplace_back(headerNode);
                    }
                }
            }
            else
            {
                printErrorMessage(FORMAT("Empty Header Include {}\n", line));
            }

            if (!settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
            {
                treatedOutput.append(line);
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
    if (ignoreHeaderDeps)
    {
        return;
    }
    const string headerFileContents = fileToPString(target->myBuildDir->filePath + slashc + node->getFileName() + ".d");
    vector<string> headerDeps = split(headerFileContents, "\n");

    // The First 2 lines are skipped as these are .o and .cpp file.
    // If the file is preprocessed, it does not generate the extra line
    const auto endIt = headerDeps.end() - 1;

    if (headerDeps.size() > 2)
    {
        buildCache.headerFiles.clear();
        for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
        {
            const size_t pos = iter->find_first_not_of(" ");
            auto it = iter->begin() + pos;
            if (const string_view headerView{&*it, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos};
                !ignoreHeaderFile(headerView))
            {
                buildCache.headerFiles.emplace_back(Node::getHalfNode(headerView));
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
    if (fileStatus)
    {
        return;
    }

    if (objectNode->fileType == file_type::not_found || node->lastWriteTime > objectNode->lastWriteTime)
    {
        fileStatus = true;
        return;
    }

    for (const Node *headerNode : buildCache.headerFiles)
    {
        if (headerNode->fileType == file_type::not_found || headerNode->lastWriteTime > objectNode->lastWriteTime)
        {
            fileStatus = true;
            return;
        }
    }
}

void SourceNode::updateBuildCache()
{
    target->cppBuildCache.srcFiles[indexInBuildCache] = std::move(buildCache);
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

SMFile::SMFile(CppSourceTarget *target_, Node *node_) : SourceNode(target_, node_)
{
}

SMFile::SMFile(CppSourceTarget *target_, const Node *node_, string logicalName_)
    : SourceNode(target_, node_, false, false), logicalName(std::move(logicalName_)), type(SM_FILE_TYPE::HEADER_UNIT)
{
}

void SMFile::initializeBuildCache(uint32_t index)
{
    SourceNode::initializeBuildCache(index);
    smRulesCache = target->cppBuildCache.modFiles[index].smRules;
    compileCommandWithToolCache = target->cppBuildCache.modFiles[index].compileCommandWithTool;
}

void SMFile::setLogicalNameAndAddToRequirePath()
{
    if (!smRulesCache.exportName.empty())
    {
        logicalName = smRulesCache.exportName;

        using Map = decltype(requirePaths2);

        const RequireNameTargetId req(this->target->id, logicalName);
        if (const SMFile *val = nullptr; requirePaths2.lazy_emplace_l(
                req, [&](const Map::value_type &val_) { val = const_cast<SMFile *>(val_.second); },
                [&](const Map::constructor &constructor) { constructor(req, this); }))
        {
        }
        else
        {
            printErrorMessage(
                FORMAT("In target:\n{}\nModule name:\n {}\n Is Being Provided By 2 different files:\n1){}\n2){}\n",
                       target->getPrintName(), logicalName, node->filePath, val->node->filePath));
        }
    }
}

void SMFile::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (!round)
    {
        RealBTarget &realBTarget = realBTargets[round];

        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            objectNode = Node::getNodeFromNormalizedString(
                target->myBuildDir->filePath + slashc + getOutputFileName() + ".ifc", true, true);
        }
        else
        {
            objectNode = Node::getNodeFromNormalizedString(
                target->myBuildDir->filePath + slashc + getOutputFileName() + ".m.o", true, true);
        }

        if (buildCache.compileCommandWithTool.hash != target->compileCommandWithTool.getHash())
        {
            isObjectFileOutdated = true;
            isObjectFileOutdatedCallCompleted = true;
            return;
        }

        if (node->fileType == file_type::not_found || objectNode->fileType == file_type::not_found ||
            node->lastWriteTime > objectNode->lastWriteTime)
        {
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
        }

        if (!isObjectFileOutdated)
        {
            if (type == SM_FILE_TYPE::HEADER_UNIT)
            {
                checkObjectFileOutdatedHeaderUnits();
            }
            else
            {
                checkObjectFileOutdatedModules();
            }
        }

        if (isObjectFileOutdated)
        {
            fileStatus = true;
        }

        if (fileStatus)
        {
            IPCManagerBS *ipcManager = nullptr;
            if (auto r = N2978::makeIPCManagerBS(objectNode->filePath); r)
            {
                *ipcManager = *r;
            }
            else
            {
                printErrorMessage(FORMAT("Could not make the build-system manager {}\n", objectNode->filePath));
            }

            // todo
            // compile command construction and running

            char buffer[320];
            N2978::CTB type;
            if (const auto &r = ipcManager->receiveMessage(buffer, type); r)
            {
                if (type == N2978::CTB::MODULE)
                {
                    N2978::CTBModule &mod = reinterpret_cast<N2978::CTBModule &>(buffer);
                }
                else if (type == N2978::CTB::NON_MODULE)
                {
                    N2978::CTBNonModule &nonMod = reinterpret_cast<N2978::CTBNonModule &>(buffer);
                }
                else if (type == N2978::CTB::NON_MODULE)
                {
                }
            }

            assignFileStatusToDependents(0);
            const string compileCommand = target->compileCommand + getCompileCommand();
            const RunCommand r(compileCommand);

            auto [output, exitStatus] = r.endProcess();
            realBTargets[0].exitStatus = exitStatus;
            // Compile-Command is only updated on succeeding i.e. in case of failure it will be re-executed
            // because cached compile-command would be different
            if (realBTargets[0].exitStatus == EXIT_SUCCESS)
            {
                buildCache.compileCommandWithTool.hash = target->compileCommandWithTool.getHash();
                parseHeaderDeps(output);
            }

            const string printCommand =
                target->getSourceCompileCommandPrintFirstHalf() + target->getCompileCommandPrintSecondPart(*this);
            CacheWriteManager::addNewEntry(exitStatus, target, this, settings.pcSettings.compileCommandColor,
                                           printCommand, output);
        }

        string smrulesFileOutputClang;

        if (realBTarget.exitStatus == EXIT_SUCCESS)
        {
            includeNames.reserve(4 * 1024);
            includeNames.clear();

            if (type != SM_FILE_TYPE::HEADER_UNIT)
            {
                setSMFileType();
                setLogicalNameAndAddToRequirePath();

                for (auto &[_, logicalName2] : smRulesCache.moduleArray)
                {
                    if (logicalName == logicalName2)
                    {
                        printErrorMessage(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                                 target->getPrintName(), node->filePath));
                    }
                }
            }

            assert(type != SM_FILE_TYPE::NOT_ASSIGNED && "Type Not Assigned");

            initializeNewHeaderUnitsSMRulesNotOutdated(builder);
            builder.addNewTopBeUpdatedTargets(&this->realBTargets[round]);
            isComplete = true;
        }
    }
}

string SMFile::getOutputFileName() const
{
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        // node->getFileName() is not used to prevent error in case header-file with same fileName exists in 2 different
        // include directories. But is being included by different logicalName.
        string str = logicalName;
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

void SMFile::saveSMRulesJsonToSMRulesCache(const string &smrulesFileOutputClang)
{
    // We get half-node since we trust the compiler to have generated if it is returning true
    const Node *smRuleFileNode =
        Node::getHalfNode(target->myBuildDir->filePath + slashc + getOutputFileName() + ".smrules");

    Document d;
    // The assumption is that clang only outputs scanning data during scanning on output while MSVC outputs nothing.
    if (smrulesFileOutputClang.empty())
    {
        smRuleFileBuffer = readValueFromFile(smRuleFileNode->filePath, d);
    }
    else
    {
        // ParseInsitu with .data() can be used.
        d.Parse(smrulesFileOutputClang.c_str());
    }

    Value &rule = d.FindMember(svtogsr(JConsts::rules))->value[0];
    smRulesCache = BuildCache::Cpp::ModuleFile::SmRules{};

    if (const auto it = rule.FindMember(svtogsr(JConsts::provides)); it == rule.MemberEnd())
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            smRulesCache.exportName = logicalName;
        }
        smRulesCache.isInterface = false;
    }
    else
    {
        Value &provideJson = it->value[0];
        const Value &logicalNameValue = provideJson.FindMember(Value(svtogsr(JConsts::logicalName)))->value;
        const bool isInterface = provideJson.FindMember(Value(svtogsr(JConsts::isInterface)))->value.GetBool();
        smRulesCache.exportName = vtosv(logicalNameValue);
        smRulesCache.isInterface = isInterface;
    }

    // Pushing header-unit array and module-array

    for (auto it = rule.FindMember(svtogsr(JConsts::requires_)); it != rule.MemberEnd(); ++it)
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            smRulesCache.headerUnitArray.reserve(it->value.GetArray().Size());
        }
        for (Value &requireValue : it->value.GetArray())
        {
            Value &logicalNameSMRules = requireValue.FindMember(Value(svtogsr(JConsts::logicalName)))->value;

            if (auto sourcePathIt = requireValue.FindMember(Value(svtogsr(JConsts::sourcePath)));
                sourcePathIt == requireValue.MemberEnd())
            {
                // If source-path does not exist, then it is not a header-unit
                // This source-path will be saved before saving and then will be checked in next invocations in
                // resolveRequirePaths function.

                auto &[fullPath, logicalName] = smRulesCache.moduleArray.emplace_back();
                logicalName = vtosv(logicalNameSMRules);
            }
            else
            {
                includeNames.emplace_back(logicalNameSMRules.GetString(), logicalNameSMRules.GetStringLength());

                BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu =
                    smRulesCache.headerUnitArray.emplace_back();

                // lower-cased before saving for further use
                string_view str(sourcePathIt->value.GetString(), sourcePathIt->value.GetStringLength());
                lowerCasePStringOnWindows(const_cast<char *>(str.data()), str.size());
                hu.node = Node::getHalfNode(str);

                hu.angle = requireValue.FindMember(Value(svtogsr(JConsts::lookupMethod)))->value ==
                           Value(svtogsr(JConsts::includeAngle));
            }
        }
    }
}

InclNodePointerTargetMap SMFile::findHeaderUnitTarget(Node *headerUnitNode) const
{
    // The target from which this header-unit comes from
    CppSourceTarget *huDirTarget = nullptr;
    const HeaderUnitNode *nodeDir = nullptr;

    // Iterating over all header-unit-dirs of the target to find out which header-unit dir this header-unit
    // comes from and which target that header-unit-dir belongs to if any
    for (const auto &[inclNode, targetLocal] : target->reqHuDirs)
    {
        if (pathContainsFile(inclNode.node->filePath, headerUnitNode->filePath))
        {
            if (huDirTarget && huDirTarget != targetLocal)
            {
                printErrorMessage(FORMAT("Module Header Unit\n{}\n belongs to two different Targets\n{}\n{}\n",
                                         headerUnitNode->filePath, nodeDir->node->filePath, inclNode.node->filePath,
                                         settings.pcSettings.toolErrorOutput));
            }
            huDirTarget = targetLocal;
            nodeDir = &inclNode;
        }
    }

    if (huDirTarget)
    {
        return {nodeDir, huDirTarget};
    }

    if (const auto it = target->configuration->moduleFilesToTarget.find(headerUnitNode);
        it != target->configuration->moduleFilesToTarget.end())
    {
        // The mapped target must be the same as the SMFile target from which this header-unit is discovered or one
        // of its reqDeps
        if (it->second == target || target->reqDeps.find(it->second) != target->reqDeps.end())
        {
            return {nullptr, it->second};
            for (const InclNode &incl : target->reqIncls)
            {
                if (pathContainsFile(incl.node->filePath, headerUnitNode->filePath))
                {
                    return {nullptr, it->second};
                }
            }
            // printErrorMessage("HMake Internal Error");
        }
    }

    string errorMessage = FORMAT("Could not find the target for Header Unit\n{}\ndiscovered in file\n{}\nin "
                                 "Target\n{}.\nSearched for header-unit target in the following reqHuDirs.\n",
                                 headerUnitNode->filePath, node->filePath, target->name);
    for (const auto &[inclNode, targetLocal] : target->reqHuDirs)
    {
        errorMessage += FORMAT("HuDirTarget {} inclNode {}\n", targetLocal->name, inclNode.node->filePath);
    }

    printErrorMessage(errorMessage);
}

void SMFile::initializeNewHeaderUnitsSMRulesNotOutdated(Builder &builder)
{
    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(&headerUnit, hu.angle);
        addDepHalfNowHalfLater(headerUnit);
    }

    builder.executeMutex.lock();
    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.addedForRoundOne)
        {
            headerUnit.addedForRoundOne = true;
            builder.updateBTargets.emplace(&headerUnit.realBTargets[1]);
            builder.updateBTargetsSizeGoal += 1;
        }
    }
}

void SMFile::initializeHeaderUnits(Builder &builder)
{
    for (uint32_t i = 0; i < smRulesCache.headerUnitArray.size(); ++i)
    {
        auto &singleHuDep = smRulesCache.headerUnitArray[i];
        auto [nodeDir, huDirTarget] = huDirPlusTargets[i];

        SMFile *headerUnit = nullptr;

        if (const auto it = huDirTarget->headerUnitsSet.find(singleHuDep.node); it == huDirTarget->headerUnitsSet.end())
        {
            headerUnit = new SMFile(huDirTarget, singleHuDep.node);
            // not needed for new header-units since the doubt is only about older header-units that whether they have
            // been added or not.
            headerUnit->addedForRoundOne = true;
            huDirTarget->headerUnitsSet.emplace(headerUnit);

            /*if (nodeDir->ignoreHeaderDeps)
            {
                headerUnit->ignoreHeaderDeps = ignoreHeaderDepsForIgnoreHeaderUnits;
            }*/

            headerUnit->indexInBuildCache = huDirTarget->newHeaderUnitsSize + huDirTarget->oldHeaderUnits.size();
            ++huDirTarget->newHeaderUnitsSize;

            headerUnit->type = SM_FILE_TYPE::HEADER_UNIT;
            headerUnit->logicalName = string(includeNames[i]);
            headerUnit->buildCache.node = const_cast<Node *>(headerUnit->node);
        }
        else
        {
            headerUnit = *it;

            if (!headerUnit->addedForRoundOne)
            {
                headerUnit->addedForRoundOne = true;
            }
        }

        // Should be true if JConsts::lookupMethod == "include-angle";
        headerUnitsConsumptionData.emplace(headerUnit, singleHuDep.angle);
        addDepHalfNowHalfLater(*headerUnit);

        singleHuDep.targetIndex = huDirTarget->cacheIndex;
        singleHuDep.myIndex = headerUnit->indexInBuildCache;
    }
}

void SMFile::setSMFileType()
{
    if (smRulesCache.isInterface)
    {
        if (!smRulesCache.exportName.empty())
        {
            type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        }
    }
    else
    {
        if (!smRulesCache.exportName.empty())
        {
            type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
        }
        else
        {
            type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
        }
    }
}

void SMFile::checkObjectFileOutdatedHeaderUnits()
{
    if (isObjectFileOutdatedCallCompleted || atomic_ref(isObjectFileOutdatedCallCompleted).load())
    {
        return;
    }

    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];

        if (!localTarget)
        {
            // TODO
            // Maybe we can set isObjectFileSMRuleFileOutdated here as well.
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.isObjectFileOutdatedCallCompleted &&
            !atomic_ref(headerUnit.isObjectFileOutdatedCallCompleted).load())
        {
            headerUnit.checkObjectFileOutdatedHeaderUnits();
        }

        if (headerUnit.isObjectFileOutdated)
        {
            isObjectFileOutdated = true;
            atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
            return;
        }
    }
    atomic_ref(isObjectFileOutdatedCallCompleted).store(true);
}

void SMFile::checkObjectFileOutdatedModules()
{
    for (const BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &hu : smRulesCache.headerUnitArray)
    {
        CppSourceTarget *localTarget = cppSourceTargets[hu.targetIndex];
        if (!localTarget)
        {
            isObjectFileOutdated = true;
            return;
        }

        SMFile &headerUnit = localTarget->oldHeaderUnits[hu.myIndex];

        if (!headerUnit.isObjectFileOutdatedCallCompleted &&
            !atomic_ref(headerUnit.isObjectFileOutdatedCallCompleted).load())
        {
            headerUnit.checkObjectFileOutdatedHeaderUnits();
        }

        if (headerUnit.isObjectFileOutdated)
        {
            isObjectFileOutdated = true;
            return;
        }
    }
}

string SMFile::getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const
{
    return getReducedPath(target->myBuildDir->filePath + slashc + node->getFileName() + ".m.o", pathPrint);
}

BTargetType SMFile::getBTargetType() const
{
    return BTargetType::SMFILE;
}

void SMFile::updateBuildCache()
{
    BuildCache::Cpp::ModuleFile *modFile;
    if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        modFile = &target->cppBuildCache.headerUnits[indexInBuildCache];
    }
    else
    {
        modFile = &target->cppBuildCache.modFiles[indexInBuildCache];
    }
    auto &[srcFile, smRules, compileCommandWithTool] = *modFile;
    if (Builder::round == 1)
    {
        // moduleArray is not yet moved as it is used in resolveRequiredPaths and is later updated in
        // SMFile::updateBTarget round 0.
        srcFile = std::move(buildCache);
        smRules.headerUnitArray = std::move(smRulesCache.headerUnitArray);
        smRules.exportName = smRulesCache.exportName;
        smRules.isInterface = smRulesCache.isInterface;
    }
    else
    {
        smRules.moduleArray = std::move(smRulesCache.moduleArray);
        compileCommandWithTool.hash = compileCommandWithToolCache.hash;
    }
}

string SMFile::getCompileCommand()
{
    string s;
    if (const Compiler &c = target->configuration->compilerFeatures.compiler;
        c.bTFamily == BTFamily::GCC && c.btSubFamily == BTSubFamily::CLANG)
    {
        if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            s = "-fmodule-header=user -o \"" + objectNode->filePath + "\" -noScanIPC -xc++-header \"" + node->filePath +
                '\"';
        }
        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            s = "-fmodules-reduced-bmi -o \"" + objectNode->filePath + "\" -noScanIPC -c -xc++-module \"" +
                node->filePath + "\" -fmodule-output=\"" + target->myBuildDir->filePath + slashc + getOutputFileName() +
                ".ifc\"";
        }
        else
        {
            s = "-o \"" + objectNode->filePath + "\" -noScanIPC -c \"" + node->filePath + '\"';
        }
    }

    return s;
}

// TODO
// Propose the idea of big smfile. This combined with markArchivePoint could result in much increased in performance.
void SMFile::setFileStatusAndPopulateAllDependencies()
{
    flat_hash_set<SMFile *> uniqueElements;
    for (auto &[dependency, ignore] : realBTargets[0].dependencies)
    {
        if (dependency->bTarget->getBTargetType() == BTargetType::SMFILE)
        {
            if (auto *smFile = static_cast<SMFile *>(dependency->bTarget))
            {
                if (uniqueElements.emplace(smFile).second)
                {
                    for (SMFile *smFileDep : smFile->allSMFileDependenciesRoundZero)
                    {
                        uniqueElements.emplace(smFileDep);
                    }
                }
            }
        }
    }

    allSMFileDependenciesRoundZero.reserve(uniqueElements.size());
    for (SMFile *smFile : uniqueElements)
    {
        allSMFileDependenciesRoundZero.emplace_back(smFile);
    }

    if (!fileStatus)
    {
        for (const SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            if (smFile->objectNode->lastWriteTime > objectNode->lastWriteTime)
            {
                fileStatus = true;
                break;
            }
        }
    }

    if (fileStatus)
    {
        for (SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            for (auto &h : smFile->headerUnitsConsumptionData)
            {
                headerUnitsConsumptionData.emplace(h);
            }
        }
    }
}

string SMFile::getFlag() const
{
    string str;
    if (target->evaluate(BTFamily::MSVC))
    {
        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }
        if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            str = "/interface ";
            str += "/ifcOutput" + addQuotes(target->myBuildDir->filePath + slashc + getOutputFileName() + ".ifc") + " ";
        }
        else if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            str = "/exportHeader ";
            str += "/ifcOutput" + addQuotes(objectNode->filePath) + " ";
        }
        else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            str = "/internalPartition ";
        }

        if (type != SM_FILE_TYPE::HEADER_UNIT)
        {
            str += "/Fo" + addQuotes(objectNode->filePath);
        }
    }
    else if (target->evaluate(BTFamily::GCC))
    {
        // Flags are for clang. I don't know GCC flags at the moment but clang is masqueraded as gcc in HMake so
        // modifying this temporarily on Linux.

        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }
        /*        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
                {
                    str = "/interface ";
                }
                else if (type == SM_FILE_TYPE::HEADER_UNIT)
                {
                    str = "/exportHeader ";
                }
                else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
                {
                    str = "/internalPartition ";
                }*/

        if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
        {
            str +=
                "-fmodule-output=" + addQuotes(target->myBuildDir->filePath + slashc + getOutputFileName() + ".ifc") +
                " ";
        }

        if (type != SM_FILE_TYPE::HEADER_UNIT)
        {
            str += "-o " + addQuotes(objectNode->filePath);
        }
    }

    return str;
}

string SMFile::getFlagPrint() const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    const bool infra = ccpSettings.infrastructureFlags;

    string str;

    if (target->evaluate(BTFamily::MSVC))
    {
        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }

        if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
        {
            str = infra ? "/interface " : "";
            str += infra ? "/ifcOutput" : "";

            if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(target->myBuildDir->filePath + slashc + getOutputFileName() + ".ifc",
                                      ccpSettings.ifcOutputFile) +
                       " ";
            }
        }
        else if (type == SM_FILE_TYPE::HEADER_UNIT)
        {
            str = infra ? "/exportHeader " : "";
            str += infra ? "/ifcOutput" : "";

            if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(objectNode->filePath, ccpSettings.ifcOutputFile) + " ";
            }
        }
        else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            str = (infra ? "/internalPartition " : "") + str;
        }

        if (type != SM_FILE_TYPE::HEADER_UNIT)
        {
            str += infra ? "/Fo" : "";
            if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(objectNode->filePath, ccpSettings.objectFile) + " ";
            }
        }
    }
    else if (target->evaluate(BTFamily::GCC))
    {

        if (type == SM_FILE_TYPE::NOT_ASSIGNED)
        {
            printErrorMessage("Error! In getRequireFlag() type is NOT_ASSIGNED");
        }
        /*        else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
                {
                    str = (infra ? "/interface " : "");
                }
                else if (type == SM_FILE_TYPE::HEADER_UNIT)
                {
                    str = (infra ? "/exportHeader " : "");
                }
                else if (type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
                {
                    str = (infra ? "/internalPartition " : "") + str;
                }*/

        /*if (type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION)
        {
            str += infra ? "-fmodule-output=" : "";

            if (ccpSettings.ifcOutputFile.printLevel != PathPrintLevel::NO)
            {
                str += getReducedPath(outputFilesWithoutExtension + ".ifc", ccpSettings.ifcOutputFile) + " ";
            }
        }

        str += infra ? "-o" : "";
        if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
        {
            str += getReducedPath(outputFilesWithoutExtension + ".m.o", ccpSettings.objectFile) + " ";
        }*/
    }

    return str;
}

string SMFile::getRequireFlag(const SMFile &dependentSMFile) const
{

    string str;

    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    }
    if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT ||
        type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
    {
        const string ifcFilePath = addQuotes(target->myBuildDir->filePath + slashc + getOutputFileName() + ".ifc");
        str = "/reference " + logicalName + "=" + ifcFilePath + " ";
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionData.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");

        const string angleStr = dependentSMFile.headerUnitsConsumptionData.find(this)->second ? "angle" : "quote";
        str += "/headerUnit:";
        str += angleStr + " ";
        str += logicalName + "=" + addQuotes(objectNode->filePath) + " ";
    }
    else
    {
        printErrorMessage("HMake Error! In getRequireFlag() unknown type");
    }
    return str;
}

string SMFile::getRequireFlagPrint(const SMFile &dependentSMFile) const
{
    const string ifcFilePath = target->myBuildDir->filePath + slashc + getOutputFileName() + ".ifc";
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getRequireIFCPathOrLogicalName = [&](const string &logicalName_) {
        return ccpSettings.onlyLogicalNameOfRequireIFC
                   ? logicalName_
                   : logicalName_ + "=" + getReducedPath(ifcFilePath, ccpSettings.requireIFCs);
    };

    string str;
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        printErrorMessage("HMake Error! In getRequireFlag() type is NOT_ASSIGNED");
    }
    if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT ||
        type == SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
    {
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            str = "";
        }
        else
        {
            if (ccpSettings.infrastructureFlags)
            {
                str += "/reference ";
            }
            str += getRequireIFCPathOrLogicalName(logicalName) + " ";
        }
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        assert(dependentSMFile.headerUnitsConsumptionData.contains(const_cast<SMFile *>(this)) &&
               "SMFile referencing a headerUnit for which there is no consumption method");
        if (ccpSettings.requireIFCs.printLevel == PathPrintLevel::NO)
        {
            str = "";
        }
        else
        {
            if (ccpSettings.infrastructureFlags)
            {
                const string angleStr =
                    dependentSMFile.headerUnitsConsumptionData.find(this)->second ? "angle" : "quote";
                str += "/headerUnit:" + angleStr + " ";
            }
            str += getRequireIFCPathOrLogicalName(logicalName) + " ";
        }
    }

    return str;
}

string SMFile::getModuleCompileCommandPrintLastHalf() const
{
    const CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string moduleCompileCommandPrintLastHalf;
    if (ccpSettings.requireIFCs.printLevel != PathPrintLevel::NO)
    {
        for (const SMFile *smFile : allSMFileDependenciesRoundZero)
        {
            moduleCompileCommandPrintLastHalf += smFile->getRequireFlagPrint(*this);
        }
    }

    moduleCompileCommandPrintLastHalf +=
        ccpSettings.infrastructureFlags
            ? target->getInfrastructureFlags(target->configuration->compilerFeatures.compiler)
            : "";
    moduleCompileCommandPrintLastHalf += getReducedPath(node->filePath, ccpSettings.sourceFile) + " ";
    moduleCompileCommandPrintLastHalf += getFlagPrint();
    return moduleCompileCommandPrintLastHalf;
}