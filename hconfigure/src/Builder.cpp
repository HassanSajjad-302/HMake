#include "Builder.hpp"
#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "Target.hpp"
#include "Utilities.hpp"
#include <fstream>
#include <memory>
#include <mutex>
#include <stack>
#include <thread>

using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack;

string getThreadId()
{
    string threadId;
    auto myId = std::this_thread::get_id();
    std::stringstream ss;
    ss << myId;
    threadId = ss.str();
    return threadId;
}

PostBasic::PostBasic(const BuildTool &buildTool, const string &commandFirstHalf, string printCommandFirstHalf,
                     const string &buildCacheFilesDirPath, const string &fileName, const PathPrint &pathPrint,
                     bool isTarget_)
    : isTarget{isTarget_}
{
    string str = isTarget ? "_t" : "";

    string outputFileName = buildCacheFilesDirPath + fileName + "_output" + str;
    string errorFileName = buildCacheFilesDirPath + fileName + "_error" + str;

    if (pathPrint.printLevel != PathPrintLevel::NO)
    {
        printCommandFirstHalf +=
            "> " + getReducedPath(outputFileName, pathPrint) + " 2>" + getReducedPath(errorFileName, pathPrint);
    }

    printCommand = std::move(printCommandFirstHalf);

#ifdef _WIN32
    path responseFile = path(buildCacheFilesDirPath + fileName + ".response");
    ofstream(responseFile) << commandFirstHalf;

    path toolPath = buildTool.bTPath;
    string cmdCharLimitMitigateCommand = addQuotes(toolPath.make_preferred().string()) + " @" +
                                         addQuotes(responseFile.generic_string()) + "> " + addQuotes(outputFileName) +
                                         " 2>" + addQuotes(errorFileName);
    bool success = system(addQuotes(cmdCharLimitMitigateCommand).c_str());
#else
    string finalCompileCommand = buildTool.bTPath.generic_string() + " " + commandFirstHalf + "> " +
                                 addQuotes(outputFileName) + " 2>" + addQuotes(errorFileName);
    bool success = system(finalCompileCommand.c_str());
#endif
    if (success == EXIT_SUCCESS)
    {
        successfullyCompleted = true;
        commandSuccessOutput = file_to_string(outputFileName);
    }
    else
    {
        successfullyCompleted = false;
        commandSuccessOutput = file_to_string(outputFileName);
        commandErrorOutput = file_to_string(errorFileName);
    }
}

void PostBasic::executePrintRoutine(uint32_t color, bool printOnlyOnError) const
{
    if (!printOnlyOnError)
    {
        print(fg(static_cast<fmt::color>(color)), pes, printCommand + " " + getThreadId() + "\n");
        if (successfullyCompleted)
        {
            if (!commandSuccessOutput.empty())
            {
                print(fg(static_cast<fmt::color>(color)), pes, commandSuccessOutput + "\n");
            }
            if (!commandErrorOutput.empty())
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes,
                      commandErrorOutput + "\n");
            }
        }
    }
    if (!successfullyCompleted)
    {
        if (!commandSuccessOutput.empty())
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes, commandSuccessOutput + "\n");
        }
        if (!commandErrorOutput.empty())
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes,
                  commandErrorOutput + "\n");
        }
    }
}

PostCompile::PostCompile(const Target &target_, const BuildTool &buildTool, const string &commandFirstHalf,
                         string printCommandFirstHalf, const string &buildCacheFilesDirPath, const string &fileName,
                         const PathPrint &pathPrint)
    : target{const_cast<Target &>(target_)}, PostBasic(buildTool, commandFirstHalf, std::move(printCommandFirstHalf),
                                                       buildCacheFilesDirPath, fileName, pathPrint, false)
{
}

bool PostCompile::checkIfFileIsInEnvironmentIncludes(const string &str)
{
    //  Premature Optimization Haha
    // TODO:
    //  Add a key in hconfigure that informs hbuild that the library isn't to be
    //  updated, so includes from the directories coming from it aren't mentioned
    //  in targetCache and neither are those libraries checked for an edit for
    //  faster startup times.

    // If a file is in environment includes, it is not marked as dependency as an
    // optimization. If a file is in subdirectory of environment include, it is
    // still marked as dependency. It is not checked if any of environment
    // includes is related(equivalent, subdirectory) with any of normal includes
    // or vice-versa.

    for (const string &dir : target.environment.includeDirectories)
    {
        if (equivalent(dir, path(str).parent_path()))
        {
            return true;
        }
    }
    return false;
}

void PostCompile::parseDepsFromMSVCTextOutput(SourceNode &sourceNode)
{
    vector<string> outputLines = split(commandSuccessOutput, "\n");
    string includeFileNote = "Note: including file:";

    for (auto iter = outputLines.begin(); iter != outputLines.end();)
    {
        if (iter->contains(includeFileNote))
        {
            unsigned long pos = iter->find_first_not_of(includeFileNote);
            pos = iter->find_first_not_of(" ", pos);
            iter->erase(iter->begin(), iter->begin() + (int)pos);
            if (!checkIfFileIsInEnvironmentIncludes(*iter))
            {
                sourceNode.headerDependencies.emplace(Node::getNodeFromString(*iter, true));
            }
            if (settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
            {
                iter = outputLines.erase(iter);
            }
        }
        else if (*iter == path(sourceNode.node->filePath).filename().string())
        {
            if (settings.ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput)
            {
                iter = outputLines.erase(iter);
            }
        }
        else
        {
            ++iter;
        }
    }

    string treatedOutput; // Output With All information of include files removed.
    for (const auto &i : outputLines)
    {
        treatedOutput += i;
    }
    commandSuccessOutput = treatedOutput;
}

void PostCompile::parseDepsFromGCCDepsOutput(SourceNode &sourceNode)
{
    string headerFileContents =
        file_to_string(target.buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".d");
    vector<string> headerDeps = split(headerFileContents, "\n");

    // First 2 lines are skipped as these are .o and .cpp file. While last line is
    // an empty line
    for (auto iter = headerDeps.begin() + 2; iter != headerDeps.end() - 1; ++iter)
    {
        unsigned long pos = iter->find_first_not_of(" ");
        string headerDep = iter->substr(pos, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos);
        if (!checkIfFileIsInEnvironmentIncludes(headerDep))
        {
            sourceNode.headerDependencies.emplace(Node::getNodeFromString(headerDep, true));
        }
    }
}

void PostCompile::executePostCompileRoutineWithoutMutex(SourceNode &sourceNode)
{
    if (successfullyCompleted)
    {
        // Clearing old header-deps and adding the new ones.
        sourceNode.headerDependencies.clear();
    }
    if (target.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(sourceNode);
    }
    else if (target.compiler.bTFamily == BTFamily::GCC && successfullyCompleted)
    {
        parseDepsFromGCCDepsOutput(sourceNode);
    }
}

Builder::Builder()
{
    map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> requirePaths;
    for (auto &[first, cTarget] : cTargets)
    {
        if (cTarget->cTargetType == TargetType::LIBRARY_STATIC || cTarget->cTargetType == TargetType::LIBRARY_SHARED ||
            cTarget->cTargetType == TargetType::EXECUTABLE)
        {
            auto &target = static_cast<Target &>(*cTarget);
            target.initializeForBuild();
            target.checkForPreBuiltAndCacheDir();
            target.parseModuleSourceFiles(*this);
        }
    }
    finalBTargetsSizeGoal = finalBTargets.size();
    launchThreadsAndUpdateBTargets();
    checkForHeaderUnitsCache();
    createHeaderUnits();
    populateRequirePaths(requirePaths);
    for (auto &[first, cTarget] : cTargets)
    {
        if (cTarget->cTargetType == TargetType::LIBRARY_STATIC || cTarget->cTargetType == TargetType::LIBRARY_SHARED ||
            cTarget->cTargetType == TargetType::EXECUTABLE)
        {
            auto &target = static_cast<Target &>(*cTarget);
            target.populateSetTarjanNodesSourceNodes(*this);
        }
    }
    populateSetTarjanNodesBTargetsSMFiles(requirePaths);
    populateFinalBTargets();
    launchThreadsAndUpdateBTargets();
}

void Builder::populateRequirePaths(
    map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths)
{
    for (const auto &it : moduleScopes)
    {
        for (const auto &smFileConst : it.second.smFiles)
        {
            auto &smFile = const_cast<SMFile &>(*smFileConst);

            string smFilePath =
                smFile.target.buildCacheFilesDirPath + path(smFile.node->filePath).filename().string() + ".smrules";
            Json smFileJson;
            ifstream(smFilePath) >> smFileJson;
            bool hasRequires = false;
            Json rule = smFileJson.at("rules").get<Json>()[0];
            if (rule.contains("provides"))
            {
                smFile.hasProvide = true;
                // There can be only one provides but can be multiple requires.
                smFile.logicalName = rule.at("provides").get<Json>()[0].at("logical-name").get<string>();
                if (auto [pos, ok] = requirePaths.emplace(
                        SMFileVariantPathAndLogicalName(smFile.logicalName, *(smFile.target.variantFilePath)), &smFile);
                    !ok)
                {
                    const auto &[key, val] = *pos;
                    // TODO
                    // Mention the module scope too.
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Module {} Is Being Provided By 2 different files:\n1){}\n2){}\n", smFile.logicalName,
                          smFile.node->filePath, val->node->filePath);
                    exit(EXIT_FAILURE);
                }
            }
            smFile.requiresJson = std::move(rule.at("requires"));
        }
    }
}

void Builder::checkForHeaderUnitsCache()
{
    // TODO:
    //  Currently using targetSet instead of variantFilePaths. With
    //  scoped-modules feature, it should use module scope, be it variantFilePaths
    //  or any module-scope

    set<const string *> addressed;
    // TODO:
    // Should be iterating over headerUnits scope instead.
    for (auto &[first, cTarget] : cTargets)
    {
        if (cTarget->cTargetType == TargetType::LIBRARY_STATIC || cTarget->cTargetType == TargetType::LIBRARY_SHARED ||
            cTarget->cTargetType == TargetType::EXECUTABLE)
        {
            auto &target = static_cast<Target &>(*cTarget);
            const auto [pos, Ok] = addressed.emplace(target.variantFilePath);
            if (!Ok)
            {
                continue;
            }
            // SHU system header units     AHU application header units
            path shuCachePath = path(**pos).parent_path() / "shus/headerUnits.cache";
            path ahuCachePath = path(**pos).parent_path() / "ahus/headerUnits.cache";
            if (exists(shuCachePath))
            {
                Json shuCacheJson;
                ifstream(shuCachePath) >> shuCacheJson;
                for (auto &shu : shuCacheJson)
                {
                    target.moduleSourceFileDependencies.emplace(shu, &target);
                }
            }
            else
            {
                create_directory(shuCachePath.parent_path());
            }
            if (exists(ahuCachePath))
            {
                Json ahuCacheJson;
                ifstream(shuCachePath) >> ahuCacheJson;
                for (auto &ahu : ahuCacheJson)
                {
                    const auto &[pos1, Ok1] = target.moduleSourceFileDependencies.emplace(ahu, &target);
                    const_cast<SMFile &>(*pos1).presentInCache = true;
                }
            }
            else
            {
                create_directory(ahuCachePath.parent_path());
            }
        }
    }
}

void Builder::populateSetTarjanNodesBTargetsSMFiles(
    const map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths)
{
    for (auto &[scopeStrPtr, moduleScope] : moduleScopes)
    {
        for (SMFile *smFileConst : moduleScope.smFiles)
        {
            auto &smFile = const_cast<SMFile &>(*smFileConst);
            // If following remains false then source is GlobalModuleFragment.
            bool hasLogicalNameRequireDependency = false;
            // If following is true then smFile is PartitionImplementation.
            bool hasPartitionExportDependency = false;
            for (const Json &json : smFileConst->requiresJson)
            {
                string requireLogicalName = json.at("logical-name").get<string>();
                if (requireLogicalName == smFile.logicalName)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", smFile.node->filePath);
                    exit(EXIT_FAILURE);
                }
                if (json.contains("lookup-method"))
                {
                    string headerUnitPath =
                        path(json.at("source-path").get<string>()).lexically_normal().generic_string();
                    bool isStandardHeaderUnit = false;
                    bool isApplicationHeaderUnit = false;
                    Target *ahuDirTarget;
                    auto isSubDir = [](path p, const path &root) {
                        while (p != p.root_path())
                        {
                            if (p == root.parent_path())
                            {
                                return true;
                            }
                            p = p.parent_path();
                        }
                        return false;
                    };
                    if (isSubDirPathStandard(headerUnitPath, smFile.target.environment.includeDirectories))
                    {
                        isStandardHeaderUnit = true;
                        ahuDirTarget = &(smFile.target);
                    }
                    if (!isStandardHeaderUnit)
                    {
                        for (auto &dir : moduleScope.appHUDirTarget)
                        {
                            // TODO;
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
                              headerUnitPath, *scopeStrPtr);
                        exit(EXIT_FAILURE);
                    }
                    const auto &[pos1, Ok1] = moduleScope.headerUnits.emplace(headerUnitPath, ahuDirTarget);
                    auto &smFileHeaderUnit = const_cast<SMFile &>(*pos1);
                    smFile.headerUnitsConsumptionMethods[&smFileHeaderUnit].emplace(
                        json.at("lookup-method").get<string>() == "include-angle", requireLogicalName);
                    smFile.addDependency(smFileHeaderUnit);

                    if (Ok1)
                    {
                        smFileHeaderUnit.type = SM_FILE_TYPE::HEADER_UNIT;
                        smFileHeaderUnit.resultType = ResultType::CPP_MODULE;
                        smFileHeaderUnit.standardHeaderUnit = isStandardHeaderUnit;
                        if (isApplicationHeaderUnit)
                        {
                            smFileHeaderUnit.ahuTarget = ahuDirTarget;
                        }
                        // TODO:
                        // Check for update
                        smFileHeaderUnit.fileStatus = FileStatus::NEEDS_UPDATE;
                    }
                }
                else
                {
                    hasLogicalNameRequireDependency = true;
                    if (requireLogicalName.contains(':'))
                    {
                        hasPartitionExportDependency = true;
                    }
                    if (auto it = requirePaths.find(
                            SMFileVariantPathAndLogicalName(requireLogicalName, *(smFile.target.variantFilePath)));
                        it == requirePaths.end())
                    {
                        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                              "No File Provides This {}.\n", requireLogicalName);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        SMFile &smFileDep = *(const_cast<SMFile *>(it->second));
                        smFile.addDependency(smFileDep);
                    }
                }
            }

            if (smFile.hasProvide)
            {
                smFile.type =
                    smFile.logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
            }
            else
            {
                if (hasLogicalNameRequireDependency)
                {
                    smFile.type = hasPartitionExportDependency ? SM_FILE_TYPE::PARTITION_IMPLEMENTATION
                                                               : SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
                }
                else
                {
                    smFile.type = SM_FILE_TYPE::GLOBAL_MODULE_FRAGMENT;
                }
            }
            smFile.target.addDependency(smFile);
            smFile.target.smFiles.emplace_back(&smFile);
            smFile.resultType = ResultType::CPP_MODULE;
        }
    }
}

void Builder::populateFinalBTargets()
{
    TBT::tarjanNodes = &tarjanNodesBTargets;
    TBT::findSCCS();
    TBT::checkForCycle([](BTarget *target) -> string { return "cycle check function not implemented"; }); // TODO
    unsigned long needsUpdate = 0;
    for (unsigned long i = 0; i < TBT::topologicalSort.size(); ++i)
    {
        BTarget *bTarget = TBT::topologicalSort[i];
        bTarget->indexInTopologicalSort = i;
        if (bTarget->resultType == ResultType::LINK)
        {
            auto &target = static_cast<Target &>(*bTarget);
            auto b = target.bTargetType;
            if (target.fileStatus != FileStatus::NEEDS_UPDATE)
            {
                int a;
                // TODO
                /*                if (target.getLinkOrArchiveCommand() != target.linkCommand)
                                {
                                    target.fileStatus = FileStatus::NEEDS_UPDATE;
                                }*/
            }
        }
        for (BTarget *dependent : bTarget->dependents)
        {
            if (bTarget->fileStatus == FileStatus::UPDATED)
            {
                --(dependent->dependenciesSize);
            }
            else if (bTarget->fileStatus == FileStatus::NEEDS_UPDATE)
            {
                auto &dependentTarget = static_cast<Target &>(*dependent);
                dependentTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            }
        }
        if (bTarget->fileStatus == FileStatus::NEEDS_UPDATE)
        {
            ++needsUpdate;
            if (!bTarget->dependenciesSize)
            {
                finalBTargets.emplace_back(bTarget);
            }
        }
        for (BTarget *dependency : bTarget->dependencies)
        {
            bTarget->allDependencies.emplace(dependency);
            // Following assign all dependencies of the dependency to all dependencies of the bTarget. Because of
            // iteration in topologicalSort, indexInTopologicalSort for all dependencies would already be set.
            for (BTarget *dep : dependency->allDependencies)
            {
                bTarget->allDependencies.emplace(dep);
            }
        }
    }
    finalBTargetsSizeGoal = needsUpdate;
}

set<string> Builder::getTargetFilePathsFromVariantFile(const string &fileName)
{
    Json variantFileJson;
    ifstream(fileName) >> variantFileJson;
    return variantFileJson.at(JConsts::targets).get<set<string>>();
}

set<string> Builder::getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage)
{
    Json projectFileJson;
    ifstream(fileName) >> projectFileJson;
    vector<string> vec;
    if (isPackage)
    {
        vector<Json> pVariantJson = projectFileJson.at(JConsts::variants).get<vector<Json>>();
        for (const auto &i : pVariantJson)
        {
            vec.emplace_back(i.at(JConsts::index).get<string>());
        }
    }
    else
    {
        vec = projectFileJson.at(JConsts::variants).get<vector<string>>();
    }
    set<string> targetFilePaths;
    for (auto &i : vec)
    {
        for (const string &str :
             getTargetFilePathsFromVariantFile(i + (isPackage ? "/packageVariant.hmake" : "/projectVariant.hmake")))
        {
            targetFilePaths.emplace(str);
        }
    }
    return targetFilePaths;
}

mutex oneAndOnlyMutex;
mutex printMutex;

void Builder::launchThreadsAndUpdateBTargets()
{
    vector<thread *> threads;

    // TODO
    // Instead of launching all threads, only the required amount should be
    // launched. Following should be helpful for this calculation in DSL.
    // https://cs.stackexchange.com/a/16829
    unsigned short launchThreads = 12;
    if (launchThreads)
    {
        while (threads.size() != launchThreads - 1)
        {
            threads.emplace_back(new thread{&Builder::updateBTargets, this});
        }
        updateBTargets();
    }
    for (thread *t : threads)
    {
        t->join();
        delete t;
    }
    finalBTargets.clear();
    finalBTargetsIndex = 0;
}

void Builder::printDebugFinalBTargets()
{
    for (BTarget *bTarget : finalBTargets)
    {
    }
}

void Builder::updateBTargets()
{
    bool loop = true;
    while (loop)
    {
        oneAndOnlyMutex.lock();
        while (finalBTargetsIndex < finalBTargets.size())
        {
            printDebugFinalBTargets();
            BTarget *bTarget = finalBTargets[finalBTargetsIndex];
            ++finalBTargetsIndex;
            oneAndOnlyMutex.unlock();
            switch (bTarget->resultType)
            {
            case ResultType::LINK: {
                auto *target = static_cast<Target *>(bTarget->reportResultTo);
                unique_ptr<PostBasic> postLinkOrArchive;
                if (target->getTargetType() == TargetType::LIBRARY_STATIC)
                {
                    PostBasic postLinkOrArchive1 = target->Archive();
                    postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                }
                else if (target->getTargetType() == TargetType::EXECUTABLE)
                {
                    PostBasic postLinkOrArchive1 = target->Link();
                    postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                }

                /*                if (postLinkOrArchive->successfullyCompleted)
                                {
                                    target->targetCache.linkCommand =
                   target->getLinkOrArchiveCommand();
                                }
                                else
                                {
                                    target->targetCache.linkCommand = "";
                                }*/

                target->pruneAndSaveBuildCache(true);

                printMutex.lock();
                if (target->getTargetType() == TargetType::LIBRARY_STATIC)
                {
                    postLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor, false);
                }
                else if (target->getTargetType() == TargetType::EXECUTABLE)
                {
                    postLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor, false);
                }
                printMutex.unlock();
                if (!postLinkOrArchive->successfullyCompleted)
                {
                    exit(EXIT_FAILURE);
                }
            }
            break;
            case ResultType::SOURCENODE: {
                auto *target = static_cast<Target *>(bTarget->reportResultTo);
                auto *sourceNode = static_cast<SourceNode *>(bTarget);
                PostCompile postCompile = target->Compile(*sourceNode);
                postCompile.executePostCompileRoutineWithoutMutex(*sourceNode);
                printMutex.lock();
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                printMutex.unlock();
            }
            break;
            case ResultType::CPP_SMFILE: {
                auto *smFile = static_cast<SMFile *>(bTarget);
                PostBasic postBasic = smFile->target.GenerateSMRulesFile(*smFile, true);
                printMutex.lock();
                postBasic.executePrintRoutine(settings.pcSettings.compileCommandColor, true);
                printMutex.unlock();
                if (!postBasic.successfullyCompleted)
                {
                    exit(EXIT_FAILURE);
                }
            }
            break;
            case ResultType::CPP_MODULE: {
                auto &smFile = static_cast<SMFile &>(*bTarget);
                PostCompile postCompile = smFile.target.CompileSMFile(smFile, *this);
                postCompile.executePostCompileRoutineWithoutMutex(smFile);
                printMutex.lock();
                postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor, false);
                printMutex.unlock();
                if (!postCompile.successfullyCompleted)
                {
                    smFile.target.pruneAndSaveBuildCache(false);
                    exit(EXIT_FAILURE);
                }
                smFile.fileStatus = FileStatus::UPDATED;
            }
            break;
            case ResultType::ACTIONTARGET:
                break;
            }
            oneAndOnlyMutex.lock();
            switch (bTarget->resultType)
            {
            case ResultType::LINK: {
                auto *target = static_cast<Target *>(bTarget->reportResultTo);
                target->execute(bTarget->id);
                printMutex.lock();
                target->executePrintRoutine(bTarget->id);
                printMutex.unlock();
            }
            break;
            case ResultType::SOURCENODE:
                break;
            case ResultType::CPP_SMFILE:
                break;
            case ResultType::CPP_MODULE:
                break;
            case ResultType::ACTIONTARGET:
                break;
            }
            for (BTarget *dependent : bTarget->dependents)
            {
                --(dependent->dependenciesSize);
                if (!dependent->dependenciesSize && dependent->fileStatus == FileStatus::NEEDS_UPDATE)
                {
                    finalBTargets.emplace_back(dependent);
                }
            }
        }
        if (finalBTargetsIndex == finalBTargetsSizeGoal)
        {
            loop = false;
        }
        oneAndOnlyMutex.unlock();
    }
}

void Builder::createHeaderUnits()
{
    // TODO:
    //  Currently using targetSet instead of variantFilePaths. With
    //  scoped-modules feature, it should use module scope, be it variantFilePaths
    //  or any module-scope

    set<const string *> addressed;
    for (auto &[first, cTarget] : cTargets)
    {
        if (cTarget->cTargetType == TargetType::LIBRARY_STATIC || cTarget->cTargetType == TargetType::LIBRARY_SHARED ||
            cTarget->cTargetType == TargetType::EXECUTABLE)
        {
            auto &target = static_cast<Target &>(*cTarget);

            const auto [pos, Ok] = addressed.emplace(target.variantFilePath);
            if (!Ok)
            {
                continue;
            }
            // SHU system header units     AHU application header units
            path shuCachePath = path(**pos) / "shus/headerUnits.cache";
            path ahuCachePath = path(**pos) / "ahus/headerUnits.cache";
            if (exists(shuCachePath))
            {
                Json shuCacheJson;
                ifstream(shuCachePath) >> shuCacheJson;
                for (auto &shu : shuCacheJson)
                {
                    target.moduleSourceFileDependencies.emplace(shu, &target);
                }
            }
            if (exists(ahuCachePath))
            {
                Json ahuCacheJson;
                ifstream(shuCachePath) >> ahuCacheJson;
                for (auto &ahu : ahuCacheJson)
                {
                    const auto &[pos1, Ok1] = target.moduleSourceFileDependencies.emplace(ahu, &target);
                    const_cast<SMFile &>(*pos1).presentInCache = true;
                }
            }
        }
    }
}

bool Builder::isSubDirPathStandard(const path &headerUnitPath, set<string> &environmentIncludes)
{
    string headerUnitPathSMallCase = headerUnitPath.generic_string();
    for (auto &c : headerUnitPathSMallCase)
    {
        c = (char)::tolower(c);
    }
    for (const string &str : environmentIncludes)
    {
        string directoryPathSmallCase = str;
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

bool Builder::isSubDirPathApplication(const path &headerUnitPath, map<string *, Target *> &applicationIncludes)
{

    return false;
}