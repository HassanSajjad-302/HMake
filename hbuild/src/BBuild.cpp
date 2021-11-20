
#include "BBuild.hpp"
#include "Configure.hpp"
#include "fstream"
#include "iostream"
#include "regex"

using std::ifstream, std::ofstream, std::filesystem::copy_options, std::runtime_error, std::cout, std::endl,
    std::to_string, std::filesystem::create_directory, std::filesystem::directory_iterator, std::regex,
    std::filesystem::current_path;

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
    string targetName;
    Compiler compiler;
    Linker linker;
    Archiver staticLibraryTool;
    vector<string> environmentIncludeDirectories;
    vector<string> environmentLibraryDirectories;
    string environmentCompilerFlags;
    string compilerFlags;
    string linkerFlags;
    vector<string> sourceFiles;
    vector<SourceDirectory> sourceDirectories;
    vector<BLibraryDependency> libraryDependencies;
    vector<BIDD> includeDirectories;
    vector<string> libraryDirectories;
    string compilerTransitiveFlags;
    string linkerTransitiveFlags;
    vector<BCompileDefinition> compileDefinitions;
    vector<string> preBuildCustomCommands;
    vector<string> postBuildCustomCommands;
    string buildCacheFilesDirPath;
    BTargetType targetType;
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

    targetType = targetFileJson.at("TARGET_TYPE").get<BTargetType>();
    if (targetType != BTargetType::EXECUTABLE && targetType != BTargetType::STATIC && targetType != BTargetType::SHARED)
    {
        throw runtime_error("BTargetType value in the targetFile is not correct.");
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

    // Build Starts
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

    string projectIncludeDirectoriesFlags;
    string totalIncludeDirectoriesFlags;
    string libraryDependenciesFlags;
    string compileDefinitionsString;
    string libraryDirectoriesString;

    auto getIncludeFlag = [&compiler]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        else
        {
            return "-I ";
        }
    };

    for (const auto &i : includeDirectories)
    {
        projectIncludeDirectoriesFlags.append(getIncludeFlag() + addQuotes(i.path) + " ");
    }
    totalIncludeDirectoriesFlags = projectIncludeDirectoriesFlags;
    for (const auto &i : environmentIncludeDirectories)
    {
        totalIncludeDirectoriesFlags.append(getIncludeFlag() + addQuotes(i) + " ");
    }

    string projectLibraryDirectoriesFlags;
    string totalLibraryDirectoriesFlags;

    auto getLibraryDirectoryFlag = [&compiler]() {
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
        projectLibraryDirectoriesFlags.append(getLibraryDirectoryFlag() + addQuotes(i) + " ");
    }
    totalLibraryDirectoriesFlags = projectLibraryDirectoriesFlags;
    for (const auto &i : environmentLibraryDirectories)
    {
        totalLibraryDirectoriesFlags.append(getLibraryDirectoryFlag() + addQuotes(i) + " ");
    }

    auto getLinkFlag = [&linker](const std::string &libraryPath, const std::string &libraryName) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return addQuotes((path(libraryPath) / path(libraryName)).string() + " ");
        }
        else
        {
            return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
        }
    };

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

    for (const auto &i : compileDefinitions)
    {
        compileDefinitionsString += (compiler.bTFamily == BTFamily::MSVC ? "/D" : "-D") + i.name + "=" + i.value + " ";
    }

    // Build process starts
    cout << "Starting Building" << endl;
    cout << "Building From Start. Does Not Cache Builds Yet." << endl;
    create_directory(outputDirectory);
    create_directory(buildCacheFilesDirPath);
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

    string totalCompilerFlags = environmentCompilerFlags + " " + compilerFlags + " " + compilerTransitiveFlags;
    for (const auto &i : sourceFiles)
    {
        string compileCommand;
        compileCommand += "\"" + addQuotes(compiler.bTPath.make_preferred().string()) + " ";
        compileCommand += totalCompilerFlags + " ";
        compileCommand += compileDefinitionsString + " ";
        compileCommand += totalIncludeDirectoriesFlags;
        compileCommand += compiler.bTFamily == BTFamily::MSVC ? " /c /nologo " : " -c ";
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
                              " /OUT:" + addQuotes((path(outputDirectory) / (outputName + ".lib")).string()) + " " +
                              addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ");
        }
        else if (staticLibraryTool.bTFamily == BTFamily::GCC)
        {
            archiveCommand = staticLibraryTool.bTPath.make_preferred().string() + " rcs " +
                             addQuotes((path(outputDirectory) / ("lib" + outputName + ".a")).string()) + " " +
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
        cout << "Linking" << endl;
        string linkerCommand;
        if (targetType == BTargetType::EXECUTABLE)
        {
            linkerCommand = "\"" + addQuotes(linker.bTPath.make_preferred().string()) + " " + linkerFlags + " " +
                            linkerTransitiveFlags + " " +
                            addQuotes(path(buildCacheFilesDirPath / path("")).string() + "*.o ") + " " +
                            libraryDependenciesFlags + totalLibraryDirectoriesFlags;
            linkerCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
            linkerCommand += addQuotes((path(outputDirectory) / outputName).string()) + "\"";
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
    }

    cout << "Build Complete" << endl;

    if (copyPackage)
    {
        cout << "Copying Started" << endl;
        path copyFrom;
        path copyTo = packageTargetPath / "";
        if (targetType == BTargetType::EXECUTABLE)
        {
            copyFrom = path(outputDirectory) / outputName;
        }
        else if (targetType == BTargetType::STATIC)
        {
            copyFrom = path(outputDirectory) / ("lib" + outputName + ".a");
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
            throw runtime_error(packageVariantFilePath.string() + " is not a regular file");
        }
        BVariant{packageVariantFilePath};
    }
}
