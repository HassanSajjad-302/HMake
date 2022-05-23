
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
    std::unique_ptr, std::make_unique, fmt::print, std::filesystem::file_time_type, std::filesystem::last_write_time;

void from_json(const Json &j, SourceDirectory &sourceDirectory)
{
    sourceDirectory.sourceDirectory = Directory(j.at("PATH").get<string>());
    sourceDirectory.regex = j.at("REGEX_STRING").get<string>();
}

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

ParsedTarget::ParsedTarget(const string &targetFilePath_, vector<string> dependents)
{
    targetFilePath = targetFilePath_;
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
    sourceFiles = targetFileJson.at("SOURCE_FILES").get<vector<string>>();
    sourceDirectories = targetFileJson.at("SOURCE_DIRECTORIES").get<vector<SourceDirectory>>();

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
    checkForCircularDependencies(dependents);
    dependents.emplace_back(actualOutputName);

    // TODO
    // Big todo: Currently totally ignoring prebuild libraries
    for (const auto &i : libraryDependencies)
    {
        if (!i.preBuilt)
        {
            libraryDependenciesBTargets.emplace_back(i.path, dependents);
        }
    }
}

void ParsedTarget::checkForCircularDependencies(const vector<string> &dependents)
{
    if (find(dependents.begin(), dependents.end(), actualOutputName) != end(dependents))
    {
        cerr << "Circular Dependency Detected" << endl;
        exit(EXIT_FAILURE);
    }
}

void ParsedTarget::executePreBuildCommands()
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
    // TODO
    for (auto &i : libraryDependenciesBTargets)
    {
        i.executePreBuildCommands();
    }
}

void ParsedTarget::executePostBuildCommands()
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
    for (auto &i : libraryDependenciesBTargets)
    {
        i.executePreBuildCommands();
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

    compileCommand = addQuotes(compiler.bTPath.make_preferred().string()) + " ";
    if (ccpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        compileCommandFirstHalf += getReducedPath(compiler.bTPath.make_preferred().string(), ccpSettings.tool) + " ";
    }

    compileCommand += environment.compilerFlags + " ";
    if (ccpSettings.environmentCompilerFlags)
    {
        compileCommandFirstHalf += environment.compilerFlags + " ";
    }
    compileCommand += compilerFlags + " ";
    if (ccpSettings.compilerFlags)
    {
        compileCommandFirstHalf += compilerFlags + " ";
    }
    compileCommand += compilerTransitiveFlags + " ";
    if (ccpSettings.compilerTransitiveFlags)
    {
        compileCommandFirstHalf += compilerTransitiveFlags + " ";
    }

    for (const auto &i : compileDefinitions)
    {
        compileCommand += (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
        if (ccpSettings.compileDefinitions)
        {
            compileCommandFirstHalf +=
                (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
        }
    }

    for (const auto &i : includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.path) + " ");
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            compileCommandFirstHalf.append(getIncludeFlag() +
                                           getReducedPath(i.path, ccpSettings.projectIncludeDirectories) + " ");
        }
    }

    for (const auto &i : environment.includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(i.directoryPath.generic_string()) + " ");
        if (ccpSettings.environmentIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            compileCommandFirstHalf.append(
                getIncludeFlag() +
                getReducedPath(i.directoryPath.generic_string(), ccpSettings.environmentIncludeDirectories) + " ");
        }
    }
}

void ParsedTarget::parseSourceDirectoriesAndFinalizeSourceFiles()
{
    for (const auto &i : sourceDirectories)
    {
        for (const auto &j : directory_iterator(i.sourceDirectory.directoryPath))
        {
            if (regex_match(j.path().filename().string(), regex(i.regex)))
            {
                sourceFiles.emplace_back(j.path().string());
            }
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
}

SourceNode &BTargetCache::addNodeInSourceFileDependencies(const string &str)
{
    SourceNode sourceNode{.node = Node::getNodeFromString(str)};
    auto [pos, ok] = sourceFileDependencies.insert(sourceNode);
    if (ok)
    {
        pos->presentInCache = false;
    }
    else
    {
        pos->presentInCache = true;
    }
    pos->presentInSource = true;
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

int BTargetCache::maximumThreadsNeeded() const
{
    int numberOfOutdatedFiles = 0;
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
PostLinkOrArchive::PostLinkOrArchive(string commandFirstHalf, string printCommandFirstHalf,
                                     const string &buildCacheFilesDirPath, const string &fileName,
                                     const PathPrint &pathPrint, bool isTarget_)
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

void PostLinkOrArchive::executePrintRoutine(uint32_t color) const
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
      PostLinkOrArchive(std::move(commandFirstHalf), std::move(printCommandFirstHalf), buildCacheFilesDirPath, fileName,
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
            size_t pos = iter->find_first_not_of(includeFileNote);
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

    string headerDepFilecontents = file_to_string(parsedTarget.buildCacheFilesDirPath +
                                                  path(sourceNode.node->filePath).filename().string() + ".d");
    vector<string> headerDeps = split(headerDepFilecontents, "\n");

    // First 2 lines are skipped as these are .o and .cpp file. While last line is an empty line
    for (auto iter = headerDeps.begin() + 2; iter != headerDeps.end() - 1; ++iter)
    {
        size_t pos = iter->find_first_not_of(" ");
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

bool std::less<SourceNode>::operator()(const SourceNode &lhs, const SourceNode &rhs) const
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

void ParsedTarget::populateBuildTree(vector<BuildNode> &localBuildTree)
{
    for (auto it = localBuildTree.begin(); it != localBuildTree.end(); ++it)
    {
        if (it->target->outputDirectory == outputDirectory && it->target->actualOutputName == actualOutputName)
        {
            // If it exists before in the graph, move it to the end
            rotate(it, it + 1, localBuildTree.end());
            return;
        }
    }

    setCompileCommand();
    parseSourceDirectoriesAndFinalizeSourceFiles();

    BTargetCache bTargetCache;
    if (exists(path(buildCacheFilesDirPath) / (targetName + ".cache")))
    {
        Json targetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
        bTargetCache = targetCacheJson;
    }

    if (!exists(path(buildCacheFilesDirPath)))
    {
        create_directory(buildCacheFilesDirPath);
        addAllSourceFilesInBuildNode(bTargetCache);
    }
    else if (bTargetCache.compileCommand != compileCommand)
    {
        addAllSourceFilesInBuildNode(bTargetCache);
    }
    else
    {
        for (const auto &sourceFile : sourceFiles)
        {
            SourceNode &permSourceNode = bTargetCache.addNodeInSourceFileDependencies(sourceFile);
            permSourceNode.compilationStatus = FileStatus::UPDATED;

            path objectFilePath =
                path(buildCacheFilesDirPath + path(permSourceNode.node->filePath).filename().string() + ".o");

            if (!exists(objectFilePath))
            {
                permSourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
                continue;
            }
            file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
            permSourceNode.node->checkIfNotUpdatedAndUpdate();
            if (permSourceNode.node->lastUpdateTime > objectFileLastEditTime)
            {
                permSourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
            }
            else
            {
                for (auto &d : permSourceNode.headerDependencies)
                {
                    d->checkIfNotUpdatedAndUpdate();
                    if (d->lastUpdateTime > objectFileLastEditTime)
                    {
                        permSourceNode.compilationStatus = FileStatus::NEEDS_RECOMPILE;
                        break;
                    }
                }
            }
        }
    }

    // Removing unused object-files from buildCacheFilesDirPath as they later affect linker command with *.o option.
    set<string> tmpObjectFiles;
    for (const auto &src : bTargetCache.sourceFileDependencies)
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

    if (bTargetCache.maximumThreadsNeeded() > 0)
    {
        bTargetCache.compileCommand = compileCommand;
        relink = true;
        BuildNode buildNode;
        buildNode.target = this;
        buildNode.targetCache = bTargetCache;
        localBuildTree.emplace_back(buildNode);
    }
    else
    {
        auto needsRelinkBeforePopularizingBuildTree = [&]() -> bool {
            for (const auto &src : bTargetCache.sourceFileDependencies)
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

        relink = needsRelinkBeforePopularizingBuildTree();
        BuildNode buildNode;
        buildNode.target = this;
        buildNode.targetCache = bTargetCache;
        localBuildTree.emplace_back(buildNode);
    }
    // buildNode.target above is pointing to this. this is either a ParsedTarget in Builder::Builder() ParsedTargets or
    // a vector element of ParsedTarget::libraryDependenciesPTargets of one of these. Because these vector do not resize
    // after construction, it is safe to use. Otherwise, a vector<shared_ptr<ParsedTarget>>> can be used instead
    // for libraryDependeciesPTargets

    for (auto &i : libraryDependenciesBTargets)
    {
        i.populateBuildTree(localBuildTree);
    }
}

string ParsedTarget::getInfrastructureFlags(const SourceNode &sourceNode)
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        return "/showIncludes /c ";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        // Will like to use -MD but not using it currently because sometimes it prints 2 header deps in one line and
        // no space in them so no way of knowing whether this is a space in path or 2 different headers. Which then
        // breaks when last_write_time is checked for that path.
        return "-c -MMD ";
    }
    return "";
}

string ParsedTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode)
{
    CompileCommandPrintSettings ccpSettings = settings.ccpSettings;
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string command;
    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += getInfrastructureFlags(sourceNode);
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

    string finalCompileCommand = compileCommand + addQuotes(sourceNode.node->filePath) + " ";

    finalCompileCommand += getInfrastructureFlags(sourceNode);
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand += "/Fo" + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }
    else
    {
        finalCompileCommand += "-o " + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }

    cout << finalCompileCommand << endl;
    PostCompile postCompile{*this,
                            finalCompileCommand,
                            compileCommandFirstHalf + getCompileCommandPrintSecondPart(sourceNode),
                            buildCacheFilesDirPath,
                            compileFileName,
                            settings.ccpSettings.outputAndErrorFiles};
    return postCompile;
}

PostLinkOrArchive ParsedTarget::Archive()
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
    cout << archiveCommand << endl;
    PostLinkOrArchive postLinkOrArchive(archiveCommand, archivePrintCommand, buildCacheFilesDirPath, targetName,
                                        acpSettings.outputAndErrorFiles, true);
    return postLinkOrArchive;
}

PostLinkOrArchive ParsedTarget::Link()
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

    for (auto &i : libraryDependenciesBTargets)
    {
        linkCommand.append(getLinkFlag(i.outputDirectory, i.outputName));
        linkPrintCommand.append(getLinkFlagPrint(i.outputDirectory, i.outputName, lcpSettings.libraryDependencies));
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

    cout << linkCommand << endl;
    PostLinkOrArchive postLinkOrArchive(linkCommand, linkPrintCommand, buildCacheFilesDirPath, targetName,
                                        lcpSettings.outputAndErrorFiles, true);
    return postLinkOrArchive;
}

TargetType ParsedTarget::getTargetType()
{
    return targetType;
}

void ParsedTarget::pruneAndSaveBuildCache(BTargetCache &bTargetCache)
{
    // Prune the src_deps which were read from the older cache but are no longer the src_deps.
    const auto count = std::erase_if(bTargetCache.sourceFileDependencies, [](const auto &item) {
        // auto const& [key, value] = item;
        return !item.presentInSource;
    });
    Json cacheFileJson = bTargetCache;
    ofstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) << cacheFileJson.dump(4);
}

bool ParsedTarget::needsRelink() const
{
    if (relink)
    {
        return true;
    }
    for (const auto &i : libraryDependenciesBTargets)
    {
        if (i.needsRelink())
        {
            return true;
        }
    }
    return false;
}
bool ParsedTarget::hasDependency(ParsedTarget *parsedTarget)
{
    for (const auto &dep : libraryDependenciesBTargets)
    {
        if (dep.targetFilePath == parsedTarget->targetFilePath)
        {
            return true;
        }
    }
    return false;
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

using std::this_thread::get_id;
void Builder::actuallyBuild()
{
    oneAndOnlyMutex.lock();
    if (newTargetCompiled)
    {
        for (auto it = buildTree.crbegin(); it != buildTree.crend(); ++it)
        {
            auto &buildNode = const_cast<BuildNode &>(*it);
            if (newTargetCompiled && !buildNode.isLinked && buildNode.isCompiled &&
                buildNode.linkDependenciesSize == buildNode.dependenciesLinked)
            {
                buildNode.isLinked = true;

                oneAndOnlyMutex.unlock();
                unique_ptr<PostLinkOrArchive> postLinkOrArchive;
                ParsedTarget *target = buildNode.target;
                if (target->getTargetType() == TargetType::STATIC)
                {
                    PostLinkOrArchive postLinkOrArchive1 = target->Archive();
                    postLinkOrArchive = make_unique<PostLinkOrArchive>(postLinkOrArchive1);
                }
                else if (target->getTargetType() == TargetType::EXECUTABLE)
                {
                    PostLinkOrArchive postLinkOrArchive1 = target->Link();
                    postLinkOrArchive = make_unique<PostLinkOrArchive>(postLinkOrArchive1);
                }
                target->pruneAndSaveBuildCache(buildNode.targetCache);
                oneAndOnlyMutex.lock();
                if (target->getTargetType() == TargetType::STATIC)
                {
                    postLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor);
                }
                else if (target->getTargetType() == TargetType::EXECUTABLE)
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

    if (compileIterator == buildTree.rend())
    {
        oneAndOnlyMutex.unlock();
        return;
    }
    auto oldCompileIterator = compileIterator;
    if (compileIterator->targetCache.areFilesLeftToCompile())
    {
        SourceNode &tempSourceNode = compileIterator->targetCache.getNextNodeForCompilation();
        tempSourceNode.compilationStatus = FileStatus::COMPILING;
        ParsedTarget *target = compileIterator->target;
        oneAndOnlyMutex.unlock();
        PostCompile postCompile = target->Compile(tempSourceNode);
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
        if (oldCompileIterator->targetCache.areAllFilesCompiled())
        {
            oldCompileIterator->isCompiled = true;
            newTargetCompiled = true;
        }
    }
    actuallyBuild();
}

void Builder::populateBuildNodeDependents()
{
    if (!buildTree.empty())
    {
        for (size_t i = 0; i < buildTree.size() - 1; ++i)
        {
            for (size_t j = i + 1; j < buildTree.size(); ++j)
            {
                if (buildTree[i].target->hasDependency(buildTree[j].target))
                {
                    buildTree[j].linkDependents.emplace_back(&buildTree[i]);
                }
            }
            buildTree[i].linkDependenciesSize = buildTree[i].target->parsedTargetDependenciesSize();
        }
    }
}

Builder::Builder(const vector<string> &targetFilePaths, mutex &oneAndOnlyMutex) : oneAndOnlyMutex(oneAndOnlyMutex)
{
    vector<ParsedTarget> parsedTargets;
    for (auto &t : targetFilePaths)
    {
        parsedTargets.emplace_back(t);
    }
    for (auto &t : parsedTargets)
    {
        t.executePreBuildCommands();
    }

    for (auto &t : parsedTargets)
    {
        t.populateBuildTree(buildTree);
    }
    removeRedundantNodes(buildTree);
    populateBuildNodeDependents();

    compileIterator = buildTree.rbegin();
    // linkIterator = buildTree.rbegin();

    std::size_t actionsNeeded = 0;
    for (const auto &buildNode : buildTree)
    {
        actionsNeeded += buildNode.targetCache.maximumThreadsNeeded() + 1;
    }
    size_t numberOfBuildThreads;
    actionsNeeded > buildThreadsAllowed ? numberOfBuildThreads = buildThreadsAllowed
                                        : numberOfBuildThreads = actionsNeeded;
    vector<thread *> buildThreads;

    // numberOfBuildThreads = 12;
    while (buildThreads.size() != numberOfBuildThreads)
    {
        buildThreads.emplace_back(new thread{&Builder::actuallyBuild, this});
    }

    for (auto t : buildThreads)
    {
        t->join();
        delete t;
    }

    for (auto &t : parsedTargets)
    {
        t.executePostBuildCommands();
    }

    for (auto &p : parsedTargets)
    {
        p.copyParsedTarget();
    }
}

vector<string> Builder::getTargetFilePathsFromVariantFile(const string &fileName)
{
    Json variantFileJson;
    ifstream(fileName) >> variantFileJson;
    return variantFileJson.at("TARGETS").get<vector<string>>();
}

vector<string> Builder::getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage)
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
    vector<string> targetFilePaths;
    for (auto &i : vec)
    {
        vector<string> variantTargets =
            getTargetFilePathsFromVariantFile(i + (isPackage ? "/packageVariant.hmake" : "/projectVariant.hmake"));
        targetFilePaths.insert(targetFilePaths.end(), variantTargets.begin(), variantTargets.end());
    }
    return targetFilePaths;
}

void Builder::removeRedundantNodes(vector<BuildNode> &buildTree)
{
    auto it = buildTree.begin();

    while (it != buildTree.end())
    {

        if (!it->target->needsRelink())
        {

            it = buildTree.erase(it);
        }
        else
            ++it;
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

    auto nthOccurrence = [](const std::string &str, const std::string &findMe, size_t nth) -> size_t {
        size_t pos = 0;
        int count = 0;

        for (int i = 0; i < str.size(); ++i)
        {
            if (str.size() > i + findMe.size())
            {
                bool found = true;
                for (int j = 0; j < findMe.size(); ++j)
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

    auto countSubstring = [](const std::string &str, const std::string &sub) -> int {
        if (sub.length() == 0)
            return 0;
        int count = 0;
        for (size_t offset = str.find(sub); offset != std::string::npos; offset = str.find(sub, offset + sub.length()))
        {
            ++count;
        }
        return count;
    };

    string str = subjectPath;
    int finalDepth = pathPrint.depth;
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
    size_t count = countSubstring(str, toolOnWindows ? "\\" : "/");
    if (finalDepth >= count)
    {
        return str;
    }
    size_t index = nthOccurrence(str, toolOnWindows ? "\\" : "/", count - finalDepth);
    return str.substr(index + 1, str.size() - 1);
}
