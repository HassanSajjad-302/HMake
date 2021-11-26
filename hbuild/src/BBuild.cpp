
#include "BBuild.hpp"
#include "fstream"
#include "iostream"
#include "regex"

using std::ifstream, std::ofstream, std::filesystem::exists, std::filesystem::copy_options, std::runtime_error,
    std::cout, std::endl, std::to_string, std::filesystem::create_directory, std::filesystem::directory_iterator,
    std::regex, std::filesystem::current_path, std::cerr, std::make_shared, std::make_pair;

void from_json(const Json &j, BTargetType &targetType)
{
    if (j == "EXECUTABLE")
    {
        targetType = BTargetType::EXECUTABLE;
    }
    else if (j == "STATIC")
    {
        targetType = BTargetType::STATIC;
    }
    else if (j == "SHARED")
    {
        targetType = BTargetType::SHARED;
    }
    else if (j == "PLIBRARY_STATIC")
    {
        targetType = BTargetType::PLIBRARY_STATIC;
    }
    else
    {
        targetType = BTargetType::PLIBRARY_SHARED;
    }
}

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
        if (j.contains("IMPORTED"))
        {
            p.imported = j.at("IMPORTED").get<bool>();
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

BTarget::BTarget(const string &targetFilePath)
{
    string fileName = path(targetFilePath).filename().string();
    targetName = fileName.substr(0, fileName.size() - string(".hmake").size());
    Json targetFileJson;
    ifstream(targetFilePath) >> targetFileJson;

    targetType = targetFileJson.at("TARGET_TYPE").get<BTargetType>();
    if (targetType != BTargetType::EXECUTABLE && targetType != BTargetType::STATIC && targetType != BTargetType::SHARED)
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
            packageTargetPath = packageCopyPath / path(packageName) / path(to_string(packageVariantIndex)) / targetName;
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
    if (targetType == BTargetType::STATIC)
    {
        staticLibraryTool = targetFileJson.at("ARCHIVER").get<Archiver>();
    }
    Json environmentJson = targetFileJson.at("ENVIRONMENT").get<Json>();
    environmentIncludeDirectories = environmentJson.at("INCLUDE_DIRECTORIES").get<vector<string>>();
    environmentLibraryDirectories = environmentJson.at("LIBRARY_DIRECTORIES").get<vector<string>>();
    environmentCompilerFlags = environmentJson.at("COMPILER_FLAGS").get<string>();
    environmentLinkerFlags = environmentJson.at("LINKER_FLAGS").get<string>();
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

    buildCacheFilesDirPath = (path(targetFilePath).parent_path() / ("Cache_Build_Files")).string();
    if (copyPackage)
    {
        consumerDependenciesJson = targetFileJson.at("CONSUMER_DEPENDENCIES").get<Json>();
    }
    // Parsing finished
}

void BTarget::setActualOutputName()
{
    assert(targetType != BTargetType::PLIBRARY_SHARED);
    assert(targetType != BTargetType::PLIBRARY_STATIC);

    // TODO
    // This should be OS dependent instead of tools dependent or should it be not?
    // Add target OS target property
    if (targetType == BTargetType::EXECUTABLE)
    {
        actualOutputName = outputName + (compiler.bTFamily == BTFamily::MSVC ? ".exe" : "");
    }
    else if (targetType == BTargetType::STATIC)
    {
        actualOutputName = compiler.bTFamily == BTFamily::MSVC ? "" : "lib";
        actualOutputName += outputName;
        actualOutputName = compiler.bTFamily == BTFamily::MSVC ? ".lib" : ".a";
    }
    else
    {
    }
}

void BTarget::executePreBuildCommands()
{
    // Execute the prebuild commands serially of this target and its dependencies.
    if (!preBuildCustomCommands.empty())
    {
        cout << "Executing PRE_BUILD_CUSTOM_COMMANDS" << endl;
    }
    for (auto &i : preBuildCustomCommands)
    {
        cout << i << endl;
        if (int a = system(i.c_str()); a != EXIT_SUCCESS)
        {
            exit(a);
        }
    }
    for (const auto &i : libraryDependencies)
    {
        if (!i.preBuilt)
        {
            BTarget buildTarget(i.path);
            buildTarget.executePreBuildCommands();
        }
        else
        {
            // TODO
            // Currently ignore prebuilt libs for time being
            string dir = path(i.path).parent_path().string();
            string libName = path(i.path).filename().string();
            libName.erase(0, 3);
            libName.erase(libName.find('.'), 2);

            if (!i.imported)
            {
                BPTarget(i.hmakeFilePath, i.path);
            }
        }
    }
    havePreBuildCommandsExecuted = true;
}

bool BTarget::checkIfAlreadyBuiltAndCreatNecessaryDirectories()
{
    if (exists(path(buildCacheFilesDirPath) / (targetName + ".cache")) &&
        exists(path(outputDirectory) / actualOutputName))
    {
        return true;
    }
    else
    {
        if (!exists(outputDirectory))
        {
            create_directory(outputDirectory);
        }
        if (!exists(buildCacheFilesDirPath))
        {
            create_directory(buildCacheFilesDirPath);
        }
        return false;
    }
}

void BTarget::setCompileCommand()
{
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

    string projectIncludeDirectoriesFlags;
    for (const auto &i : includeDirectories)
    {
        projectIncludeDirectoriesFlags.append(getIncludeFlag() + addQuotes(i.path) + " ");
    }
    string totalIncludeDirectoriesFlags = projectIncludeDirectoriesFlags;
    for (const auto &i : environmentIncludeDirectories)
    {
        totalIncludeDirectoriesFlags.append(getIncludeFlag() + addQuotes(i) + " ");
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

    string projectLibraryDirectoriesFlags;
    for (const auto &i : libraryDirectories)
    {
        projectLibraryDirectoriesFlags.append(getLibraryDirectoryFlag() + addQuotes(i) + " ");
    }
    string totalLibraryDirectoriesFlags = projectLibraryDirectoriesFlags;
    for (const auto &i : environmentLibraryDirectories)
    {
        totalLibraryDirectoriesFlags.append(getLibraryDirectoryFlag() + addQuotes(i) + " ");
    }

    auto getLinkFlag = [this](const std::string &libraryPath, const std::string &libraryName) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return addQuotes((path(libraryPath) / path(libraryName)).string() + " ");
        }
        else
        {
            return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
        }
    };

    string libraryDependenciesFlags;
    for (const auto &i : libraryDependencies)
    {
        if (!i.preBuilt)
        {
            BTarget buildTarget(i.path);
            libraryDependenciesFlags.append(getLinkFlag(buildTarget.outputDirectory, buildTarget.outputName));
        }
        else
        {
            string dir = path(i.path).parent_path().string();
            string libName = path(i.path).filename().string();
            libName.erase(0, 3);
            libName.erase(libName.find('.'), 2);
            libraryDependenciesFlags.append(getLinkFlag(dir, libName));

            if (!i.imported)
            {
                BPTarget(i.hmakeFilePath, i.path);
            }
        }
    }

    string compileDefinitionsString;
    for (const auto &i : compileDefinitions)
    {
        compileDefinitionsString += (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
    }

    string totalCompilerFlags = environmentCompilerFlags + " " + compilerFlags + " " + compilerTransitiveFlags;
    string totalLinkerFlags = environmentLinkerFlags + " " + linkerFlags + " " + linkerTransitiveFlags;

    compileCommand += "\"" + addQuotes(compiler.bTPath.make_preferred().string()) + " ";
    compileCommand += totalCompilerFlags + " ";
    compileCommand += compileDefinitionsString + " ";
    compileCommand += totalIncludeDirectoriesFlags;
}

void BTarget::parseSourceDirectoriesAndFinalizeSourceFiles()
{
    for (const auto &i : sourceDirectories)
    {
        for (const auto &j : directory_iterator(i.sourceDirectory.directoryPath))
        {
            if (regex_match(j.path().filename().string(), regex(i.regex)))
            {
                sourceFiles.push_back(j.path().string());
            }
        }
    }
}

void to_json(Json &j, const BTargetCache &bTargetCache)
{
    j["COMPILE_COMMAND"] = bTargetCache.compileCommand;
    // j["DEPENDENCIES"] = bTargetCache.sourceFileDependencies;
}

void from_json(const Json &j, BTargetCache &bTargetCache)
{
    bTargetCache.compileCommand = j["COMPILE_COMMAND"];
    // When we read dependencies, we have to mark for all the file fileExists = true;
    //  bTargetCache.sourceFileDependencies = j["DEPENDENCIES"];
}

bool Node::operator<(const Node &node) const
{
    return filePath < node.filePath;
}

bool SourceNode::operator<(const SourceNode &sourceNode) const
{
    return *node < *sourceNode.node;
}

SourceNode *BTarget::getSourceNode(SourceNode *sourceNode)
{
    if (auto [pos, ok] = targetCache.sourceFileDependencies.insert({sourceNode, set<Node *>{}}); !ok)
    {
        // Means it already exists
        const auto &[key, val] = *pos;
        return key;
    }
    else
    {
        return sourceNode;
    }
}

Node *BTarget::getNode(Node *node)
{
    if (auto [pos, ok] = allFiles.insert(node); !ok)
    {
        // Means it already exists
        return *pos;
    }
    else
    {
        return node;
    }
}

void BTarget::build()
{
    if (!havePreBuildCommandsExecuted)
    {
        executePreBuildCommands();
    }

    setActualOutputName();
    setCompileCommand();
    parseSourceDirectoriesAndFinalizeSourceFiles();
    bool isAlreadyBuilt = checkIfAlreadyBuiltAndCreatNecessaryDirectories();

    if (isAlreadyBuilt)
    {
        lastOutputTouchTime = last_write_time((path(outputDirectory) / actualOutputName));
        Json targetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (targetName + ".cache")) >> targetCacheJson;
        BTargetCache bTargetCache = targetCacheJson;

        if (bTargetCache.compileCommand != compileCommand)
        {
        }
        else
        {
            map<SourceNode *, set<Node *>> &srcDepsRef = bTargetCache.sourceFileDependencies;
            for (const auto &sourceFile : sourceFiles)
            {
                Node *permNode;
                {
                    Node *tempNode = new Node{.filePath = sourceFile};
                    permNode = getNode(tempNode);
                    if (tempNode != permNode)
                    {
                        delete tempNode;
                    }
                }

                SourceNode *permSourceNode;
                {
                    SourceNode *tempSourceNode = new SourceNode{.node = permNode};
                    permSourceNode = getSourceNode(tempSourceNode);
                    if (tempSourceNode != permSourceNode)
                    {
                        delete tempSourceNode;
                    }
                }

                if (!permSourceNode->node->isUpdated)
                {
                    permSourceNode->node->lastUpdateTime = last_write_time(path(permSourceNode->node->filePath));
                    permSourceNode->node->isUpdated = true;
                }
                if (permSourceNode->node->lastUpdateTime > lastOutputTouchTime)
                {
                    permSourceNode->needsRecompile = true;
                }
                else
                {
                    for (auto &d : bTargetCache.sourceFileDependencies.at(permSourceNode))
                    {
                        if (d->lastUpdateTime > lastOutputTouchTime)
                        {
                            permSourceNode->needsRecompile = true;
                            break;
                        }
                    }
                    permSourceNode->needsRecompile = false;
                }
                permSourceNode->fileExists = true;
            }
        }
    }
    else
    {
    }

    for (const auto &i : sourceFiles)
    {
        compileCommand += compiler.bTFamily == BTFamily::MSVC ? " /c " : " -c ";
        compileCommand += i;
        compileCommand += compiler.bTFamily == BTFamily::MSVC ? " /Fo" : " -o ";
        compileCommand += addQuotes((path(buildCacheFilesDirPath) / path(i).filename()).string()) + ".o" + "\"";

        cout << compileCommand << endl;
        int code = system(compileCommand.c_str());
        if (code != EXIT_SUCCESS)
        {
            exit(code);
        }
    }

    if (targetType == BTargetType::STATIC)
    {
        cout << "Archiving" << endl;
        string archiveCommand = "\"";
        if (staticLibraryTool.bTFamily == BTFamily::MSVC)
        {
            archiveCommand += staticLibraryTool.bTPath.make_preferred().string() +
                              " /OUT:" + addQuotes((path(outputDirectory) / actualOutputName).string()) + " " +
                              addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ");
        }
        else if (staticLibraryTool.bTFamily == BTFamily::GCC)
        {
            archiveCommand = staticLibraryTool.bTPath.make_preferred().string() + " rcs " +
                             addQuotes((path(outputDirectory) / actualOutputName).string()) + " " +
                             addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ");
        }

        archiveCommand += "\"";
        cout << archiveCommand << endl;
        int code = system(archiveCommand.c_str());
        if (code != EXIT_SUCCESS)
        {
            exit(code);
        }
    }
    else
    {
        /*
        cout << "Linking" << endl;
        string linkerCommand;
        if (targetType == BTargetType::EXECUTABLE)
        {
            linkerCommand = "\"" + addQuotes(linker.bTPath.make_preferred().string()) + " " + totalLinkerFlags + " " +
                            addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ") + " " +
                            libraryDependenciesFlags + totalLibraryDirectoriesFlags;
            linkerCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
            linkerCommand += addQuotes((path(outputDirectory) / actualOutputName).string()) + "\"";
        }
        else
        {
        }

        cout << linkerCommand << endl;
        int code = system(linkerCommand.c_str());
        if (code != EXIT_SUCCESS)
        {
            exit(code);
        }
         */
    }

    cout << "Build Complete" << endl;

    if (copyPackage)
    {
        cout << "Copying Started" << endl;
        path copyFrom;
        path copyTo = packageTargetPath / "";
        if (targetType != BTargetType::SHARED)
        {
            copyFrom = path(outputDirectory) / actualOutputName;
        }
        else
        {
            // Shared Libraries Not Supported Yet.
        }
        copyFrom = copyFrom.lexically_normal();
        copyTo = copyTo.lexically_normal();
        cout << "Copying Target" << endl;
        cout << "Copying Target From " << copyFrom.string() << endl;
        cout << "Copying Target To " << copyTo.string() << endl;
        create_directories(copyTo);
        copy(copyFrom, copyTo, copy_options::update_existing);
        cout << "Target Copying Done" << endl;
        cout << "Copying Include Directories" << endl;
        for (auto &i : includeDirectories)
        {
            if (i.copy)
            {
                path includeDirectoryCopyFrom = i.path;
                path includeDirectoryCopyTo = packageTargetPath / "include";
                includeDirectoryCopyFrom = includeDirectoryCopyFrom.lexically_normal();
                includeDirectoryCopyTo = includeDirectoryCopyTo.lexically_normal();
                cout << "Copying IncludeDirectory From " << includeDirectoryCopyFrom << endl;
                cout << "Copying IncludeDirectory To " << includeDirectoryCopyTo << endl;
                create_directories(includeDirectoryCopyTo);
                copy(includeDirectoryCopyFrom, includeDirectoryCopyTo,
                     copy_options::update_existing | copy_options::recursive);
                cout << "IncludeDirectory Copying Done" << endl;
            }
        }
        string consumerTargetFileName;
        path consumerTargetFile = packageTargetPath / (targetName + ".hmake");
        ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);
        cout << "Copying Completed" << endl;
    }

    if (!postBuildCustomCommands.empty())
    {
        cout << "Executing POST_BUILD_CUSTOM_COMMANDS" << endl;
    }
    for (auto &i : postBuildCustomCommands)
    {
        cout << i << endl;
        if (int a = system(i.c_str()); a != EXIT_SUCCESS)
        {
            exit(a);
        }
    }
}

int BTarget::getNumberOfUpdates()
{
    return 0;
}

BPTarget::BPTarget(const string &targetFilePath, const path &copyFrom)
{

    string targetName;
    string compilerFlags;
    vector<BLibraryDependency> libraryDependencies;
    vector<BIDD> includeDirectories;
    vector<BCompileDefinition> compileDefinitions;
    string targetFileName;
    Json consumerDependenciesJson;
    path packageTargetPath;
    bool packageMode;
    bool copyPackage;
    string packageName;
    string packageCopyPath;
    int packageVariantIndex;

    string fileName = path(targetFilePath).filename().string();
    targetName = fileName.substr(0, fileName.size() - string(".hmake").size());
    Json targetFileJson;
    ifstream(targetFilePath) >> targetFileJson;

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
            packageTargetPath = packageCopyPath / path(packageName) / path(to_string(packageVariantIndex)) / targetName;
        }
    }
    else
    {
        copyPackage = false;
    }

    libraryDependencies = targetFileJson.at("LIBRARY_DEPENDENCIES").get<vector<BLibraryDependency>>();
    if (packageMode)
    {
        includeDirectories = targetFileJson.at("INCLUDE_DIRECTORIES").get<vector<BIDD>>();
    }

    if (copyPackage)
    {
        consumerDependenciesJson = targetFileJson.at("CONSUMER_DEPENDENCIES").get<Json>();
    }

    for (const auto &i : libraryDependencies)
    {
        if (!i.preBuilt)
        {
            BTarget buildTarget(i.path);
        }
        else
        {
            if (!i.imported)
            {
                BPTarget(i.hmakeFilePath, copyFrom);
            }
        }
    }
    if (copyPackage)
    {
        cout << "Copying Started" << endl;
        path copyTo = packageTargetPath / "";
        copyTo = copyTo.lexically_normal();
        cout << "Copying Target" << endl;
        cout << "Copying Target From " << copyFrom.string() << endl;
        cout << "Copying Target To " << copyTo.string() << endl;
        create_directories(copyTo);
        copy(copyFrom, copyTo, copy_options::update_existing);
        cout << "Target Copying Done" << endl;
        cout << "Copying Include Directories" << endl;
        for (auto &i : includeDirectories)
        {
            if (i.copy)
            {
                path includeDirectoryCopyFrom = i.path;
                path includeDirectoryCopyTo = packageTargetPath / "include";
                includeDirectoryCopyFrom = includeDirectoryCopyFrom.lexically_normal();
                includeDirectoryCopyTo = includeDirectoryCopyTo.lexically_normal();
                cout << "Copying IncludeDirectory From " << includeDirectoryCopyFrom << endl;
                cout << "Copying IncludeDirectory To " << includeDirectoryCopyTo << endl;
                create_directories(includeDirectoryCopyTo);
                copy(includeDirectoryCopyFrom, includeDirectoryCopyTo,
                     copy_options::update_existing | copy_options::recursive);
                cout << "IncludeDirectory Copying Done" << endl;
            }
        }
        string consumerTargetFileName;
        path consumerTargetFile = packageTargetPath / (targetName + ".hmake");
        ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);
        cout << "Copying Completed" << endl;
    }
}

BVariant::BVariant(const path &variantFilePath)
{
    Json variantFileJson;
    ifstream(variantFilePath) >> variantFileJson;
    vector<string> targetFilePaths = variantFileJson.at("TARGETS").get<vector<string>>();
    for (const auto &t : targetFilePaths)
    {
        BTarget{t};
    }
}

BProject::BProject(const path &projectFilePath)
{
    Json projectFileJson;
    ifstream(projectFilePath) >> projectFileJson;
    vector<string> vec = projectFileJson.at("VARIANTS").get<vector<string>>();
    for (auto &i : vec)
    {
        BVariant{current_path() / i / "projectVariant.hmake"};
    }
}

BPackage::BPackage(const path &packageFilePath)
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
    for (auto &variant : variants)
    {
        string integerIndex = variant.at("INDEX").get<string>();
        path packageVariantFilePath = current_path() / integerIndex / "packageVariant.hmake";
        if (!is_regular_file(packageVariantFilePath))
        {
            cerr << packageVariantFilePath.string() << " is not a regular file";
        }
        BVariant{packageVariantFilePath};
    }
}
