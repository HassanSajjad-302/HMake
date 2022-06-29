
#include "BBuild.hpp"

#include "fmt/color.h"
#include "fmt/format.h"
#include "fstream"
#include "iostream"
#include "regex"
#include <utility>

using std::ifstream, std::ofstream, std::filesystem::exists, std::filesystem::copy_options, std::runtime_error,
    std::cout, std::endl, std::to_string, std::filesystem::create_directory, std::filesystem::directory_iterator,
    std::regex, std::filesystem::current_path, std::cerr, std::make_shared, std::make_pair, std::lock_guard,
    std::unique_ptr, std::make_unique, fmt::print, std::filesystem::file_time_type, std::filesystem::last_write_time,
    std::get, std::make_tuple;

void from_json(const Json &j, BIDD &p)
{
    p.copy = j.at("COPY").get<bool>();
    p.path = j.at("PATH").get<string>();
}

void from_json(const Json &j, BLibraryDependency &p)
{
    p.preBuilt = j.at("PREBUILT").get<bool>();
    p.path = j.at("PATH").get<string>();
    if (p.preBuilt)
    {
        if (j.contains("IMPORTED_FROM_OTHER_HMAKE_PACKAGE"))
        {
            p.imported = j.at("IMPORTED_FROM_OTHER_HMAKE_PACKAGE").get<bool>();
            if (!p.imported)
            {
                p.hmakeFilePath = j.at("HMAKE_FILE_PATH").get<string>();
            }
        }
        else
        {
            // hbuild was ran in project variant mode.
            p.imported = true;
        }
    }
    p.copy = true;
}

void from_json(const Json &j, BCompileDefinition &p)
{
    p.name = j.at("NAME").get<string>();
    p.value = j.at("VALUE").get<string>();
}

ParsedTarget::ParsedTarget(const string &targetFilePath_)
{
    targetFilePath = targetFilePath_;
}

void ParsedTarget::parseTarget()
{
    string fileName = path(targetFilePath).filename().string();
    targetName = fileName.substr(0, fileName.size() - string(".hmake").size());
    Json targetFileJson;
    ifstream(targetFilePath) >> targetFileJson;

    targetType = targetFileJson.at("TARGET_TYPE").get<TargetType>();
    if (targetType != TargetType::EXECUTABLE && targetType != TargetType::STATIC && targetType != TargetType::SHARED)
    {
        cerr << "BTargetType value in the targetFile is not correct.";
        exit(EXIT_FAILURE);
    }

    if (targetFileJson.at("VARIANT").get<string>() == "PACKAGE")
    {
        packageMode = true;
    }
    else
    {
        packageMode = false;
    }
    if (packageMode)
    {
        copyPackage = targetFileJson.at("PACKAGE_COPY").get<bool>();
        if (copyPackage)
        {
            packageName = targetFileJson.at("PACKAGE_NAME").get<string>();
            packageCopyPath = targetFileJson.at("PACKAGE_COPY_PATH").get<string>();
            packageVariantIndex = targetFileJson.at("PACKAGE_VARIANT_INDEX").get<int>();
            packageTargetPath =
                packageCopyPath + packageName + "/" + to_string(packageVariantIndex) + "/" + targetName + "/";
        }
    }
    else
    {
        copyPackage = false;
    }
    outputName = targetFileJson.at("OUTPUT_NAME").get<string>();
    outputDirectory = targetFileJson.at("OUTPUT_DIRECTORY").get<path>().string();
    compiler = targetFileJson.at("COMPILER").get<Compiler>();
    linker = targetFileJson.at("LINKER").get<Linker>();
    if (targetType == TargetType::STATIC)
    {
        archiver = targetFileJson.at("ARCHIVER").get<Archiver>();
    }
    environment = targetFileJson.at("ENVIRONMENT").get<Environment>();
    compilerFlags = targetFileJson.at("COMPILER_FLAGS").get<string>();
    linkerFlags = targetFileJson.at("LINKER_FLAGS").get<string>();
    sourceFiles = SourceAggregate::convertFromJsonAndGetAllSourceFiles(targetFileJson, "SOURCE_");
    modulesSourceFiles = SourceAggregate::convertFromJsonAndGetAllSourceFiles(targetFileJson, "MODULES_");

    libraryDependencies = targetFileJson.at("LIBRARY_DEPENDENCIES").get<vector<BLibraryDependency>>();
    if (packageMode)
    {
        includeDirectories = targetFileJson.at("INCLUDE_DIRECTORIES").get<vector<BIDD>>();
    }
    else
    {
        vector<string> includeDirs = targetFileJson.at("INCLUDE_DIRECTORIES").get<vector<string>>();
        for (auto &i : includeDirs)
        {
            includeDirectories.push_back(BIDD{i, true});
        }
    }
    compilerTransitiveFlags = targetFileJson.at("COMPILER_TRANSITIVE_FLAGS").get<string>();
    linkerTransitiveFlags = targetFileJson.at("LINKER_TRANSITIVE_FLAGS").get<string>();
    compileDefinitions = targetFileJson.at("COMPILE_DEFINITIONS").get<vector<BCompileDefinition>>();
    preBuildCustomCommands = targetFileJson.at("PRE_BUILD_CUSTOM_COMMANDS").get<vector<string>>();
    postBuildCustomCommands = targetFileJson.at("POST_BUILD_CUSTOM_COMMANDS").get<vector<string>>();

    buildCacheFilesDirPath = (path(targetFilePath).parent_path() / ("Cache_Build_Files/")).generic_string();
    if (copyPackage)
    {
        consumerDependenciesJson = targetFileJson.at("CONSUMER_DEPENDENCIES").get<Json>();
    }
    // Parsing finished

    actualOutputName = getActualNameFromTargetName(targetType, Cache::osFamily, targetName);
    auto [pos, Ok] = variantFilePaths.emplace(path(targetFilePath).parent_path().parent_path().string() + "/" +
                                              (packageMode ? "packageVariant.hmake" : "projectVariant.hmake"));
    variantFilePath = &(*pos);
    if (!sourceFiles.empty() && !modulesSourceFiles.empty())
    {
        cerr << "A Target can not have both module-source and regular-source. You can make regular-source dependency "
                "of module source."
             << endl;
        cerr << "Target: " << targetFilePath << endl;
        exit(EXIT_FAILURE);
    }
    if (!modulesSourceFiles.empty())
    {
        isModule = true;
    }
}

void ParsedTarget::executePreBuildCommands() const
{
    if (!preBuildCustomCommands.empty() && settings.gpcSettings.preBuildCommandsStatement)
    {
        print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)),
              "Executing PRE_BUILD_CUSTOM_COMMANDS\n");
    }
    for (auto &i : preBuildCustomCommands)
    {
        if (settings.gpcSettings.preBuildCommands)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildSequenceOutput)), pes, i + "\n");
        }
        if (int a = system(i.c_str()); a != EXIT_SUCCESS)
        {
            exit(a);
        }
    }
}

void ParsedTarget::executePostBuildCommands() const
{
    if (!postBuildCustomCommands.empty() && settings.gpcSettings.postBuildCommandsStatement)
    {
        print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)),
              "Executing POST_BUILD_CUSTOM_COMMANDS\n");
    }
    for (auto &i : postBuildCustomCommands)
    {
        if (settings.gpcSettings.postBuildCommands)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildSequenceOutput)), pes, i + "\n");
        }
        if (int a = system(i.c_str()); a != EXIT_SUCCESS)
        {
            exit(a);
        }
    }
}

void ParsedTarget::setCompileCommand()
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    auto getIncludeFlag = [this]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        else
        {
            return "-I ";
        }
    };

    // compileCommand = addQuotes(compiler.bTPath.make_preferred().string()) + " ";
    if (ccpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        compileCommandPrintFirstHalf +=
            getReducedPath(compiler.bTPath.make_preferred().string(), ccpSettings.tool) + " ";
    }

    compileCommand += environment.compilerFlags + " ";
    if (ccpSettings.environmentCompilerFlags)
    {
        compileCommandPrintFirstHalf += environment.compilerFlags + " ";
    }
    compileCommand += compilerFlags + " ";
    if (ccpSettings.compilerFlags)
    {
        compileCommandPrintFirstHalf += compilerFlags + " ";
    }
    compileCommand += compilerTransitiveFlags + " ";
    if (ccpSettings.compilerTransitiveFlags)
    {
        compileCommandPrintFirstHalf += compilerTransitiveFlags + " ";
    }

    for (const auto &i : compileDefinitions)
    {
        compileCommand += (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
        if (ccpSettings.compileDefinitions)
        {
            compileCommandPrintFirstHalf +=
                (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
        }
    }

    for (const auto &i : includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.path) + " ");
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            compileCommandPrintFirstHalf.append(getIncludeFlag() +
                                                getReducedPath(i.path, ccpSettings.projectIncludeDirectories) + " ");
        }
    }

    for (const auto &i : environment.includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.directoryPath.generic_string()) + " ");
        if (ccpSettings.environmentIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            compileCommandPrintFirstHalf.append(
                getIncludeFlag() +
                getReducedPath(i.directoryPath.generic_string(), ccpSettings.environmentIncludeDirectories) + " ");
        }
    }
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
}

void from_json(const Json &j, Node *node)
{
    node = Node::getNodeFromString(j);
}

void to_json(Json &j, const SourceNode &sourceNode)
{
    j["SRC_FILE"] = sourceNode.node;
    j["HEADER_DEPENDENCIES"] = sourceNode.headerDependencies;
}

void from_json(const Json &j, SourceNode &sourceNode)
{
    // from_json function of Node* did not work correctly, so not using it.*/
    // sourceNode.node = j.at("SRC_FILE").get<Node*>();
    // sourceNode.headerDependencies = j.at("HEADER_DEPENDENCIES").get<set<Node *>>();

    sourceNode.node = Node::getNodeFromString(j.at("SRC_FILE").get<string>());
    vector<string> headerDeps;
    headerDeps = j.at("HEADER_DEPENDENCIES").get<vector<string>>();
    for (const auto &h : headerDeps)
    {
        sourceNode.headerDependencies.insert(Node::getNodeFromString(h));
    }
    sourceNode.presentInCache = true;
    sourceNode.presentInSource = false;
}

SourceNode &BTargetCache::addNodeInSourceFileDependencies(const string &str)
{
    SourceNode sourceNode{.node = Node::getNodeFromString(str)};
    auto [pos, ok] = sourceFileDependencies.insert(sourceNode);
    if (!pos->targetCacheSearched)
    {
        pos->targetCacheSearched = true;
        pos->presentInCache = !ok;
        pos->presentInSource = true;
    }
    return const_cast<SourceNode &>(*pos);
}

bool BTargetCache::areFilesLeftToCompile() const
{
    for (const auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            return true;
        }
    }
    return false;
}

bool BTargetCache::areAllFilesCompiled() const
{
    for (const auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus != FileStatus::UPDATED)
        {
            return false;
        }
    }
    return true;
}

unsigned long BTargetCache::maximumThreadsNeeded() const
{
    unsigned long numberOfOutdatedFiles = 0;
    for (const auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            ++numberOfOutdatedFiles;
        }
    }
    return numberOfOutdatedFiles;
}

SourceNode &BTargetCache::getNextNodeForCompilation()
{
    for (auto &sourceNode : sourceFileDependencies)
    {
        if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            return const_cast<SourceNode &>(sourceNode);
        }
    }
    cerr << "Internal Error: " << endl;
    cerr << "Should Not had reached here as when this function is called, we should have a sourcefile yet to be "
            "compiled."
         << endl;
    exit(-1);
}

void to_json(Json &j, const BTargetCache &bTargetCache)
{
    j["COMPILE_COMMAND"] = bTargetCache.compileCommand;
    j["DEPENDENCIES"] = bTargetCache.sourceFileDependencies;
}

void from_json(const Json &j, BTargetCache &bTargetCache)
{
    bTargetCache.compileCommand = j["COMPILE_COMMAND"];
    bTargetCache.sourceFileDependencies = j.at("DEPENDENCIES").get<set<SourceNode>>();
}
PostBasic::PostBasic(string commandFirstHalf, string printCommandFirstHalf, const string &buildCacheFilesDirPath,
                     const string &fileName, const PathPrint &pathPrint, bool isTarget_)
    : isTarget{isTarget_}
{

    string str = isTarget ? "_t" : "";

    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;

    string outputFileName = buildCacheFilesDirPath + fileName + "_output" + str;
    string errorFileName = buildCacheFilesDirPath + fileName + "_error" + str;

    commandFirstHalf += "> " + addQuotes(outputFileName) + " 2>" + addQuotes(errorFileName);

    if (pathPrint.printLevel != PathPrintLevel::NO)
    {
        printCommandFirstHalf += "> " + getReducedPath(outputFileName, ccpSettings.outputAndErrorFiles) + " 2>" +
                                 getReducedPath(errorFileName, ccpSettings.outputAndErrorFiles);
    }

    printCommand = std::move(printCommandFirstHalf);

#ifdef _WIN32
    commandFirstHalf = addQuotes(commandFirstHalf);
#endif
    if (system(commandFirstHalf.c_str()) == EXIT_SUCCESS)
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

void PostBasic::executePrintRoutine(uint32_t color) const
{
    string threadId;
    if (settings.gpcSettings.threadId)
    {
        auto myid = std::this_thread::get_id();
        std::stringstream ss;
        ss << myid;
        threadId = ss.str();
    }
    print(fg(static_cast<fmt::color>(color)), pes, printCommand + " " + threadId);
    print("\n");
    if (!commandSuccessOutput.empty())
    {
        print(fg(static_cast<fmt::color>(color)), pes, commandSuccessOutput);
        print("\n");
    }
    if (!commandErrorOutput.empty())
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes, commandErrorOutput);
        print("\n");
    }
}

PostCompile::PostCompile(const ParsedTarget &parsedTarget_, string commandFirstHalf, string printCommandFirstHalf,
                         const string &buildCacheFilesDirPath, const string &fileName, const PathPrint &pathPrint)
    : parsedTarget{const_cast<ParsedTarget &>(parsedTarget_)},
      PostBasic(std::move(commandFirstHalf), std::move(printCommandFirstHalf), buildCacheFilesDirPath, fileName,
                pathPrint, false)
{
}

bool PostCompile::checkIfFileIsInEnvironmentIncludes(const string &str)
{
    // If a file is in environment includes, it is not marked as dependency as an optimization.
    // If a file is in subdirectory of environment include, it is still marked as dependency.
    // It is not checked if any of environment includes is related(equivalent, subdirectory) with any of normal includes
    // or vice-versa.

    for (const auto &d : parsedTarget.environment.includeDirectories)
    {
        if (equivalent(d.directoryPath, path(str).parent_path()))
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
                Node *node = Node::getNodeFromString(*iter);
                sourceNode.headerDependencies.emplace(node);
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
        treatedOutput.append(i);
        treatedOutput.append("\n");
    }
    commandSuccessOutput = treatedOutput;
}

void PostCompile::parseDepsFromGCCDepsOutput(SourceNode &sourceNode)
{

    string headerFileContents = file_to_string(parsedTarget.buildCacheFilesDirPath +
                                               path(sourceNode.node->filePath).filename().string() + ".d");
    vector<string> headerDeps = split(headerFileContents, "\n");

    // First 2 lines are skipped as these are .o and .cpp file. While last line is an empty line
    for (auto iter = headerDeps.begin() + 2; iter != headerDeps.end() - 1; ++iter)
    {
        unsigned long pos = iter->find_first_not_of(" ");
        string headerDep = iter->substr(pos, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos);
        if (!checkIfFileIsInEnvironmentIncludes(headerDep))
        {
            Node *node = Node::getNodeFromString(headerDep);
            sourceNode.headerDependencies.emplace(node);
        }
    }
}

void PostCompile::executePostCompileRoutineWithoutMutex(SourceNode &sourceNode)
{
    if (!successfullyCompleted)
    {
        return;
    }
    // Clearing old header-deps and adding the new ones.
    sourceNode.headerDependencies.clear();
    if (parsedTarget.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(sourceNode);
    }
    else if (parsedTarget.compiler.bTFamily == BTFamily::GCC)
    {
        parseDepsFromGCCDepsOutput(sourceNode);
    }
}

bool operator<(const SMRuleRequires &lhs, const struct SMRuleRequires &rhs)
{
    if (lhs.type == SM_REQUIRE_TYPE::HEADER_UNIT && rhs.type == SM_REQUIRE_TYPE::HEADER_UNIT)
    {
        return lhs.sourcePath < rhs.sourcePath;
    }
    else
    {
        return lhs.logicalName < rhs.logicalName;
    }
}

void SMRuleRequires::populateFromJson(const Json &j, const string &smFilePath)
{
    logicalName = j.at("logical-name").get<string>();
    if (j.contains("lookup-method"))
    {
        // This is header unit
        type = SM_REQUIRE_TYPE::HEADER_UNIT;
        j.at("lookup-method").get<string>() == "include-angle" ? angle = true : angle = false;
        sourcePath = j.at("source-path").get<string>();
    }
    else
    {
        type = logicalName.contains(':') ? SM_REQUIRE_TYPE::PARTITION_EXPORT : SM_REQUIRE_TYPE::PRIMARY_EXPORT;
    }
    auto [pos, ok] = SMRuleRequires::smRuleRequiresSet->insert(*this);
    if (!ok)
    {
        print(stderr, "Two SMRuleRequire in SMFile: {} are same\nSMRuleRequire1:\n{}SMRuleRequire2:\n{}", smFilePath,
              *this, *pos);
        exit(EXIT_FAILURE);
    }
}

SMFile::SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_)
    : sourceNode{sourceNode_}, parsedTarget{parsedTarget_}
{
}

SMFile::SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_, bool angle_)
    : sourceNode{sourceNode_}, parsedTarget{parsedTarget_}, angle{angle_}
{
}

void SMFile::populateFromJson(const Json &j)
{
    bool hasRequires = false;
    Json rule = j.at("rules").get<Json>()[0];
    if (rule.contains("provides"))
    {
        hasProvide = true;
        // There can be only one provides but can be multiple requires.
        logicalName = rule.at("provides").get<Json>()[0].at("logical-name").get<string>();
    }

    set<SMRuleRequires> smRuleRequiresSet;
    if (rule.contains("requires"))
    {
        hasRequires = true;
        SMRuleRequires::smRuleRequiresSet = &smRuleRequiresSet;
        vector<Json> requireJsons = rule.at("requires").get<vector<Json>>();
        if (requireJsons.empty())
        {
            hasRequires = false;
        }
        dependencies.resize(requireJsons.size());
        for (unsigned long i = 0; i < requireJsons.size(); ++i)
        {
            dependencies[i].populateFromJson(requireJsons[i], sourceNode.node->filePath);
            if (dependencies[i].type == SM_REQUIRE_TYPE::PARTITION_EXPORT)
            {
                type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
            }
        }
    }

    if (!hasProvide && !hasRequires)
    {
        type = SM_FILE_TYPE::HEADER_UNIT;
    }
    else if (hasProvide)
    {
        type = logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
    }
    else
    {
        if (type != SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
        {
            type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
        }
    }
}

string SMFile::getFlag(const string &outputFilesWithoutExtension) const
{
    string str = "/ifcOutput" + addQuotes(outputFilesWithoutExtension + ".ifc") + " " + "/Fo" +
                 addQuotes(outputFilesWithoutExtension + ".o");
    if (type == SM_FILE_TYPE::NOT_ASSIGNED)
    {
        print(stderr, "Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return "/interface " + str;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        string angleStr = angle ? "angle " : "quote ";
        return "/exportHeader /headerName:" + angleStr + str;
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

string SMFile::getRequireFlag(const string &ifcFilePath) const
{
    if (type == SM_FILE_TYPE ::NOT_ASSIGNED)
    {
        print(stderr, "Error! In getRequireFlag() type is NOT_ASSIGNED");
        exit(EXIT_FAILURE);
    }
    else if (type == SM_FILE_TYPE::PRIMARY_EXPORT || type == SM_FILE_TYPE::PARTITION_EXPORT)
    {
        return "/reference " + logicalName + "=" + ifcFilePath;
    }
    else if (type == SM_FILE_TYPE::HEADER_UNIT)
    {
        string str = angle ? "angle" : "quote";
        return "/headerUnit:" + str + " " + logicalName + "=" + ifcFilePath;
    }
    print(stderr, "Error! In getRequireFlag() unknown type");
    exit(EXIT_FAILURE);
}

bool SMFileCompareWithHeaderUnits::operator()(const SMFile &lhs, const SMFile &rhs) const
{
    return lhs.sourceNode < rhs.sourceNode ||
           *(lhs.parsedTarget.variantFilePath) < *(rhs.parsedTarget.variantFilePath) || lhs.angle < rhs.angle;
}

bool SMFileCompareWithoutHeaderUnits::operator()(const SMFile &lhs, const SMFile &rhs) const
{
    return lhs.sourceNode < rhs.sourceNode || *(lhs.parsedTarget.variantFilePath) < *(rhs.parsedTarget.variantFilePath);
}

void Node::checkIfNotUpdatedAndUpdate()
{
    if (!isUpdated)
    {
        lastUpdateTime = last_write_time(path(filePath));
        isUpdated = true;
    }
}

bool std::less<Node *>::operator()(const Node *lhs, const Node *rhs) const
{
    return lhs->filePath < rhs->filePath;
}

bool operator<(const SourceNode &lhs, const SourceNode &rhs)
{
    return std::less<Node *>{}(lhs.node, rhs.node);
}

Node *Node::getNodeFromString(const string &str)
{
    Node *tempNode = new Node{.filePath = str};

    if (auto [pos, ok] = allFiles.insert(tempNode); !ok)
    {
        // Means it already exists
        delete tempNode;
        return *pos;
    }
    else
    {
        return tempNode;
    }
}

void ParsedTarget::addAllSourceFilesInBuildNode(BTargetCache &bTargetCache)
{
    for (const auto &i : sourceFiles)
    {
        SourceNode &node = bTargetCache.addNodeInSourceFileDependencies(i);
        node.compilationStatus = FileStatus::NEEDS_RECOMPILE;
    }
}

void ParsedTarget::setSourceNodeCompilationStatus(SourceNode &sourceNode, bool checkSMFileExists,
                                                  const string &extension) const
{
    sourceNode.compilationStatus = FileStatus::UPDATED;

    path objectFilePath =
        path(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + extension + ".o");

    if (checkSMFileExists)
    {
        path smrulesFilePath =
            path(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".smrules");

        if (!exists(objectFilePath) || !exists(smrulesFilePath))
        {
            sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
            return;
        }
    }
    else
    {
        if (!exists(objectFilePath))
        {
            sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
            return;
        }
    }
    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    sourceNode.node->checkIfNotUpdatedAndUpdate();
    if (sourceNode.node->lastUpdateTime > objectFileLastEditTime)
    {
        sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
    }
    else
    {
        for (auto &d : sourceNode.headerDependencies)
        {
            d->checkIfNotUpdatedAndUpdate();
            if (d->lastUpdateTime > objectFileLastEditTime)
            {
                sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
                break;
            }
        }
    }
}

void ParsedTarget::populateBuildTree()
{
    setCompileCommand();
    if (exists(path(buildCacheFilesDirPath) / (targetName + ".cache")))
    {
        Json targetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
        buildNode.targetCache = targetCacheJson;
    }

    if (!exists(path(buildCacheFilesDirPath)))
    {
        create_directory(buildCacheFilesDirPath);
        addAllSourceFilesInBuildNode(buildNode.targetCache);
    }
    else if (buildNode.targetCache.compileCommand != compileCommand)
    {
        addAllSourceFilesInBuildNode(buildNode.targetCache);
    }
    else
    {
        for (const auto &sourceFile : sourceFiles)
        {
            SourceNode &sourceNode = buildNode.targetCache.addNodeInSourceFileDependencies(sourceFile);
            setSourceNodeCompilationStatus(sourceNode, false);
        }
    }

    // Removing unused object-files from buildCacheFilesDirPath as they later affect linker command with *.o option.
    set<string> tmpObjectFiles;
    for (const auto &src : buildNode.targetCache.sourceFileDependencies)
    {
        if (src.presentInSource == src.presentInCache)
        {
            tmpObjectFiles.insert(path(src.node->filePath).filename().string() + ".o");
        }
    }

    for (const auto &j : directory_iterator(path(buildCacheFilesDirPath)))
    {
        if (!j.path().filename().string().ends_with(".o"))
        {
            continue;
        }
        if (!tmpObjectFiles.contains(j.path().filename().string()))
        {
            std::filesystem::remove(j.path());
        }
    }

    if (buildNode.targetCache.maximumThreadsNeeded() > 0)
    {
        buildNode.targetCache.compileCommand = compileCommand;
        buildNode.relink = true;
    }
    else
    {
        auto needsRelinkBeforePopularizingBuildTree = [&]() -> bool {
            for (const auto &src : buildNode.targetCache.sourceFileDependencies)
            {
                if (src.presentInSource != src.presentInCache)
                {
                    return true;
                }
            }
            path outputPath = path(outputDirectory) / actualOutputName;
            if (!exists(outputPath))
            {
                return true;
            }
            for (const auto &i : libraryDependencies)
            {
                if (i.preBuilt)
                {
                    if (!exists(path(i.path)))
                    {
                        cerr << "Prebuilt Library " << i.path << " Does Not Exist" << endl;
                        exit(EXIT_FAILURE);
                    }
                    if (last_write_time(i.path) > last_write_time(outputPath))
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        buildNode.relink = needsRelinkBeforePopularizingBuildTree();
    }
    if (buildNode.relink)
    {
        for (BuildNode *dependentBuildNode : buildNode.linkDependents)
        {
            dependentBuildNode->relink = true;
        }
    }
}

void ParsedTarget::populateModuleBuildTree(Builder &builder, map<string, SourceNode *> &requirePaths,
                                           set<TarjanNode<SMFile>> &tarjanNodesSMFiles)
{
    setCompileCommand();

    if (exists(path(buildCacheFilesDirPath) / (targetName + ".cache")))
    {
        Json targetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
        buildNode.targetCache = targetCacheJson;
    }

    bool rebuildAll = false;
    if (!exists(path(buildCacheFilesDirPath)))
    {
        create_directory(buildCacheFilesDirPath);
        rebuildAll = true;
    }
    else if (buildNode.targetCache.compileCommand != compileCommand)
    {
        rebuildAll = true;
    }

    for (const auto &sourceFile : modulesSourceFiles)
    {
        SourceNode &sourceNode = buildNode.targetCache.addNodeInSourceFileDependencies(sourceFile);
        if (auto [smFilePos, Ok] = builder.smFilesWithoutHeaderUnits.emplace(SMFile(sourceNode, *this)); Ok)
        {
            if (rebuildAll)
            {
                ++Builder::modulesActionsNeeded;
                sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
            }
            else
            {
                // For Header_Unit, we will recheck compilation-status later. We could not have avoided this check. It
                // will always return FileStatus::NEEDS_RECOMPILE as headerUnit object-file has extension including info
                // about whether it's a quoted or angled header-unit.
                setSourceNodeCompilationStatus(sourceNode, true);
            }
            if (sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
            {
                GenerateSMRulesFile(sourceNode);
            }

            string smFileName = path(sourceNode.node->filePath).filename().string();
            string smFilePath = buildCacheFilesDirPath + smFileName + ".smrules";

            Json smFileJson;
            ifstream(smFilePath) >> smFileJson;

            auto &smFile = const_cast<SMFile &>(*smFilePos);
            smFile.populateFromJson(smFileJson);

            if (smFile.hasProvide)
            {
                if (auto [pos, ok] = requirePaths.insert({smFile.logicalName, const_cast<SourceNode *>(&sourceNode)});
                    !ok)
                {
                    const auto &[key, val] = *pos;
                    print(stderr, "Module {} Is Being Provided By 2 different files:\n1){}\n2){}\n",
                          val->node->filePath, key, sourceNode.node->filePath);
                    exit(EXIT_FAILURE);
                }
            }
            if (smFile.type == SM_FILE_TYPE::HEADER_UNIT)
            {
                smFile.sourceNode.compilationStatus = FileStatus::UPDATED;
                smFile.materialize = false;
                smFile.angle = true;
                auto [smFile1, OkSMFile1] = builder.smFilesWithHeaderUnits.insert(smFile);
                tarjanNodesSMFiles.emplace(&(*smFile1));
                if (!OkSMFile1)
                {
                    print(stderr, "Angled Header-Unit {} is already present in smFilesWithHeaderUnits ",
                          smFile.sourceNode.node->filePath);
                }
                smFile.angle = false;
                auto [smFile2, OkSMFile2] = builder.smFilesWithHeaderUnits.insert(smFile);
                tarjanNodesSMFiles.emplace(&(*smFile2));
                if (!OkSMFile2)
                {
                    print(stderr, "Quoted Header-Unit {} is already present in smFilesWithHeaderUnits ",
                          smFile.sourceNode.node->filePath);
                }
            }
            else
            {
                smFile.materialize = true;
                auto [smFile1, OkSMFile1] = builder.smFilesWithHeaderUnits.insert(smFile);
                tarjanNodesSMFiles.emplace(&(*smFile1));
                if (!OkSMFile1)
                {
                    print(stderr, "Module Source {} is already present in smFilesWithHeaderUnits ",
                          smFile.sourceNode.node->filePath);
                }
            }
        }
        else
        {
            print(stderr,
                  "Module Source {} is provided by this target {}. But, is also provided by some other target in same "
                  "variant",
                  sourceFile, targetFilePath);
        }
    }

    buildNode.targetCache.compileCommand = compileCommand;
    buildNode.relink = true;
}

string ParsedTarget::getInfrastructureFlags()
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        return "/showIncludes /c /Tp";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        // Will like to use -MD but not using it currently because sometimes it prints 2 header deps in one line and
        // no space in them so no way of knowing whether this is a space in path or 2 different headers. Which then
        // breaks when last_write_time is checked for that path.
        return "-c -MMD";
    }
    return "";
}

string ParsedTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode)
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string command;
    if (ccpSettings.infrastructureFlags)
    {
        command += getInfrastructureFlags() + " ";
    }
    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += compiler.bTFamily == BTFamily::MSVC ? "/Fo" : "-o ";
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(buildCacheFilesDirPath + compileFileName + ".o", ccpSettings.objectFile) + " ";
    }
    return command;
}

PostCompile ParsedTarget::Compile(SourceNode &sourceNode)
{
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string finalCompileCommand = compileCommand + " ";

    finalCompileCommand += getInfrastructureFlags() + " " + addQuotes(sourceNode.node->filePath) + " ";
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand += "/Fo" + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }
    else
    {
        finalCompileCommand += "-o " + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }

    PostCompile postCompile{*this,
                            addQuotes(compiler.bTPath.make_preferred().string()) + " " + finalCompileCommand,
                            compileCommandPrintFirstHalf + getCompileCommandPrintSecondPart(sourceNode),
                            buildCacheFilesDirPath,
                            compileFileName,
                            settings.ccpSettings.outputAndErrorFiles};
    return postCompile;
}

PostBasic ParsedTarget::Archive()
{
    ArchiveCommandPrintSettings acpSettings = settings.acpSettings;
    auto getLibraryPath = [&]() -> string {
        if (archiver.bTFamily == BTFamily::MSVC)
        {
            return (path(outputDirectory) / (outputName + ".lib")).string();
        }
        else if (archiver.bTFamily == BTFamily::GCC)
        {
            return (path(outputDirectory) / ("lib" + outputName + ".a")).string();
        }
        return "";
    };

    string archiveCommand = addQuotes(archiver.bTPath.make_preferred().string()) + " ";

    string archivePrintCommand;
    if (acpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        archivePrintCommand += getReducedPath(archiver.bTPath.make_preferred().string(), acpSettings.tool) + " ";
    }

    archiveCommand += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
    if (acpSettings.infrastructureFlags)
    {
        archivePrintCommand += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
    }
    auto getArchiveOutputFlag = [&]() -> string {
        if (archiver.bTFamily == BTFamily::MSVC)
        {
            return "/OUT:";
        }
        else if (archiver.bTFamily == BTFamily::GCC)
        {
            return " rcs ";
        }
        return "";
    };
    archiveCommand += getArchiveOutputFlag();
    if (acpSettings.infrastructureFlags)
    {
        archivePrintCommand += getArchiveOutputFlag();
    }

    archiveCommand += addQuotes(getLibraryPath()) + " ";
    if (acpSettings.archive.printLevel != PathPrintLevel::NO)
    {
        archivePrintCommand += getReducedPath(getLibraryPath(), acpSettings.archive) + " ";
    }

    // Wildcard expansion works inside quotes in batch however does not work in bash.
#ifdef _WIN32
    archiveCommand += addQuotes(buildCacheFilesDirPath + "*.o") + " ";
    if (acpSettings.objectFiles.printLevel != PathPrintLevel::NO)
    {
        archivePrintCommand += getReducedPath(buildCacheFilesDirPath + "*.o", acpSettings.objectFiles) + " ";
    }

#else
    archiveCommand += addQuotes(buildCacheFilesDirPath) + "*.o ";
    if (acpSettings.objectFiles.printLevel != PathPrintLevel::NO)
    {
        string str = getReducedPath(buildCacheFilesDirPath, acpSettings.objectFiles);
        archivePrintCommand += str + "*.o ";
    }

#endif
    PostBasic postArchive(addQuotes(archiver.bTPath.make_preferred().string()) + " " + archiveCommand,
                          archivePrintCommand, buildCacheFilesDirPath, targetName, acpSettings.outputAndErrorFiles,
                          true);
    return postArchive;
}

PostBasic ParsedTarget::Link()
{
    LinkCommandPrintSettings lcpSettings = settings.lcpSettings;
    string linkCommand = addQuotes(linker.bTPath.make_preferred().string()) + " ";

    string linkPrintCommand;
    if (lcpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        linkPrintCommand += getReducedPath(linker.bTPath.make_preferred().string(), lcpSettings.tool) + " ";
    }

    linkCommand += linker.bTFamily == BTFamily::MSVC ? "/NOLOGO " : "";
    if (lcpSettings.infrastructureFlags)
    {
        linkPrintCommand += linker.bTFamily == BTFamily::MSVC ? "/NOLOGO " : "";
    }

    linkCommand += linkerFlags + " ";
    if (lcpSettings.linkerFlags)
    {
        linkPrintCommand += linkerFlags + " ";
    }

    linkCommand += linkerTransitiveFlags + " ";
    if (lcpSettings.linkerTransitiveFlags)
    {
        linkPrintCommand += linkerTransitiveFlags + " ";
    }

    // Wildcard expansion works inside quotes in batch however does not work in bash.
#ifdef _WIN32
    linkCommand += addQuotes(buildCacheFilesDirPath + "*.o") + " ";
    if (lcpSettings.objectFiles.printLevel != PathPrintLevel::NO)
    {
        linkPrintCommand += getReducedPath(buildCacheFilesDirPath + "*.o", lcpSettings.objectFiles) + " ";
    }
#else
    linkCommand += addQuotes(buildCacheFilesDirPath) + "*.o ";
    if (lcpSettings.objectFiles.printLevel != PathPrintLevel::NO)
    {
        string str = getReducedPath(buildCacheFilesDirPath, lcpSettings.objectFiles);
        if (!str.empty())
        {
            linkPrintCommand += str + "*.o ";
        }
    }
#endif

    auto getLinkFlag = [this](const std::string &libraryPath, const std::string &libraryName) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return addQuotes(libraryPath + libraryName + ".lib") + " ";
        }
        else
        {
            return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
        }
    };

    auto getLinkFlagPrint = [this](const std::string &libraryPath, const std::string &libraryName,
                                   const PathPrint &pathPrint) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return getReducedPath(libraryPath + libraryName + ".lib", pathPrint) + " ";
        }
        else
        {
            return "-L" + getReducedPath(libraryPath, pathPrint) + " -l" + getReducedPath(libraryName, pathPrint) + " ";
        }
    };

    for (const ParsedTarget *parsedTargetDep : libraryParsedTargetDependencies)
    {
        linkCommand.append(getLinkFlag(parsedTargetDep->outputDirectory, parsedTargetDep->outputName));
        linkPrintCommand.append(getLinkFlagPrint(parsedTargetDep->outputDirectory, parsedTargetDep->outputName,
                                                 lcpSettings.libraryDependencies));
    }

    for (auto &i : libraryDependencies)
    {
        if (i.preBuilt)
        {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                auto b = lcpSettings.libraryDependencies;
                linkCommand.append(i.path + " ");
                linkPrintCommand.append(getReducedPath(i.path + " ", b));
            }
            else
            {
                string dir = path(i.path).parent_path().string();
                string libName = path(i.path).filename().string();
                libName.erase(0, 3);
                libName.erase(libName.find('.'), 2);
                linkCommand.append(getLinkFlag(dir, libName));
                linkPrintCommand.append(getLinkFlagPrint(dir, libName, lcpSettings.libraryDependencies));
            }
        }
    }

    auto getLibraryDirectoryFlag = [this]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/LIBPATH:";
        }
        else
        {
            return "-L";
        }
    };

    for (const auto &i : libraryDirectories)
    {
        linkCommand.append(getLibraryDirectoryFlag() + addQuotes(i) + " ");
        if (lcpSettings.libraryDirectories.printLevel != PathPrintLevel::NO)
        {
            linkPrintCommand.append(getLibraryDirectoryFlag() + getReducedPath(i, lcpSettings.libraryDirectories) +
                                    " ");
        }
    }

    for (const auto &i : environment.libraryDirectories)
    {
        linkCommand.append(getLibraryDirectoryFlag() + addQuotes(i.directoryPath.generic_string()) + " ");
        if (lcpSettings.environmentLibraryDirectories.printLevel != PathPrintLevel::NO)
        {
            linkPrintCommand.append(
                getLibraryDirectoryFlag() +
                getReducedPath(i.directoryPath.generic_string(), lcpSettings.environmentLibraryDirectories) + " ");
        }
    }

    linkCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
    if (lcpSettings.infrastructureFlags)
    {
        linkPrintCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
    }

    linkCommand += addQuotes((path(outputDirectory) / actualOutputName).string());
    if (lcpSettings.binary.printLevel != PathPrintLevel::NO)
    {
        linkPrintCommand += getReducedPath((path(outputDirectory) / actualOutputName).string(), lcpSettings.binary);
    }

    PostBasic postLink(addQuotes(linker.bTPath.make_preferred().string()) + " " + linkCommand, linkPrintCommand,
                       buildCacheFilesDirPath, targetName, lcpSettings.outputAndErrorFiles, true);
    return postLink;
}

PostBasic ParsedTarget::GenerateSMRulesFile(const SourceNode &sourceNode)
{
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string finalCompileCommand = compileCommand + addQuotes(sourceNode.node->filePath) + " ";

    // finalCompileCommand += getInfrastructureFlags(sourceNode);
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand +=
            "/scanDependencies" + addQuotes(buildCacheFilesDirPath + compileFileName + ".smrules") + " ";
    }
    else
    {
        print(stderr, "Modules supported only on MSVC");
    }

    PostBasic postGenerateSMRules(addQuotes(compiler.bTPath.make_preferred().string()) + " " + finalCompileCommand,
                                  compileCommandPrintFirstHalf + getCompileCommandPrintSecondPart(sourceNode),
                                  buildCacheFilesDirPath, targetName, settings.ccpSettings.outputAndErrorFiles, true);
    return postGenerateSMRules;
}

TargetType ParsedTarget::getTargetType() const
{
    return targetType;
}

void ParsedTarget::pruneAndSaveBuildCache(BTargetCache &bTargetCache) const
{
    Json cacheFileJson = bTargetCache;
    ofstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) << cacheFileJson.dump(4);
}

void ParsedTarget::copyParsedTarget() const
{
    if (packageMode && copyPackage)
    {
        if (settings.gpcSettings.copyingPackage)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)), "Copying Package\n");
        }
        string copyFrom;
        if (targetType != TargetType::SHARED)
        {
            copyFrom = outputDirectory + actualOutputName;
        }
        else
        {
            // Shared Libraries Not Supported Yet.
        }
        if (settings.gpcSettings.copyingTarget)
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildSequenceOutput)),
                  "Copying Target {} From {} To {}\n", targetName, copyFrom, packageTargetPath);
        }
        create_directories(path(packageTargetPath));
        copy(path(copyFrom), path(packageTargetPath), copy_options::update_existing);
        for (auto &i : includeDirectories)
        {
            if (i.copy)
            {
                string includeDirectoryCopyFrom = i.path;
                string includeDirectoryCopyTo = packageTargetPath + "include/";
                if (settings.gpcSettings.copyingTarget)
                {
                    print(fg(static_cast<fmt::color>(settings.pcSettings.hbuildStatementOutput)),
                          "Copying IncludeDirectory From {} To {}\n", targetName, includeDirectoryCopyFrom,
                          includeDirectoryCopyTo);
                }
                create_directories(path(includeDirectoryCopyTo));
                copy(path(includeDirectoryCopyFrom), path(includeDirectoryCopyTo),
                     copy_options::update_existing | copy_options::recursive);
            }
        }
        string consumerTargetFileName;
        string consumerTargetFile = packageTargetPath + (targetName + ".hmake");
        ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);

        for (const auto &target : libraryDependencies)
        {
            if (target.preBuilt && !target.imported) // A prebuilt target is being packaged.
            {
                Json preBuiltTarget;
                ifstream(target.hmakeFilePath) >> preBuiltTarget;
                string preBuiltTargetName = preBuiltTarget["NAME"];
                copyFrom = preBuiltTarget["PATH"];
                string preBuiltPackageCopyPath = preBuiltTarget["PACKAGE_COPY_PATH"];
                string targetInstallPath = packageCopyPath + packageName + "/" + to_string(packageVariantIndex) + "/" +
                                           preBuiltTargetName + "/";
                create_directories(path(targetInstallPath));
                copy(path(copyFrom), path(targetInstallPath), copy_options::update_existing);
                ofstream(targetInstallPath + preBuiltTargetName + ".hmake")
                    << preBuiltTarget["CONSUMER_DEPENDENCIES"].dump(4);
            }
        }
    }
}

bool operator<(const ParsedTarget &lhs, const ParsedTarget &rhs)
{
    return lhs.targetFilePath < rhs.targetFilePath;
}

Builder::Builder(const set<string> &targetFilePaths, mutex &oneAndOnlyMutex) : oneAndOnlyMutex(oneAndOnlyMutex)
{
    // This will only have targets with parsedTarget() called before and libraryDependenciesParsedTargets populated if
    // any.
    vector<ParsedTarget *> moduleTargets;
    populateParsedTargetSetAndModuleTargets(targetFilePaths, moduleTargets);
    for (ParsedTarget *t : buildTreeFinal)
    {
        t->populateBuildTree();
    }
    removeRedundantNodes();

    getFinalSMFilesFromModuleParsedTargets(moduleTargets);

    for (auto it = finalSMFiles.begin(); it != finalSMFiles.end(); ++it)
    {
        SMFile &smFile = **it;
        string fileName = path(smFile.sourceNode.node->filePath).filename().string();
        if (smFile.type == SM_FILE_TYPE::HEADER_UNIT)
        {
            fileName += smFile.angle ? "_angle" : "_quote";
        }
        string finalCompileCommand = smFile.parsedTarget.compileCommand + " ";

        for (const SMFile *linkDependency : smFile.commandLineDependencies)
        {
            string linkFileName = path(linkDependency->sourceNode.node->filePath).filename().string();
            if (linkDependency->type == SM_FILE_TYPE::HEADER_UNIT)
            {
                linkFileName += linkDependency->angle ? "_angle" : "_quote";
            }
            string ifcFilePath = addQuotes(linkDependency->parsedTarget.buildCacheFilesDirPath + linkFileName + ".ifc");
            finalCompileCommand += linkDependency->getRequireFlag(ifcFilePath) + " ";
        }
        finalCompileCommand += "/c " + addQuotes(smFile.sourceNode.node->filePath) + " ";

        finalCompileCommand += smFile.getFlag(smFile.parsedTarget.buildCacheFilesDirPath + fileName);
        cout << finalCompileCommand << endl << endl;

        path responseFile = path(smFile.parsedTarget.buildCacheFilesDirPath + fileName + ".response");
        ofstream(responseFile) << finalCompileCommand;

        // compileCommand = addQuotes(compiler.bTPath.make_preferred().string()) + " ";
        string cmdCharLimitMitigateCommand = addQuotes(smFile.parsedTarget.compiler.bTPath.make_preferred().string()) +
                                             " @" + addQuotes(responseFile.generic_string());
        if (system(addQuotes(cmdCharLimitMitigateCommand).c_str()) == EXIT_FAILURE)
        {
            cout << "Command Failed. Exiting" << endl << endl;
            exit(EXIT_FAILURE);
        }
    }

    for (const ParsedTarget *target : buildTreeFinal)
    {
        target->executePreBuildCommands();
    }

    compileIterator = buildTreeFinal.rbegin();

    unsigned long actionsNeeded = 0;
    for (const ParsedTarget *parsedTarget : buildTreeFinal)
    {
        actionsNeeded += parsedTarget->buildNode.targetCache.maximumThreadsNeeded() + 1;
    }
    unsigned int numberOfBuildThreads;
    actionsNeeded > buildThreadsAllowed ? numberOfBuildThreads = buildThreadsAllowed
                                        : numberOfBuildThreads = actionsNeeded;
    vector<thread *> buildThreads;

    // numberOfBuildThreads = 12;
    while (buildThreads.size() != numberOfBuildThreads)
    {
        buildThreads.emplace_back(new thread{&Builder::buildSourceFiles, this});
    }

    for (auto t : buildThreads)
    {
        t->join();
        delete t;
    }

    for (const ParsedTarget *parsedTarget : buildTreeFinal)
    {
        parsedTarget->executePostBuildCommands();
    }

    for (ParsedTarget *parsedTarget : buildTreeFinal)
    {
        parsedTarget->copyParsedTarget();
    }
}

void Builder::populateParsedTargetSetAndModuleTargets(const set<string> &targetFilePaths,
                                                      vector<ParsedTarget *> &moduleTargets)
{
    set<string> parsedVariantsForModules;
    // parsedTargetStack will only contain pointers to parsedTargetSet elements for which libsParsed is false.
    stack<ParsedTarget *> parsedTargetsStack;

    // Calls parseTarget if target was not preset in parsedTargetSet
    auto getParsedTargetPointer = [this](const string &targetFilePath) -> tuple<ParsedTarget *, bool> {
        auto [foundIter, Ok] = parsedTargetSet.emplace(targetFilePath);
        auto &c = *foundIter;
        auto *ptr = const_cast<ParsedTarget *>(&c);
        if (Ok)
        {
            ptr->parseTarget();
        }
        return make_tuple(ptr, Ok);
    };

    auto checkVariantOfTargetForModules = [&](const string &variantFilePath) {
        if (!parsedVariantsForModules.contains(variantFilePath))
        {
            parsedVariantsForModules.emplace(variantFilePath);
            Json variantJson;
            ifstream(variantFilePath) >> variantJson;
            for (const string &targetFilePath : variantJson.at("TARGETS_WITH_MODULES").get<vector<string>>())
            {
                auto [ptr, Ok] = getParsedTargetPointer(targetFilePath);
                // We push on Stack so Regular Target Dependencies of this Module Target are also added
                parsedTargetsStack.push(ptr);
                moduleTargets.emplace_back(ptr);
            }
        }
    };

    for (const string &targetFilePath : targetFilePaths)
    {
        auto [ptr, Ok] = getParsedTargetPointer(targetFilePath);
        if (Ok)
        {
            if (ptr->isModule)
            {
                checkVariantOfTargetForModules(*(ptr->variantFilePath));
            }
            // The pointer may have already been pushed in checkVariantOfTargetForModules but may have not.
            parsedTargetsStack.push(ptr);
        }
    }

    set<TarjanNode<ParsedTarget>> tarjanNodesParsedTargets;
    while (!parsedTargetsStack.empty())
    {
        ParsedTarget *ptr = parsedTargetsStack.top();
        parsedTargetsStack.pop();
        if (!ptr->libsParsed)
        {
            for (auto &dep : ptr->libraryDependencies)
            {
                auto [ptrDep, unused] = getParsedTargetPointer(dep.path);
                ptr->libraryParsedTargetDependencies.emplace(ptrDep);
                ptrDep->buildNode.linkDependents.emplace_back(&(ptr->buildNode));
                if (ptrDep->isModule)
                {
                    cerr << ptr->targetFilePath << " Has Dependency " << ptrDep->targetFilePath
                         << " Which Has Module Source. A Target With Module Source Cannot Be A Dependency Of "
                            "Other Target"
                         << endl;
                }
                parsedTargetsStack.push(ptrDep);
            }
            ptr->libsParsed = true;
            if (!ptr->isModule)
            {
                auto [pos, success] = tarjanNodesParsedTargets.emplace(ptr);
                auto &b = *pos;
                auto &tarjanNode = const_cast<TarjanNode<ParsedTarget> &>(b);
                for (ParsedTarget *dep : ptr->libraryParsedTargetDependencies)
                {
                    auto [posDep, OkDep] = tarjanNodesParsedTargets.emplace(dep);
                    auto &a = *posDep;
                    tarjanNode.deps.emplace_back(const_cast<TarjanNode<ParsedTarget> *>(&a));
                }
                ptr->buildNode.linkDependenciesSize = ptr->libraryDependencies.size();
            }
        }
    }
    TarjanNode<ParsedTarget>::tarjanNodes = &tarjanNodesParsedTargets;
    TarjanNode<ParsedTarget>::findSCCS();

    cout << endl;

    if (TarjanNode<ParsedTarget>::cycleExists)
    {
        cout << "There is circular dependency. Exiting " << endl;
        exit(EXIT_SUCCESS);
    }
    else
    {
        for (auto it = TarjanNode<ParsedTarget>::topologicalSort.rbegin();
             it != TarjanNode<ParsedTarget>::topologicalSort.rend(); ++it)
        {
            buildTreeFinal.emplace_back(*it);
        }
    }
}

void Builder::getFinalSMFilesFromModuleParsedTargets(const vector<ParsedTarget *> &moduleTargets)
{
    // We now know all the nodes that need relink. Based on that we can know what if some modules need relink

    map<string, SourceNode *> requirePaths;
    set<TarjanNode<SMFile>> tarjanNodesSMFiles;

    for (ParsedTarget *moduleTarget : moduleTargets)
    {
        moduleTarget->populateModuleBuildTree(*this, requirePaths, tarjanNodesSMFiles);
    }

    for (auto &smFile : smFilesWithHeaderUnits)
    {
        cout << "Source File " << smFile.sourceNode.node->filePath << endl;
        cout << "Deps: " << endl;

        auto smFileTarjanNodeIt = tarjanNodesSMFiles.find(TarjanNode(&smFile));

        for (auto &dep : smFile.dependencies)
        {
            if (dep.type == SM_REQUIRE_TYPE::HEADER_UNIT)
            {

                Node node{.filePath = path(dep.sourcePath).generic_string()};
                SourceNode sourceNode{.node = &node};
                SourceNode &depSMFileSourceNode = *(const_cast<SourceNode *>(&sourceNode));
                if (auto smFileDepIt =
                        smFilesWithHeaderUnits.find(SMFile(depSMFileSourceNode, smFile.parsedTarget, dep.angle));
                    smFileDepIt != smFilesWithHeaderUnits.end())
                {
                    auto &smFileDep = const_cast<SMFile &>(*smFileDepIt);
                    smFileDep.materialize = true;
                    smFileDep.logicalName = dep.logicalName;
                    smFileDep.dependents.emplace_back(const_cast<SMFile *>(&smFile));
                    auto smFileTarjanNodeDepIt = tarjanNodesSMFiles.find(TarjanNode(&smFileDep));
                    smFileTarjanNodeIt->deps.emplace_back(const_cast<TarjanNode<SMFile> *>(&(*smFileTarjanNodeDepIt)));
                }
                else
                {
                    // TODO
                    // Currently, Include-Header-Units(Header-Units not part of source but coming from
                    // Include-Directory) are not supported, but only Source-Header-Units are supported(Header-Units
                    // also part of module-source). It can be supported here.
                    cout << dep.logicalName << " Header-Unit Could Not Be found " << endl;
                }
            }
            else
            {
                if (auto it = requirePaths.find(dep.logicalName); it == requirePaths.end())
                {
                    cout << dep.logicalName << " Module Could Not Be found " << endl;
                }
                else
                {
                    SourceNode &depSMFileSourceNode = *(const_cast<SourceNode *>(it->second));
                    auto smFileDepIt = smFilesWithHeaderUnits.find(SMFile(depSMFileSourceNode, smFile.parsedTarget));
                    auto &smFileDep = const_cast<SMFile &>(*smFileDepIt);
                    smFileDep.dependents.emplace_back(const_cast<SMFile *>(&smFile));
                    auto smFileTarjanNodeDepIt = tarjanNodesSMFiles.find(TarjanNode(&smFileDep));
                    smFileTarjanNodeIt->deps.emplace_back(const_cast<TarjanNode<SMFile> *>(&(*smFileTarjanNodeDepIt)));
                }
            }
        }
    }

    TarjanNode<SMFile>::tarjanNodes = &tarjanNodesSMFiles;
    TarjanNode<SMFile>::findSCCS();

    if (TarjanNode<SMFile>::cycleExists)
    {
        cout << "Cycle Exists in Module Files" << endl;
        exit(EXIT_FAILURE);
    }

    finalSMFiles = TarjanNode<SMFile>::topologicalSort;

    for (SMFile *smFile : finalSMFiles)
    {
        if (smFile->type == SM_FILE_TYPE::HEADER_UNIT)
        {
            smFile->parsedTarget.setSourceNodeCompilationStatus(smFile->sourceNode, false,
                                                                smFile->angle ? "_angle" : "_quote");
        }
    }

    for (auto it = finalSMFiles.begin(); it != finalSMFiles.end(); ++it)
    {
        if (!it.operator*()->materialize)
        {
            continue;
        }
        if ((*it)->sourceNode.compilationStatus == FileStatus::NEEDS_RECOMPILE)
        {
            for (SMFile *dependent : (*it)->dependents)
            {
                dependent->sourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
            }
        }
        for (SMFile *dependent : (*it)->dependents)
        {
            dependent->commandLineDependencies.emplace(*it);
            for (SMFile *dependency : (*it)->commandLineDependencies)
            {
                if (dependency->type != SM_FILE_TYPE::PRIMARY_IMPLEMENTATION &&
                    dependency->type != SM_FILE_TYPE::PARTITION_IMPLEMENTATION)
                {
                    dependent->commandLineDependencies.emplace(dependency);
                }
            }
        }
    }

    for (auto it = finalSMFiles.cbegin(); it != finalSMFiles.cend();)
    {
        if ((*it)->sourceNode.compilationStatus != FileStatus::NEEDS_RECOMPILE || !it.operator*()->materialize)
        {
            it = finalSMFiles.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Builder::removeRedundantNodes()
{
    auto it = buildTreeFinal.begin();

    while (it != buildTreeFinal.end())
    {
        (*it)->buildNode.relink ? ++it : it = buildTreeFinal.erase(it);
    }
}

set<string> Builder::getTargetFilePathsFromVariantFile(const string &fileName)
{
    Json variantFileJson;
    ifstream(fileName) >> variantFileJson;
    return variantFileJson.at("TARGETS").get<set<string>>();
}

set<string> Builder::getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage)
{
    Json projectFileJson;
    ifstream(fileName) >> projectFileJson;
    vector<string> vec;
    if (isPackage)
    {
        vector<Json> pVariantJson = projectFileJson.at("VARIANTS").get<vector<Json>>();
        for (const auto &i : pVariantJson)
        {
            vec.emplace_back(i.at("INDEX").get<string>());
        }
    }
    else
    {
        vec = projectFileJson.at("VARIANTS").get<vector<string>>();
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

void Builder::buildSourceFiles()
{
    bool loop = true;
    while (loop)
    {
        oneAndOnlyMutex.lock();
        if (newTargetCompiled)
        {
            for (auto it = buildTreeFinal.rbegin(); it != buildTreeFinal.rend(); ++it)
            {
                BuildNode &buildNode = (*it)->buildNode;
                if (newTargetCompiled && !buildNode.linkingStarted && buildNode.isCompiled &&
                    buildNode.linkDependenciesSize == buildNode.dependenciesLinked)
                {
                    ParsedTarget &target = **it;
                    buildNode.linkingStarted = true;

                    oneAndOnlyMutex.unlock();
                    unique_ptr<PostBasic> postLinkOrArchive;
                    if (target.getTargetType() == TargetType::STATIC)
                    {
                        PostBasic postLinkOrArchive1 = target.Archive();
                        postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                    }
                    else if (target.getTargetType() == TargetType::EXECUTABLE)
                    {
                        PostBasic postLinkOrArchive1 = target.Link();
                        postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                    }
                    target.pruneAndSaveBuildCache(buildNode.targetCache);
                    oneAndOnlyMutex.lock();
                    if (target.getTargetType() == TargetType::STATIC)
                    {
                        postLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor);
                    }
                    else if (target.getTargetType() == TargetType::EXECUTABLE)
                    {
                        postLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor);
                    }
                    for (auto dependents : buildNode.linkDependents)
                    {
                        ++dependents->dependenciesLinked;
                    }
                }
            }
            newTargetCompiled = false;
        }

        if (compileIterator == buildTreeFinal.rend())
        {
            oneAndOnlyMutex.unlock();
            loop = false;
            continue;
        }
        auto oldCompileIterator = compileIterator;
        BuildNode &buildNode = (*compileIterator)->buildNode;
        if (buildNode.targetCache.areFilesLeftToCompile())
        {
            SourceNode &tempSourceNode = buildNode.targetCache.getNextNodeForCompilation();
            tempSourceNode.compilationStatus = FileStatus::COMPILING;
            oneAndOnlyMutex.unlock();
            PostCompile postCompile = (*compileIterator)->Compile(tempSourceNode);
            postCompile.executePostCompileRoutineWithoutMutex(tempSourceNode);
            oneAndOnlyMutex.lock();
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor);
            tempSourceNode.compilationStatus = FileStatus::UPDATED;
            oneAndOnlyMutex.unlock();
        }
        else
        {
            ++compileIterator;
            oneAndOnlyMutex.unlock();
        }
        if (oldCompileIterator != compileIterator)
        {
            BuildNode &oldBuildNode = (*oldCompileIterator)->buildNode;
            if (oldBuildNode.targetCache.areAllFilesCompiled())
            {
                oldBuildNode.isCompiled = true;
                newTargetCompiled = true;
            }
        }
    }
}

void Builder::buildModuleFiles()
{
    bool loop = true;
    while (loop)
    {
        oneAndOnlyMutex.lock();
        if (newTargetCompiled)
        {
            for (auto it = buildTreeFinal.rbegin(); it != buildTreeFinal.rend(); ++it)
            {
                BuildNode &buildNode = (*it)->buildNode;
                if (newTargetCompiled && !buildNode.linkingStarted && buildNode.isCompiled &&
                    buildNode.linkDependenciesSize == buildNode.dependenciesLinked)
                {
                    ParsedTarget &target = **it;
                    buildNode.linkingStarted = true;

                    oneAndOnlyMutex.unlock();
                    unique_ptr<PostBasic> postLinkOrArchive;
                    if (target.getTargetType() == TargetType::STATIC)
                    {
                        PostBasic postLinkOrArchive1 = target.Archive();
                        postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                    }
                    else if (target.getTargetType() == TargetType::EXECUTABLE)
                    {
                        PostBasic postLinkOrArchive1 = target.Link();
                        postLinkOrArchive = make_unique<PostBasic>(postLinkOrArchive1);
                    }
                    target.pruneAndSaveBuildCache(buildNode.targetCache);
                    oneAndOnlyMutex.lock();
                    if (target.getTargetType() == TargetType::STATIC)
                    {
                        postLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor);
                    }
                    else if (target.getTargetType() == TargetType::EXECUTABLE)
                    {
                        postLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor);
                    }
                    for (auto dependents : buildNode.linkDependents)
                    {
                        ++dependents->dependenciesLinked;
                    }
                }
            }
            newTargetCompiled = false;
        }

        if (compileIterator == buildTreeFinal.rend())
        {
            oneAndOnlyMutex.unlock();
            loop = false;
        }
        auto oldCompileIterator = compileIterator;
        BuildNode &buildNode = (*compileIterator)->buildNode;
        if (buildNode.targetCache.areFilesLeftToCompile())
        {
            SourceNode &tempSourceNode = buildNode.targetCache.getNextNodeForCompilation();
            tempSourceNode.compilationStatus = FileStatus::COMPILING;
            oneAndOnlyMutex.unlock();
            PostCompile postCompile = (*compileIterator)->Compile(tempSourceNode);
            postCompile.executePostCompileRoutineWithoutMutex(tempSourceNode);
            oneAndOnlyMutex.lock();
            postCompile.executePrintRoutine(settings.pcSettings.compileCommandColor);
            tempSourceNode.compilationStatus = FileStatus::UPDATED;
            oneAndOnlyMutex.unlock();
        }
        else
        {
            ++compileIterator;
            oneAndOnlyMutex.unlock();
        }
        if (oldCompileIterator != compileIterator)
        {
            BuildNode &oldBuildNode = (*oldCompileIterator)->buildNode;
            if (oldBuildNode.targetCache.areAllFilesCompiled())
            {
                oldBuildNode.isCompiled = true;
                newTargetCompiled = true;
            }
        }
    }
}

void Builder::copyPackage(const path &packageFilePath)
{
    Json packageFileJson;
    ifstream(packageFilePath) >> packageFileJson;
    Json variants = packageFileJson.at("VARIANTS").get<Json>();
    path packageCopyPath;
    bool packageCopy = packageFileJson.at("PACKAGE_COPY").get<bool>();
    if (packageCopy)
    {
        string packageName = packageFileJson.at("NAME").get<string>();
        string version = packageFileJson.at("VERSION").get<string>();
        packageCopyPath = packageFileJson.at("PACKAGE_COPY_PATH").get<string>();
        packageCopyPath /= packageName;
        if (packageFileJson.at("CACHE_INCLUDES").get<bool>())
        {
            Json commonFileJson;
            ifstream(current_path() / "Common.hmake") >> commonFileJson;
            Json commonJson;
            for (auto &i : commonFileJson)
            {
                Json commonJsonObject;
                int commonIndex = i.at("INDEX").get<int>();
                commonJsonObject["INDEX"] = commonIndex;
                commonJsonObject["VARIANTS_INDICES"] = i.at("VARIANTS_INDICES").get<Json>();
                commonJson.emplace_back(commonJsonObject);
                path commonIncludePath = packageCopyPath / "Common" / path(to_string(commonIndex)) / "include";
                create_directories(commonIncludePath);
                path dirCopyFrom = i.at("PATH").get<string>();
                copy(dirCopyFrom, commonIncludePath, copy_options::update_existing | copy_options::recursive);
            }
        }
        path consumePackageFilePath = packageCopyPath / "cpackage.hmake";
        Json consumePackageFilePathJson;
        consumePackageFilePathJson["NAME"] = packageName;
        consumePackageFilePathJson["VERSION"] = version;
        consumePackageFilePathJson["VARIANTS"] = variants;
        create_directories(packageCopyPath);
        ofstream(consumePackageFilePath) << consumePackageFilePathJson.dump(4);
    }
}

string getReducedPath(const string &subjectPath, const PathPrint &pathPrint)
{
    assert(pathPrint.printLevel != PathPrintLevel::NO &&
           "HMake Internal Error. Function getReducedPath() should not had been called if PrintLevel is NO.");

    if (pathPrint.printLevel == PathPrintLevel::FULL)
    {
        return subjectPath;
    }

    auto nthOccurrence = [](const std::string &str, const std::string &findMe, unsigned int nth) -> int {
        unsigned long count = 0;

        for (int i = 0; i < str.size(); ++i)
        {
            if (str.size() > i + findMe.size())
            {
                bool found = true;
                for (unsigned long j = 0; j < findMe.size(); ++j)
                {
                    if (str[i + j] != findMe[j])
                    {
                        found = false;
                        break;
                    }
                }
                if (found)
                {
                    ++count;
                    if (count == nth)
                    {
                        return i;
                    }
                }
            }
            else
            {
                break;
            }
        }
        return -1;
    };

    auto countSubstring = [](const string &str, const string &sub) -> unsigned long {
        if (sub.length() == 0)
            return 0;
        int count = 0;
        for (int offset = (int)str.find(sub); offset != std::string::npos;
             offset = (int)str.find(sub, offset + sub.length()))
        {
            ++count;
        }
        return count;
    };

    string str = subjectPath;
    unsigned int finalDepth = pathPrint.depth;
    if (pathPrint.isDirectory)
    {
        finalDepth += 1;
    }
    bool toolOnWindows = false;
#ifdef _WIN32
    if (pathPrint.isTool)
    {
        toolOnWindows = true;
    }
#endif
    unsigned long count = countSubstring(str, toolOnWindows ? "\\" : "/");
    if (finalDepth >= count)
    {
        return str;
    }
    size_t index = nthOccurrence(str, toolOnWindows ? "\\" : "/", count - finalDepth);
    return str.substr(index + 1, str.size() - 1);
}
