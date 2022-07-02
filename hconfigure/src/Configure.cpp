
#include "Configure.hpp"

#include "algorithm"
#include "fstream"
#include "iostream"
#include "regex"
#include "set"

using std::to_string, std::filesystem::directory_entry, std::filesystem::file_type, std::logic_error, std::ifstream,
    std::ofstream, std::move, std::filesystem::current_path, std::cout, std::endl, std::cerr, std::stringstream,
    std::make_tuple, std::filesystem::directory_iterator, std::filesystem::recursive_directory_iterator,
    std::filesystem::remove;

std::string writePath(const path &writePath)
{
    string str = writePath.string();
    std::replace(str.begin(), str.end(), '\\', '/');
    return str;
}

string addQuotes(const string &pathString)
{
    return "\"" + pathString + "\"";
}

void demo_status(const fs::path &p, fs::file_status s)
{
    std::cout << p;
    switch (s.type())
    {
    case fs::file_type::none:
        std::cout << " has `not-evaluated-yet` type";
        break;
    case fs::file_type::not_found:
        std::cout << " does not exist";
        break;
    case fs::file_type::regular:
        std::cout << " is a regular file";
        break;
    case fs::file_type::directory:
        std::cout << " is a directory";
        break;
    case fs::file_type::symlink:
        std::cout << " is a symlink";
        break;
    case fs::file_type::block:
        std::cout << " is a block device";
        break;
    case fs::file_type::character:
        std::cout << " is a character device";
        break;
    case fs::file_type::fifo:
        std::cout << " is a named IPC pipe";
        break;
    case fs::file_type::socket:
        std::cout << " is a named IPC socket";
        break;
    case fs::file_type::unknown:
        std::cout << " has `unknown` type";
        break;
    default:
        std::cout << " has `implementation-defined` type";
        break;
    }
    std::cout << '\n';
}

File::File(const path &filePath_)
{
    if (filePath_.is_relative())
    {
        filePath = Cache::sourceDirectory.directoryPath / filePath_;
    }
    else
    {
        filePath = filePath_;
    }
    if (directory_entry(filePath).status().type() == file_type::regular)
    {
        filePath = filePath.lexically_normal();
    }
    else
    {
        cerr << filePath_ << " Is Not a regular file";
        exit(EXIT_FAILURE);
    }
}

void to_json(Json &json, const File &file)
{
    json = file.filePath.generic_string();
}

void from_json(const Json &json, File &file)
{
    file.filePath = path(json.get<string>());
}

bool operator<(const File &lhs, const File &rhs)
{
    return lhs.filePath < rhs.filePath;
}

Directory::Directory(const path &directoryPath_)
{
    if (directoryPath_.is_relative())
    {
        directoryPath = Cache::sourceDirectory.directoryPath / directoryPath_;
    }
    else
    {
        directoryPath = directoryPath_;
    }
    auto type = directory_entry(directoryPath).status().type();
    if (type == file_type::directory)
    {
        directoryPath = directoryPath.lexically_normal() / "";
    }
    else
    {
        cerr << directoryPath_ << " Is Not a directory file. File type is ";
        demo_status(directoryPath, fs::status(directoryPath));
        exit(EXIT_FAILURE);
    }
}

void to_json(Json &json, const Directory &directory)
{
    json = directory.directoryPath.generic_string();
}

void from_json(const Json &json, Directory &directory)
{
    directory = Directory(path(json.get<string>()));
}

bool operator<(const Directory &lhs, const Directory &rhs)
{
    return lhs.directoryPath < rhs.directoryPath;
}

void to_json(Json &j, const DependencyType &p)
{
    if (p == DependencyType::PUBLIC)
    {
        j = "PUBLIC";
    }
    else if (p == DependencyType::PRIVATE)
    {
        j = "PRIVATE";
    }
    else
    {
        j = "INTERFACE";
    }
}

void to_json(Json &j, const CompileDefinition &cd)
{
    j["NAME"] = cd.name;
    j["VALUE"] = cd.value;
}

void from_json(const Json &j, CompileDefinition &cd)
{
    cd.name = j.at("NAME").get<string>();
    cd.value = j.at("VALUE").get<string>();
}

void to_json(Json &json, const OSFamily &bTFamily)
{
    if (bTFamily == OSFamily::WINDOWS)
    {
        json = "WINDOWS";
    }
    else if (bTFamily == OSFamily::LINUX_UNIX)
    {
        json = "LINUX_UNIX";
    }
}

void from_json(const Json &json, OSFamily &bTFamily)
{
    if (json == "WINDOWS")
    {
        bTFamily = OSFamily::WINDOWS;
    }
    else if (json == "LINUX_UNIX")
    {
        bTFamily = OSFamily::LINUX_UNIX;
    }
}

void to_json(Json &json, const BTFamily &bTFamily)
{
    if (bTFamily == BTFamily::GCC)
    {
        json = "GCC";
    }
    else if (bTFamily == BTFamily::MSVC)
    {
        json = "MSVC";
    }
    else
    {
        json = "CLANG";
    }
}

void from_json(const Json &json, BTFamily &bTFamily)
{
    if (json == "GCC")
    {
        bTFamily = BTFamily::GCC;
    }
    else if (json == "MSVC")
    {
        bTFamily = BTFamily::MSVC;
    }
    else if (json == "CLANG")
    {
        bTFamily = BTFamily::CLANG;
    }
}

void to_json(Json &json, const BuildTool &buildTool)
{
    json["FAMILY"] = buildTool.bTFamily;
    json["PATH"] = buildTool.bTPath.generic_string();
    json["VERSION"] = buildTool.bTVersion;
}

void from_json(const Json &json, BuildTool &buildTool)
{
    buildTool.bTPath = json.at("PATH").get<string>();
    buildTool.bTFamily = json.at("FAMILY").get<BTFamily>();
    buildTool.bTVersion = json.at("VERSION").get<Version>();
}

void to_json(Json &json, const LibraryType &libraryType)
{
    if (libraryType == LibraryType::STATIC)
    {
        json = "STATIC";
    }
    else
    {
        json = "SHARED";
    }
}

void from_json(const Json &json, LibraryType &libraryType)
{
    if (json == "STATIC")
    {
        libraryType = LibraryType::STATIC;
    }
    else if (json == "SHARED")
    {
        libraryType = LibraryType::SHARED;
    }
}

void to_json(Json &json, const ConfigType &configType)
{
    if (configType == ConfigType::DEBUG)
    {
        json = "DEBUG";
    }
    else
    {
        json = "RELEASE";
    }
}

void from_json(const Json &json, ConfigType &configType)
{
    if (json == "DEBUG")
    {
        configType = ConfigType::DEBUG;
    }
    else if (json == "RELEASE")
    {
        configType = ConfigType::RELEASE;
    }
}

Target::Target(string targetName_, const Variant &variant) : targetName(move(targetName_)), outputName(targetName)
{
    configurationType = variant.configurationType;
    compiler = variant.compiler;
    linker = variant.linker;
    archiver = variant.archiver;
    flags = variant.flags;
    environment = variant.environment;
}

string Target::getTargetFilePath(unsigned long variantCount) const
{
    return Cache::configureDirectory.directoryPath.generic_string() + to_string(variantCount) + "/" + targetName + "/" +
           targetName + ".hmake";
}

string Target::getTargetFilePathPackage(unsigned long variantCount) const
{
    return Cache::configureDirectory.directoryPath.generic_string() + "/Package/" + to_string(variantCount) + "/" +
           targetName + "/" + targetName + ".hmake";
}

void Target::assignDifferentVariant(const Variant &variant)
{
    configurationType = variant.configurationType;
    compiler = variant.compiler;
    linker = variant.linker;
    flags = variant.flags;
    for (auto &i : libraryDependencies)
    {
        if (i.ldlt == LDLT::LIBRARY)
        {
            i.library.assignDifferentVariant(variant);
        }
    }
}

vector<string> Target::convertCustomTargetsToJson(const vector<CustomTarget> &customTargets, VariantMode mode)
{
    vector<string> commands;
    for (auto &i : customTargets)
    {
        if (i.mode == mode)
        {
            commands.push_back(i.command);
        }
    }
    return commands;
}

// I think I can use std::move for Target members here as this is the last function called.
Json Target::convertToJson(unsigned long variantIndex) const
{
    Json targetFileJson;

    vector<Directory> includeDirectories;
    string compilerFlags;
    string linkerFlags;
    vector<Json> compileDefinitionsArray;
    vector<Json> dependenciesArray;
    SourceAggregate finalModuleAggregate;

    for (const auto &e : includeDirectoryDependencies)
    {
        includeDirectories.emplace_back(e.includeDirectory);
    }
    for (const auto &e : compilerFlagsDependencies)
    {
        compilerFlags.append(" " + e.compilerFlags + " ");
    }
    for (const auto &e : linkerFlagsDependencies)
    {
        linkerFlags.append(" " + e.linkerFlags + " ");
    }
    for (const auto &e : compileDefinitionDependencies)
    {
        Json compileDefinitionObject = e.compileDefinition;
        compileDefinitionsArray.push_back(compileDefinitionObject);
    }
    finalModuleAggregate = moduleAggregate;

    vector<const LibraryDependency *> dependencies;
    dependencies = LibraryDependency::getDependencies(*this);
    for (const auto &libraryDependency : dependencies)
    {
        if (libraryDependency->ldlt == LDLT::LIBRARY)
        {
            const Library &library = libraryDependency->library;
            for (const auto &e : library.includeDirectoryDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    includeDirectories.emplace_back(e.includeDirectory);
                }
            }

            for (const auto &e : library.compilerFlagsDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    compilerFlags.append(" " + e.compilerFlags + " ");
                }
            }

            for (const auto &e : library.linkerFlagsDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    linkerFlags.append(" " + e.linkerFlags + " ");
                }
            }

            for (const auto &e : library.compileDefinitionDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    Json compileDefinitionObject = e.compileDefinition;
                    compileDefinitionsArray.push_back(compileDefinitionObject);
                }
            }
            Json libDepObject;
            libDepObject["PREBUILT"] = false;
            libDepObject["PATH"] = (Cache::configureDirectory.directoryPath / to_string(variantIndex) /
                                    library.targetName / path(library.targetName + ".hmake"))
                                       .generic_string();

            dependenciesArray.push_back(libDepObject);
        }
        else
        {
            vector<int> v;
            auto populateDependencies = [&](const PLibrary &pLibrary) {
                includeDirectories.insert(includeDirectories.end(), pLibrary.includeDirectoryDependencies.begin(),
                                          pLibrary.includeDirectoryDependencies.end());
                compilerFlags.append(" " + pLibrary.compilerFlagsDependencies + " ");
                linkerFlags.append(" " + pLibrary.linkerFlagsDependencies + " ");

                for (const auto &e : pLibrary.compileDefinitionDependencies)
                {
                    Json compileDefinitionObject = e;
                    compileDefinitionsArray.push_back(compileDefinitionObject);
                }
                Json libDepObject;
                libDepObject["PREBUILT"] = true;
                libDepObject["PATH"] = pLibrary.libraryPath.generic_string();
                dependenciesArray.push_back(libDepObject);
            };
            const PLibrary &pLibrary =
                libraryDependency->ldlt == LDLT::PLIBRARY ? libraryDependency->pLibrary : libraryDependency->ppLibrary;
            populateDependencies(pLibrary);
        }
    }

    targetFileJson["TARGET_TYPE"] = targetType;
    targetFileJson["OUTPUT_NAME"] = outputName;
    targetFileJson["OUTPUT_DIRECTORY"] = outputDirectory;
    targetFileJson["CONFIGURATION"] = configurationType;
    targetFileJson["COMPILER"] = compiler;
    targetFileJson["LINKER"] = linker;
    targetFileJson["ARCHIVER"] = archiver;
    targetFileJson["ENVIRONMENT"] = environment;
    targetFileJson["COMPILER_FLAGS"] = flags.compilerFlags[compiler.bTFamily][configurationType];
    targetFileJson["LINKER_FLAGS"] = flags.linkerFlags[linker.bTFamily][configurationType];
    sourceAggregate.convertToJson(targetFileJson);
    moduleAggregate.convertToJson(targetFileJson);
    targetFileJson["LIBRARY_DEPENDENCIES"] = dependenciesArray;
    targetFileJson["INCLUDE_DIRECTORIES"] = includeDirectories;
    targetFileJson["COMPILER_TRANSITIVE_FLAGS"] = compilerFlags;
    targetFileJson["LINKER_TRANSITIVE_FLAGS"] = linkerFlags;
    targetFileJson["COMPILE_DEFINITIONS"] = compileDefinitionsArray;
    targetFileJson["PRE_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(preBuild, VariantMode::PROJECT);
    targetFileJson["POST_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(postBuild, VariantMode::PROJECT);
    targetFileJson["VARIANT"] = "PROJECT";
    return targetFileJson;
}

void to_json(Json &j, const SourceDirectory &sourceDirectory)
{
    j["PATH"] = sourceDirectory.sourceDirectory;
    j["REGEX_STRING"] = sourceDirectory.regex;
}

void from_json(const Json &j, SourceDirectory &sourceDirectory)
{
    sourceDirectory.sourceDirectory = Directory(j.at("PATH").get<string>());
    sourceDirectory.regex = j.at("REGEX_STRING").get<string>();
}

bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs)
{
    return lhs.sourceDirectory.directoryPath < rhs.sourceDirectory.directoryPath || lhs.regex < rhs.regex;
}

void SourceAggregate::convertToJson(Json &j) const
{
    j[Identifier + sourceFilesString] = files;
    j[Identifier + sourceDirectoriesString] = directories;
}

set<string> SourceAggregate::convertFromJsonAndGetAllSourceFiles(const Json &j, const string &Identifier)
{
    set<string> sourceFiles = j.at(Identifier + sourceFilesString).get<set<string>>();
    set<SourceDirectory> sourceDirectories = j.at(Identifier + sourceDirectoriesString).get<set<SourceDirectory>>();
    for (const auto &i : sourceDirectories)
    {
        for (const auto &k : recursive_directory_iterator(i.sourceDirectory.directoryPath))
        {
            if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(i.regex)))
            {
                sourceFiles.emplace(k.path().generic_string());
            }
        }
    }
    return sourceFiles;
}

bool SourceAggregate::empty() const
{
    return files.empty() && directories.empty();
}

void to_json(Json &j, const TargetType &targetType)
{
    if (targetType == TargetType::EXECUTABLE)
    {
        j = "EXECUTABLE";
    }
    else if (targetType == TargetType::STATIC)
    {
        j = "STATIC";
    }
    else if (targetType == TargetType::SHARED)
    {
        j = "SHARED";
    }
    else if (targetType == TargetType::PLIBRARY_SHARED)
    {
        j = "PLIBRARY_STATIC";
    }
    else
    {
        j = "PLIBRARY_SHARED";
    }
}

void from_json(const Json &j, TargetType &targetType)
{
    if (j == "EXECUTABLE")
    {
        targetType = TargetType::EXECUTABLE;
    }
    else if (j == "STATIC")
    {
        targetType = TargetType::STATIC;
    }
    else if (j == "SHARED")
    {
        targetType = TargetType::SHARED;
    }
    else if (j == "PLIBRARY_STATIC")
    {
        targetType = TargetType::PLIBRARY_STATIC;
    }
    else
    {
        targetType = TargetType::PLIBRARY_SHARED;
    }
}

string getActualNameFromTargetName(TargetType targetType, const OSFamily &osFamily, const string &targetName)
{
    if (targetType == TargetType::EXECUTABLE)
    {
        return targetName + (osFamily == OSFamily::WINDOWS ? ".exe" : "");
    }
    else if (targetType == TargetType::STATIC || targetType == TargetType::PLIBRARY_STATIC)
    {
        string actualName;
        actualName = osFamily == OSFamily::WINDOWS ? "" : "lib";
        actualName += targetName;
        actualName += osFamily == OSFamily::WINDOWS ? ".lib" : ".a";
        return actualName;
    }
    else
    {
    }
    cerr << "Other Targets Not Supported Yet." << endl;
    exit(EXIT_FAILURE);
}

string getTargetNameFromActualName(TargetType targetType, const OSFamily &osFamily, const string &actualName)
{
    if (targetType == TargetType::STATIC || targetType == TargetType::PLIBRARY_STATIC)
    {
        string libName = actualName;
        // Removes lib from libName.a
        libName = osFamily == OSFamily::WINDOWS ? actualName : libName.erase(0, 3);
        // Removes .a from libName.a or .lib from Name.lib
        unsigned short eraseCount = osFamily == OSFamily::WINDOWS ? 4 : 2;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    cerr << "Other Targets Not Supported Yet." << endl;
    exit(EXIT_FAILURE);
}

void Target::configure(unsigned long variantIndex) const
{
    path targetFileDir = Cache::configureDirectory.directoryPath / to_string(variantIndex) / targetName;
    create_directories(targetFileDir);
    if (outputDirectory.directoryPath.empty())
    {
        const_cast<Directory &>(outputDirectory) = Directory(targetFileDir);
    }

    Json targetFileJson;
    targetFileJson = convertToJson(variantIndex);

    path targetFilePath = targetFileDir / (targetName + ".hmake");
    ofstream(targetFilePath) << targetFileJson.dump(4);
}

Json Target::convertToJson(const Package &package, const PackageVariant &variant, unsigned count) const
{
    Json targetFileJson;

    Json includeDirectoriesArray;
    string compilerFlags;
    string linkerFlags;
    vector<Json> compileDefinitionsArray;
    vector<Json> dependenciesArray;

    vector<string> consumerIncludeDirectories;
    string consumerCompilerFlags;
    string consumerLinkerFlags;
    vector<Json> consumerCompileDefinitionsArray;
    vector<Json> consumerDependenciesArray;

    for (const auto &e : includeDirectoryDependencies)
    {
        Json JIDDObject;
        JIDDObject["PATH"] = e.includeDirectory;
        if (e.dependencyType == DependencyType::PUBLIC)
        {
            if (package.cacheCommonIncludeDirs && e.includeDirectory.isCommon)
            {
                consumerIncludeDirectories.emplace_back(
                    "../../Common/" + to_string(e.includeDirectory.commonDirectoryNumber) + "/include/");
                JIDDObject["COPY"] = false;
            }
            else
            {
                consumerIncludeDirectories.emplace_back("include/");
                JIDDObject["COPY"] = true;
            }
        }
        else
        {
            JIDDObject["COPY"] = false;
        }
        includeDirectoriesArray.push_back(JIDDObject);
    }
    for (const auto &e : compilerFlagsDependencies)
    {
        compilerFlags.append(" " + e.compilerFlags + " ");
        if (e.dependencyType == DependencyType::PUBLIC)
        {
            consumerCompilerFlags.append(" " + e.compilerFlags + " ");
        }
    }
    for (const auto &e : linkerFlagsDependencies)
    {
        linkerFlags.append(" " + e.linkerFlags + " ");
        if (e.dependencyType == DependencyType::PUBLIC)
        {
            consumerLinkerFlags.append(" " + e.linkerFlags + " ");
        }
    }
    for (const auto &e : compileDefinitionDependencies)
    {
        Json compileDefinitionObject = e.compileDefinition;
        compileDefinitionsArray.push_back(compileDefinitionObject);
        if (e.dependencyType == DependencyType::PUBLIC)
        {
            consumerCompileDefinitionsArray.push_back(compileDefinitionObject);
        }
    }

    vector<const LibraryDependency *> dependencies;
    dependencies = LibraryDependency::getDependencies(*this);
    for (const auto &libraryDependency : dependencies)
    {
        if (libraryDependency->ldlt == LDLT::LIBRARY)
        {
            const Library &library = libraryDependency->library;
            for (const auto &e : library.includeDirectoryDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e.includeDirectory;
                    JIDDObject["COPY"] = false;
                    includeDirectoriesArray.push_back(JIDDObject);
                }
            }

            for (const auto &e : library.compilerFlagsDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    compilerFlags.append(" " + e.compilerFlags + " ");
                }
            }

            for (const auto &e : library.linkerFlagsDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    linkerFlags.append(" " + e.linkerFlags + " ");
                }
            }

            for (const auto &e : library.compileDefinitionDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    Json compileDefinitionObject = e.compileDefinition;
                    compileDefinitionsArray.push_back(compileDefinitionObject);
                }
            }

            Json libDepObject;
            libDepObject["PREBUILT"] = false;
            Json consumeLibDepOject;
            consumeLibDepOject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE_FROM_OTHER_HMAKE_PACKAGE"] = false;
            consumeLibDepOject["NAME"] = library.targetName;
            consumerDependenciesArray.push_back(consumeLibDepOject);
            libDepObject["PATH"] = library.getTargetFilePathPackage(count);
            dependenciesArray.push_back(libDepObject);
        }
        else
        {
            Json libDepObject;
            libDepObject["PREBUILT"] = true;
            Json consumeLibDepOject;
            if (libraryDependency->ldlt == LDLT::PLIBRARY)
            {
                const auto &pLibrary = libraryDependency->pLibrary;
                for (const auto &e : pLibrary.includeDirectoryDependencies)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e;
                    JIDDObject["COPY"] = false;

                    includeDirectoriesArray.push_back(JIDDObject);
                }
                compilerFlags.append(" " + pLibrary.compilerFlagsDependencies + " ");
                linkerFlags.append(" " + pLibrary.linkerFlagsDependencies + " ");

                for (const auto &e : pLibrary.compileDefinitionDependencies)
                {
                    Json compileDefinitionObject = e;
                    compileDefinitionsArray.push_back(compileDefinitionObject);
                }
                libDepObject["PATH"] = pLibrary.libraryPath.generic_string();
                libDepObject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = false;
                libDepObject["HMAKE_FILE_PATH"] =
                    pLibrary.getTargetVariantDirectoryPath(count) / (pLibrary.libraryName + ".hmake");
                consumeLibDepOject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = false;
                consumeLibDepOject["NAME"] = pLibrary.libraryName;
            }
            else
            {
                const auto &ppLibrary = libraryDependency->ppLibrary;
                for (const auto &e : ppLibrary.includeDirectoryDependencies)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e;
                    JIDDObject["COPY"] = false;
                    includeDirectoriesArray.push_back(JIDDObject);
                }
                compilerFlags.append(" " + ppLibrary.compilerFlagsDependencies + " ");
                linkerFlags.append(" " + ppLibrary.linkerFlagsDependencies + " ");

                for (const auto &e : ppLibrary.compileDefinitionDependencies)
                {
                    Json compileDefinitionObject = e;
                    compileDefinitionsArray.push_back(compileDefinitionObject);
                }
                libDepObject["PATH"] = ppLibrary.libraryPath.generic_string();
                libDepObject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = ppLibrary.importedFromOtherHMakePackage;
                if (!ppLibrary.importedFromOtherHMakePackage)
                {
                    libDepObject["HMAKE_FILE_PATH"] =
                        ppLibrary.getTargetVariantDirectoryPath(count) / (ppLibrary.libraryName + ".hmake");
                }

                consumeLibDepOject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = ppLibrary.importedFromOtherHMakePackage;
                consumeLibDepOject["NAME"] = ppLibrary.libraryName;
                if (ppLibrary.importedFromOtherHMakePackage)
                {
                    consumeLibDepOject["PACKAGE_NAME"] = ppLibrary.packageName;
                    consumeLibDepOject["PACKAGE_VERSION"] = ppLibrary.packageVersion;
                    consumeLibDepOject["PACKAGE_PATH"] = ppLibrary.packagePath.generic_string();
                    consumeLibDepOject["USE_INDEX"] = ppLibrary.useIndex;
                    consumeLibDepOject["INDEX"] = ppLibrary.index;
                    consumeLibDepOject["PACKAGE_VARIANT_JSON"] = ppLibrary.packageVariantJson;
                }
            }
            consumerDependenciesArray.push_back(consumeLibDepOject);
            dependenciesArray.push_back(libDepObject);
        }
    }

    targetFileJson["TARGET_TYPE"] = targetType;
    targetFileJson["OUTPUT_NAME"] = outputName;
    targetFileJson["OUTPUT_DIRECTORY"] = getTargetFilePathPackage(count);
    targetFileJson["CONFIGURATION"] = configurationType;
    targetFileJson["COMPILER"] = compiler;
    targetFileJson["LINKER"] = linker;
    targetFileJson["ARCHIVER"] = archiver;
    targetFileJson["ENVIRONMENT"] = environment;
    targetFileJson["COMPILER_FLAGS"] = flags.compilerFlags[compiler.bTFamily][configurationType];
    targetFileJson["LINKER_FLAGS"] = flags.linkerFlags[linker.bTFamily][configurationType];
    sourceAggregate.convertToJson(targetFileJson);
    moduleAggregate.convertToJson(targetFileJson);
    targetFileJson["LIBRARY_DEPENDENCIES"] = dependenciesArray;
    targetFileJson["INCLUDE_DIRECTORIES"] = includeDirectoriesArray;
    targetFileJson["COMPILER_TRANSITIVE_FLAGS"] = compilerFlags;
    targetFileJson["LINKER_TRANSITIVE_FLAGS"] = linkerFlags;
    targetFileJson["COMPILE_DEFINITIONS"] = compileDefinitionsArray;
    targetFileJson["PRE_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(preBuild, VariantMode::PACKAGE);
    targetFileJson["POST_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(postBuild, VariantMode::PACKAGE);

    Json consumerDependenciesJson;

    consumerDependenciesJson["LIBRARY_TYPE"] = targetType;
    consumerDependenciesJson["LIBRARY_DEPENDENCIES"] = consumerDependenciesArray;
    consumerDependenciesJson["INCLUDE_DIRECTORIES"] = consumerIncludeDirectories;
    consumerDependenciesJson["COMPILER_TRANSITIVE_FLAGS"] = consumerCompilerFlags;
    consumerDependenciesJson["LINKER_TRANSITIVE_FLAGS"] = consumerLinkerFlags;
    consumerDependenciesJson["COMPILE_DEFINITIONS"] = consumerCompileDefinitionsArray;

    targetFileJson["CONSUMER_DEPENDENCIES"] = consumerDependenciesJson;
    targetFileJson["VARIANT"] = "PACKAGE";
    targetFileJson["PACKAGE_COPY"] = Cache::copyPackage;
    if (Cache::copyPackage)
    {
        targetFileJson["PACKAGE_NAME"] = package.name;
        targetFileJson["PACKAGE_VERSION"] = package.version;
        targetFileJson["PACKAGE_COPY_PATH"] = Cache::packageCopyPath;
        targetFileJson["PACKAGE_VARIANT_INDEX"] = count;
    }
    return targetFileJson;
}

void Target::configure(const Package &package, const PackageVariant &variant, unsigned count) const
{
    Json targetFileJson;
    targetFileJson = convertToJson(package, variant, count);
    string targetFileBuildDir = getTargetFilePathPackage(count);
    create_directory(path(targetFileBuildDir));
    string filePath = targetFileBuildDir + targetName + ".hmake";
    ofstream(filePath) << targetFileJson.dump(4);
    for (const auto &libDep : libraryDependencies)
    {
        if (libDep.ldlt == LDLT::LIBRARY)
        {
            libDep.library.setTargetType();
            libDep.library.configure(package, variant, count);
        }
        else if (libDep.ldlt == LDLT::PLIBRARY)
        {
            libDep.pLibrary.setTargetType();
            libDep.pLibrary.configure(package, variant, count);
        }
        else if (!libDep.ppLibrary.importedFromOtherHMakePackage)
        {
            libDep.ppLibrary.setTargetType();
            libDep.ppLibrary.configure(package, variant, count);
        }
    }
}

Executable::Executable(string targetName_, const Variant &variant) : Target(move(targetName_), variant)
{
    targetType = TargetType::EXECUTABLE;
}

void Executable::assignDifferentVariant(const Variant &variant)
{
    Target::assignDifferentVariant(variant);
}

Library::Library(string targetName_, const Variant &variant)
    : libraryType(variant.libraryType), Target(move(targetName_), variant)
{
}

void Library::assignDifferentVariant(const Variant &variant)
{
    Target::assignDifferentVariant(variant);
    libraryType = variant.libraryType;
}

void Library::setTargetType() const
{
    if (libraryType == LibraryType::STATIC)
    {
        targetType = TargetType::STATIC;
    }
    else
    {
        targetType = TargetType::SHARED;
    }
}

// So that the type becomes compatible for usage  in key of map
bool operator<(const Version &lhs, const Version &rhs)
{
    if (lhs.majorVersion < rhs.majorVersion)
    {
        return true;
    }
    else if (lhs.majorVersion == rhs.majorVersion)
    {
        if (lhs.minorVersion < rhs.minorVersion)
        {
            return true;
        }
        else if (lhs.minorVersion == rhs.minorVersion)
        {
            if (lhs.patchVersion < rhs.patchVersion)
            {
                return true;
            }
        }
    }
    return false;
}

bool operator==(const Version &lhs, const Version &rhs)
{
    return lhs.majorVersion == rhs.majorVersion && lhs.minorVersion == rhs.minorVersion &&
           lhs.patchVersion == rhs.patchVersion;
}

CompilerFlags &CompilerFlags::operator[](BTFamily compilerFamily) const
{
    compilerHelper = true;
    compilerCurrent = compilerFamily;
    return const_cast<CompilerFlags &>(*this);
}

CompilerFlags &CompilerFlags::operator[](CompilerVersion compilerVersion) const
{
    if (!compilerHelper)
    {
        cerr << "Wrong Usage Of CompilerFlags class" << endl;
        exit(EXIT_FAILURE);
    }
    compilerVersionHelper = true;
    compilerVersionCurrent = compilerVersion;
    return const_cast<CompilerFlags &>(*this);
}

CompilerFlags &CompilerFlags::operator[](ConfigType configType) const
{
    configHelper = true;
    configCurrent = configType;
    return const_cast<CompilerFlags &>(*this);
}

void CompilerFlags::operator=(const string &flags1)
{

    set<BTFamily> compilerFamilies1;
    CompilerVersion compilerVersions1;
    set<ConfigType> configurationTypes1;

    if (compilerHelper)
    {
        compilerFamilies1.emplace(compilerCurrent);
    }
    else
    {
        compilerFamilies1 = compilerFamilies;
    }

    if (compilerVersionHelper)
    {
        compilerVersions1 = compilerVersionCurrent;
    }
    else
    {
        CompilerVersion compilerVersion;
        compilerVersion.majorVersion = compilerVersion.minorVersion = compilerVersion.patchVersion = 0;
        compilerVersion.comparison = Comparison::GREATER_THAN_OR_EQUAL_TO;
        compilerVersions1 = compilerVersion;
    }

    if (configHelper)
    {
        configurationTypes1.emplace(configCurrent);
    }
    else
    {
        configurationTypes1 = configurationTypes;
    }

    for (auto &compiler : compilerFamilies1)
    {
        for (auto &configuration : configurationTypes1)
        {
            auto t = make_tuple(compiler, compilerVersions1, configuration);
            if (auto [pos, ok] = compilerFlags.emplace(t, flags1); !ok)
            {
                cout << "Rewriting the flags in compilerFlags for this configuration";
                compilerFlags[t] = flags1;
            }
        }
    }

    compilerHelper = false;
    compilerVersionHelper = false;
    configHelper = false;
}

CompilerFlags CompilerFlags::defaultFlags()
{
    CompilerFlags compilerFlags;
    compilerFlags.compilerFamilies.emplace(BTFamily::GCC);
    compilerFlags[BTFamily::GCC][CompilerVersion{10, 2, 0}][ConfigType::DEBUG] = "-g";
    compilerFlags[ConfigType::RELEASE] = "-O3 -DNDEBUG";
    return compilerFlags;
}

CompilerFlags::operator string() const
{
    set<string> flagsSet;
    for (const auto &[key, value] : compilerFlags)
    {
        if (compilerHelper)
        {
            if (get<0>(key) != compilerCurrent)
            {
                continue;
            }
            if (compilerVersionHelper)
            {
                const auto &compilerVersion = get<1>(key);
                if (compilerVersion.comparison == Comparison::EQUAL && compilerVersion == compilerVersionCurrent)
                {
                }
                else if (compilerVersion.comparison == Comparison::GREATER_THAN &&
                         compilerVersion > compilerVersionCurrent)
                {
                }
                else if (compilerVersion.comparison == Comparison::LESSER_THAN &&
                         compilerVersion < compilerVersionCurrent)
                {
                }
                else if (compilerVersion.comparison == Comparison::GREATER_THAN_OR_EQUAL_TO &&
                         compilerVersion >= compilerVersionCurrent)
                {
                }
                else if (compilerVersion.comparison == Comparison::LESSER_THAN_OR_EQUAL_TO &&
                         compilerVersion <= compilerVersionCurrent)
                {
                }
                else
                {
                    continue;
                }
            }
        }
        if (configHelper && get<2>(key) != configCurrent)
        {
            continue;
        }
        flagsSet.emplace(value);
    }

    string flagsStr;
    for (const auto &i : flagsSet)
    {
        flagsStr += i;
        flagsStr += " ";
    }
    return flagsStr;
}

LinkerFlags &LinkerFlags::operator[](BTFamily linkerFamily) const
{
    linkerHelper = true;
    linkerCurrent = linkerFamily;
    return const_cast<LinkerFlags &>(*this);
}

LinkerFlags &LinkerFlags::operator[](LinkerVersion linkerVersion) const
{
    if (!linkerHelper)
    {
        cerr << "Wrong Usage Of LinkerFlags class" << endl;
        exit(EXIT_FAILURE);
    }
    linkerVersionHelper = true;
    linkerVersionCurrent = linkerVersion;
    return const_cast<LinkerFlags &>(*this);
}

LinkerFlags &LinkerFlags::operator[](ConfigType configType) const
{
    configHelper = true;
    configCurrent = configType;
    return const_cast<LinkerFlags &>(*this);
}

void LinkerFlags::operator=(const string &flags1)
{

    set<BTFamily> linkerFamilies1;
    LinkerVersion linkerVersions1;
    set<ConfigType> configurationTypes1;

    if (linkerHelper)
    {
        linkerFamilies1.emplace(linkerCurrent);
    }
    else
    {
        linkerFamilies1 = linkerFamilies;
    }

    if (linkerVersionHelper)
    {
        linkerVersions1 = linkerVersionCurrent;
    }
    else
    {
        LinkerVersion linkerVersion;
        linkerVersion.minorVersion = linkerVersion.minorVersion = linkerVersion.patchVersion = 0;
        linkerVersion.comparison = Comparison::GREATER_THAN_OR_EQUAL_TO;
        linkerVersions1 = linkerVersion;
    }

    if (configHelper)
    {
        configurationTypes1.emplace(configCurrent);
    }
    else
    {
        configurationTypes1 = configurationTypes;
    }

    for (auto &linker : linkerFamilies1)
    {
        for (auto &configuration : configurationTypes1)
        {
            auto t = make_tuple(linker, linkerVersions1, configuration);
            if (auto [pos, ok] = linkerFlags.emplace(t, flags1); !ok)
            {
                cout << "Rewriting the flags in compilerFlags for this configuration";
                linkerFlags[t] = flags1;
            }
        }
    }

    linkerHelper = false;
    linkerVersionHelper = false;
    configHelper = false;
}

LinkerFlags LinkerFlags::defaultFlags()
{
    return LinkerFlags{};
}

LinkerFlags::operator string() const
{
    set<string> flagsSet;
    for (const auto &[key, value] : linkerFlags)
    {
        if (linkerHelper)
        {
            if (get<0>(key) != linkerCurrent)
            {
                continue;
            }
            if (linkerVersionHelper)
            {
                const auto &linkerVersion = get<1>(key);
                if (linkerVersion.comparison == Comparison::EQUAL && linkerVersion == linkerVersionCurrent)
                {
                }
                else if (linkerVersion.comparison == Comparison::GREATER_THAN && linkerVersion > linkerVersionCurrent)
                {
                }
                else if (linkerVersion.comparison == Comparison::LESSER_THAN && linkerVersion < linkerVersionCurrent)
                {
                }
                else if (linkerVersion.comparison == Comparison::GREATER_THAN_OR_EQUAL_TO &&
                         linkerVersion >= linkerVersionCurrent)
                {
                }
                else if (linkerVersion.comparison == Comparison::LESSER_THAN_OR_EQUAL_TO &&
                         linkerVersion <= linkerVersionCurrent)
                {
                }
                else
                {
                    continue;
                }
            }
        }
        if (configHelper && get<2>(key) != configCurrent)
        {
            continue;
        }
        flagsSet.emplace(value);
    }

    string flagsStr;
    for (const auto &i : flagsStr)
    {
        flagsStr += i;
        flagsStr += " ";
    }
    return flagsStr;
}

void Cache::initializeCache()
{
    path filePath = current_path() / "cache.hmake";
    Json cacheFileJson;
    ifstream(filePath) >> cacheFileJson;

    path srcDirPath = cacheFileJson.at("SOURCE_DIRECTORY").get<string>();
    if (srcDirPath.is_relative())
    {
        srcDirPath = (current_path() / srcDirPath).lexically_normal();
    }
    Cache::sourceDirectory = Directory(srcDirPath);
    Cache::configureDirectory = Directory(current_path());

    Cache::copyPackage = cacheFileJson.at("PACKAGE_COPY").get<bool>();
    if (copyPackage)
    {
        packageCopyPath = cacheFileJson.at("PACKAGE_COPY_PATH").get<string>();
    }

    Cache::projectConfigurationType = cacheFileJson.at("CONFIGURATION").get<ConfigType>();
    Cache::compilerArray = cacheFileJson.at("COMPILER_ARRAY").get<vector<Compiler>>();
    Cache::selectedCompilerArrayIndex = cacheFileJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();
    Cache::linkerArray = cacheFileJson.at("LINKER_ARRAY").get<vector<Linker>>();
    Cache::selectedLinkerArrayIndex = cacheFileJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();
    Cache::archiverArray = cacheFileJson.at("ARCHIVER_ARRAY").get<vector<Archiver>>();
    Cache::selectedArchiverArrayIndex = cacheFileJson.at("ARCHIVER_SELECTED_ARRAY_INDEX").get<int>();
    Cache::libraryType = cacheFileJson.at("LIBRARY_TYPE").get<LibraryType>();
    Cache::cacheVariables = cacheFileJson.at("CACHE_VARIABLES").get<Json>();
#ifdef _WIN32
    Cache::environment = Environment::initializeEnvironmentFromVSBatchCommand(
        R"("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64)");
#else

#endif
    if (!exists(path("settings.hmake")))
    {
        Json settings = Settings{};
        ofstream("settings.hmake") << settings.dump(4);
    }
}

void Cache::registerCacheVariables()
{
    path filePath = current_path() / "cache.hmake";
    Json cacheFileJson;
    ifstream(filePath) >> cacheFileJson;
    cacheFileJson["CACHE_VARIABLES"] = Cache::cacheVariables;
    ofstream(filePath) << cacheFileJson.dump(4);
}

Variant::Variant()
{
    configurationType = Cache::projectConfigurationType;
    compiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
    linker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
    archiver = Cache::archiverArray[Cache::selectedArchiverArrayIndex];
    libraryType = Cache::libraryType;
    environment = Cache::environment;
    flags = ::flags;
}

#include "variant"
void Variant::configure(VariantMode mode, unsigned long variantCount)
{
    std::variant<Target *, PLibrary *> pointers;

    stack<decltype(pointers)> targets;
    set<decltype(pointers)> targetsConfigured;

    for (auto &exe : executables)
    {
        targets.push(&exe);
    }
    for (auto &lib : libraries)
    {
        targets.push(&lib);
    }

    vector<Target *> targetsWithModules;
    while (!targets.empty())
    {
        auto target = targets.top();
        targets.pop();
        if (targetsConfigured.contains(target))
        {
            continue;
        }
        else
        {
            targetsConfigured.emplace(target);
        }
        if (!target.index())
        {
            Target *t = std::get<Target *>(target);

            if (t->targetType != TargetType::EXECUTABLE)
            {
                auto *l = static_cast<Library *>(t);
                l->setTargetType();
                l->configure(variantCount);
            }
            else
            {
                t->configure(variantCount);
            }
            if (!t->moduleAggregate.empty())
            {
                targetsWithModules.emplace_back(t);
            }

            for (auto &libDep : t->libraryDependencies)
            {
                if (libDep.ldlt == LDLT::LIBRARY)
                {
                    targets.push(&libDep.library);
                }
                else if (libDep.ldlt == LDLT::PLIBRARY)
                {
                    targets.push(&libDep.pLibrary);
                }
                else if (libDep.ldlt == LDLT::PPLIBRARY)
                {
                    targets.push(&libDep.ppLibrary);
                }
            }
        }
        else
        {
            PLibrary *l = std::get<PLibrary *>(target);
            for (auto &libDep : l->libraryDependencies)
            {
                if (libDep.ldlt == LDLT::LIBRARY)
                {
                    targets.push(&libDep.library);
                }
                else if (libDep.ldlt == LDLT::PLIBRARY)
                {
                    targets.push(&libDep.pLibrary);
                }
                else if (libDep.ldlt == LDLT::PPLIBRARY)
                {
                    targets.push(&libDep.ppLibrary);
                }
            }
        }
    }

    Json variantJson;
    variantJson["CONFIGURATION"] = configurationType;
    variantJson["COMPILER"] = compiler;
    variantJson["LINKER"] = linker;
    variantJson["ARCHIVER"] = archiver;
    string compilerFlags = flags.compilerFlags[compiler.bTFamily][configurationType];
    variantJson["COMPILER_FLAGS"] = compilerFlags;
    string linkerFlags = flags.linkerFlags[linker.bTFamily][configurationType];
    variantJson["LINKER_FLAGS"] = linkerFlags;
    variantJson["LIBRARY_TYPE"] = libraryType;
    variantJson["ENVIRONMENT"] = environment;

    vector<string> targetArray;
    vector<string> targetsWithModulesVector;

    auto parseExesAndLibs = [&](string (Target::*func)(unsigned long count) const) {
        for (const auto &exe : executables)
        {
            targetArray.emplace_back((exe.*func)(variantCount));
            if (!exe.moduleAggregate.empty())
            {
                targetsWithModulesVector.emplace_back((exe.*func)(variantCount));
            }
        }
        for (const auto &lib : libraries)
        {
            targetArray.emplace_back((lib.*func)(variantCount));
            if (!lib.moduleAggregate.empty())
            {
                targetsWithModulesVector.emplace_back((lib.*func)(variantCount));
            }
        }
    };

    mode == VariantMode::PROJECT ? parseExesAndLibs(&Target::getTargetFilePath)
                                 : parseExesAndLibs(&Target::getTargetFilePathPackage);
    variantJson["TARGETS"] = targetArray;
    variantJson["TARGETS_WITH_MODULES"] = targetsWithModulesVector;

    // Here, I have to write
    // variantJson["TARGETS_WITH_MODULES"] = targetsArrary
    path variantFileDir = Cache::configureDirectory.directoryPath / to_string(variantCount);
    create_directory(variantFileDir);
    path projectFilePath = variantFileDir / "projectVariant.hmake";
    ofstream(projectFilePath) << variantJson.dump(4);
}

void to_json(Json &j, const Version &p)
{
    j = to_string(p.majorVersion) + "." + to_string(p.minorVersion) + "." + to_string(p.patchVersion);
}

void from_json(const Json &j, Version &v)
{
    string jString = j;
    char delim = '.';
    stringstream ss(jString);
    string item;
    int count = 0;
    while (getline(ss, item, delim))
    {
        if (count == 0)
        {
            v.majorVersion = stoi(item);
        }
        else if (count == 1)
        {
            v.minorVersion = stoi(item);
        }
        else
        {
            v.patchVersion = stoi(item);
        }
        ++count;
    }
}

Environment Environment::initializeEnvironmentFromVSBatchCommand(const string &command)
{
    string temporaryIncludeFilename = "temporaryInclude.txt";
    string temporaryLibFilename = "temporaryLib.txt";
    string temporaryBatchFilename = "temporaryBatch.bat";
    ofstream(temporaryBatchFilename) << "call " + command << endl
                                     << "echo %INCLUDE% > " + temporaryIncludeFilename << endl
                                     << "echo %LIB%;%LIBPATH% > " + temporaryLibFilename;

    if (int code = system(temporaryBatchFilename.c_str()); code == EXIT_FAILURE)
    {
        cout << "Error in Executing Batch File" << endl;
        exit(-1);
    }
    remove(temporaryBatchFilename);

    auto splitPathsAndAssignToVector = [](string &accumulatedPaths, set<Directory> &separatePaths) {
        unsigned long pos = accumulatedPaths.find(';');
        while (pos != std::string::npos)
        {
            std::string token = accumulatedPaths.substr(0, pos);
            if (token.empty())
            {
                break; // Only In Release Configuration in Visual Studio, for some reason the while loop also run with
                       // empty string. Not investigating further for now
            }
            token = path(token).lexically_normal().string();
            Directory dir(token);
            if (!separatePaths.contains(dir))
            {
                separatePaths.emplace(dir);
            }
            accumulatedPaths.erase(0, pos + 1);
            pos = accumulatedPaths.find(';');
        }
    };

    Environment environment;
    string accumulatedPaths = file_to_string(temporaryIncludeFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    remove(temporaryIncludeFilename);
    splitPathsAndAssignToVector(accumulatedPaths, environment.includeDirectories);
    accumulatedPaths = file_to_string(temporaryLibFilename);
    accumulatedPaths.pop_back(); // Remove the last '\n' and ' '
    accumulatedPaths.pop_back();
    accumulatedPaths.append(";");
    remove(temporaryLibFilename);
    splitPathsAndAssignToVector(accumulatedPaths, environment.libraryDirectories);
    environment.compilerFlags = " /EHsc /MD /nologo";
    environment.linkerFlags = " /SUBSYSTEM:CONSOLE /NOLOGO";
    return environment;
}

Environment Environment::initializeEnvironmentOnLinux()
{
    // Maybe run cpp -v and then parse the output.
    // https://stackoverflow.com/questions/28688484/actual-default-linker-script-and-settings-gcc-uses
    return {};
}

void to_json(Json &j, const Environment &environment)
{
    j["INCLUDE_DIRECTORIES"] = environment.includeDirectories;
    j["LIBRARY_DIRECTORIES"] = environment.libraryDirectories;
    j["COMPILER_FLAGS"] = environment.compilerFlags;
    j["LINKER_FLAGS"] = environment.linkerFlags;
}

void from_json(const Json &j, Environment &environment)
{
    environment.includeDirectories = j.at("INCLUDE_DIRECTORIES").get<set<Directory>>();
    environment.libraryDirectories = j.at("LIBRARY_DIRECTORIES").get<set<Directory>>();
    environment.compilerFlags = j.at("COMPILER_FLAGS").get<string>();
    environment.linkerFlags = j.at("LINKER_FLAGS").get<string>();
}

void Project::configure()
{
    for (unsigned long i = 0; i < projectVariants.size(); ++i)
    {
        projectVariants[i].configure(VariantMode::PROJECT, i);
    }
    Json projectFileJson;
    vector<string> projectVariantsInt;
    for (unsigned long i = 0; i < projectVariants.size(); ++i)
    {
        projectVariantsInt.push_back(to_string(i));
    }
    projectFileJson["VARIANTS"] = projectVariantsInt;
    ofstream(Cache::configureDirectory.directoryPath / "project.hmake") << projectFileJson.dump(4);
}

Package::Package(string name_) : name(move(name_)), version{0, 0, 0}
{
}

void Package::configureCommonAmongVariants()
{
    struct NonCommonIncludeDir
    {
        const Directory *directory;
        bool isCommon = false;
        int variantIndex;
        unsigned long indexInPackageCommonTargetsVector;
    };
    struct CommonIncludeDir
    {
        vector<unsigned long> variantsIndices;
        vector<Directory *> directories;
    };
    vector<NonCommonIncludeDir> packageNonCommonIncludeDirs;
    vector<CommonIncludeDir> packageCommonIncludeDirs;

    auto isDirectoryCommonOrNonCommon = [&](Directory *includeDirectory, unsigned long index) {
        bool matched = false;
        for (auto &i : packageNonCommonIncludeDirs)
        {
            if (i.directory->directoryPath == includeDirectory->directoryPath)
            {
                if (i.isCommon)
                {
                    packageCommonIncludeDirs[i.indexInPackageCommonTargetsVector].variantsIndices.push_back(index);
                }
                else
                {
                    CommonIncludeDir includeDir;
                    includeDir.directories.push_back(const_cast<Directory *>(i.directory));
                    includeDir.directories.push_back(includeDirectory);
                    includeDir.variantsIndices.push_back(i.variantIndex);
                    includeDir.variantsIndices.push_back(index);
                    packageCommonIncludeDirs.push_back(includeDir);
                    i.isCommon = true;
                    i.indexInPackageCommonTargetsVector = packageCommonIncludeDirs.size() - 1;
                }
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            NonCommonIncludeDir includeDir;
            includeDir.directory = includeDirectory;
            includeDir.variantIndex = index;
            packageNonCommonIncludeDirs.push_back(includeDir);
        }
    };

    auto assignPackageCommonAndNonCommonIncludeDirsForSingleTarget = [&](Target *target, unsigned long index) {
        for (auto &idd : target->includeDirectoryDependencies)
        {
            if (idd.dependencyType == DependencyType::PUBLIC)
            {
                isDirectoryCommonOrNonCommon(&(idd.includeDirectory), index);
            }
        }
    };

    auto assignPackageCommonAndNonCommonIncludeDirsForSinglePLibrary = [&](PLibrary *pLibrary, unsigned long index) {
        for (auto &id : pLibrary->includeDirectoryDependencies)
        {
            isDirectoryCommonOrNonCommon(&id, index);
        }
    };

    auto assignPackageCommonAndNonCommonIncludeDirs = [&](const Target *target, unsigned long index) {
        assignPackageCommonAndNonCommonIncludeDirsForSingleTarget(const_cast<Target *>(target), index);
        auto dependencies = LibraryDependency::getDependencies(*target);
        for (const auto &libraryDependency : dependencies)
        {
            if (libraryDependency->ldlt == LDLT::LIBRARY)
            {
                auto &lib = const_cast<Library &>(libraryDependency->library);
                assignPackageCommonAndNonCommonIncludeDirsForSingleTarget(&lib, index);
            }
            else
            {
                auto &pLib = const_cast<PLibrary &>(libraryDependency->pLibrary);
                assignPackageCommonAndNonCommonIncludeDirsForSinglePLibrary(&pLib, index);
            }
        }
    };

    for (unsigned long i = 0; i < packageVariants.size(); ++i)
    {
        for (const auto &exe : packageVariants[i].executables)
        {
            assignPackageCommonAndNonCommonIncludeDirs(&exe, i);
        }
        for (const auto &lib : packageVariants[i].libraries)
        {
            assignPackageCommonAndNonCommonIncludeDirs(&lib, i);
        }
    }

    Json commonDirsJson;
    for (unsigned long i = 0; i < packageCommonIncludeDirs.size(); ++i)
    {
        for (auto j : packageCommonIncludeDirs[i].directories)
        {
            j->isCommon = true;
            j->commonDirectoryNumber = i;
        }

        Json commonDirJsonObject;
        commonDirJsonObject["INDEX"] = i;
        commonDirJsonObject["PATH"] = *packageCommonIncludeDirs[i].directories[0];
        commonDirJsonObject["VARIANTS_INDICES"] = packageCommonIncludeDirs[i].variantsIndices;
        commonDirsJson.emplace_back(commonDirJsonObject);
    }
    create_directory(Cache::configureDirectory.directoryPath / "Package");
    ofstream(Cache::configureDirectory.directoryPath / "Package" / path("Common.hmake")) << commonDirsJson.dump(4);
}

void Package::configure()
{
    checkForSimilarJsonsInPackageVariants();

    Json packageFileJson;
    Json packageVariantJson;
    int count = 0;
    for (auto &i : packageVariants)
    {
        if (i.uniqueJson.contains("INDEX"))
        {
            cerr << "Package Variant Json can not have COUNT in it's Json." << endl;
            exit(EXIT_FAILURE);
        }
        i.uniqueJson["INDEX"] = to_string(count);
        packageVariantJson.emplace_back(i.uniqueJson);
        ++count;
    }
    packageFileJson["NAME"] = name;
    packageFileJson["VERSION"] = version;
    packageFileJson["CACHE_INCLUDES"] = cacheCommonIncludeDirs;
    packageFileJson["PACKAGE_COPY"] = Cache::copyPackage;
    packageFileJson["PACKAGE_COPY_PATH"] = Cache::packageCopyPath;
    packageFileJson["VARIANTS"] = packageVariantJson;
    path packageDirectorypath = Cache::configureDirectory.directoryPath / "Package";
    create_directory(packageDirectorypath);
    Directory packageDirectory(packageDirectorypath);
    path file = packageDirectory.directoryPath / "package.hmake";
    ofstream(file) << packageFileJson.dump(4);

    if (Cache::copyPackage && cacheCommonIncludeDirs)
    {
        configureCommonAmongVariants();
    }
    count = 0;
    for (auto &variant : packageVariants)
    {
        // Json variantJsonFile = variant.configure(VariantMode::PACKAGE, count);
        path variantFileDir = packageDirectory.directoryPath / to_string(count);
        create_directory(variantFileDir);
        // path variantFilePath = variantFileDir / "packageVariant.hmake";
        // ofstream(variantFilePath) << variantJsonFile.dump(4);
        for (const auto &exe : variant.executables)
        {
            exe.configure(*this, variant, count);
        }
        for (const auto &lib : variant.libraries)
        {
            lib.setTargetType();
            lib.configure(*this, variant, count);
        }
        ++count;
    }
}

void Package::checkForSimilarJsonsInPackageVariants()
{
    // Check that no two JSON'S of packageVariants of package are same
    set<nlohmann::json> checkSet;
    for (auto &i : packageVariants)
    {
        // TODO: Implement this check correctly.
        // This associatedJson is ordered_json, thus two different json's equality test will be false if the order is
        // different even if they have same elements. Thus we first convert it into json normal which is unordered one.
        /* nlohmann::json j = i.json;
        if (auto [pos, ok] = checkSet.emplace(j); !ok) {
          throw logic_error("No two json's of packagevariants can be same. Different order with same values does
        not make two Json's different");
        }*/
    }
}

PLibrary::PLibrary(const path &libraryPath_, LibraryType libraryType_) : libraryType(libraryType_)
{
    if (libraryPath_.is_relative())
    {
        libraryPath = Cache::sourceDirectory.directoryPath / libraryPath_;
        libraryPath = libraryPath.lexically_normal();
    }
    libraryName = getTargetNameFromActualName(libraryType == LibraryType::STATIC ? TargetType::PLIBRARY_STATIC
                                                                                 : TargetType::SHARED,
                                              Cache::osFamily, libraryPath.filename().string());
}

path PLibrary::getTargetVariantDirectoryPath(int variantCount) const
{
    return Cache::configureDirectory.directoryPath / "Package" / to_string(variantCount) / libraryName;
}

void PLibrary::setTargetType() const
{
    if (libraryType == LibraryType::STATIC)
    {
        targetType = TargetType::PLIBRARY_STATIC;
    }
    else
    {
        targetType = TargetType::PLIBRARY_SHARED;
    }
}

Json PLibrary::convertToJson(const Package &package, const PackageVariant &variant, unsigned count) const
{

    Json targetFileJson;

    Json includeDirectoriesArray;
    vector<Json> dependenciesArray;

    vector<string> consumerIncludeDirectories;
    string consumerCompilerFlags;
    string consumerLinkerFlags;
    vector<Json> consumerCompileDefinitionsArray;
    vector<Json> consumerDependenciesArray;

    for (const auto &e : includeDirectoryDependencies)
    {
        Json JIDDObject;
        JIDDObject["PATH"] = e;
        JIDDObject["COPY"] = true;
        if (package.cacheCommonIncludeDirs && e.isCommon)
        {
            consumerIncludeDirectories.emplace_back("../../Common/" + to_string(e.commonDirectoryNumber) + "/include/");
            JIDDObject["COPY"] = false;
        }
        else
        {
            consumerIncludeDirectories.emplace_back("include/");
            JIDDObject["COPY"] = true;
        }
        includeDirectoriesArray.push_back(JIDDObject);
    }

    consumerCompilerFlags.append(" " + compilerFlagsDependencies + " ");
    consumerLinkerFlags.append(" " + linkerFlagsDependencies + " ");

    for (const auto &e : compileDefinitionDependencies)
    {
        Json compileDefinitionObject = e;
        consumerCompileDefinitionsArray.push_back(compileDefinitionObject);
    }

    vector<const LibraryDependency *> dependencies;
    dependencies = LibraryDependency::getDependencies(*this);
    for (const auto &libraryDependency : dependencies)
    {
        if (libraryDependency->ldlt == LDLT::LIBRARY)
        {
            const Library &library = libraryDependency->library;
            for (const auto &e : library.includeDirectoryDependencies)
            {
                if (e.dependencyType == DependencyType::PUBLIC)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e.includeDirectory;
                    JIDDObject["COPY"] = false;
                    includeDirectoriesArray.push_back(JIDDObject);
                }
            }

            Json libDepObject;
            libDepObject["PREBUILT"] = false;
            Json consumeLibDepOject;
            consumeLibDepOject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = false;
            consumeLibDepOject["NAME"] = library.targetName;
            consumerDependenciesArray.push_back(consumeLibDepOject);
            libDepObject["PATH"] = library.getTargetFilePathPackage(count);
            dependenciesArray.push_back(libDepObject);
        }
        else
        {
            Json libDepObject;
            libDepObject["PREBUILT"] = true;
            Json consumeLibDepOject;
            if (libraryDependency->ldlt == LDLT::PLIBRARY)
            {
                const auto &pLibrary = libraryDependency->pLibrary;
                for (const auto &e : pLibrary.includeDirectoryDependencies)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e;
                    JIDDObject["COPY"] = false;

                    includeDirectoriesArray.push_back(JIDDObject);
                }

                libDepObject["PATH"] = pLibrary.libraryPath.generic_string();
                libDepObject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = false;
                libDepObject["HMAKE_FILE_PATH"] =
                    pLibrary.getTargetVariantDirectoryPath(count) / (pLibrary.libraryName + ".hmake");
                consumeLibDepOject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = false;
                consumeLibDepOject["NAME"] = pLibrary.libraryName;
            }
            else
            {
                const auto &ppLibrary = libraryDependency->ppLibrary;
                for (const auto &e : ppLibrary.includeDirectoryDependencies)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e;
                    JIDDObject["COPY"] = false;
                    includeDirectoriesArray.push_back(JIDDObject);
                }

                libDepObject["PATH"] = ppLibrary.libraryPath.generic_string();
                libDepObject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = ppLibrary.importedFromOtherHMakePackage;
                if (!ppLibrary.importedFromOtherHMakePackage)
                {
                    libDepObject["HMAKE_FILE_PATH"] =
                        ppLibrary.getTargetVariantDirectoryPath(count) / (ppLibrary.libraryName + ".hmake");
                }

                consumeLibDepOject["IMPORTED_FROM_OTHER_HMAKE_PACKAGE"] = ppLibrary.importedFromOtherHMakePackage;
                consumeLibDepOject["NAME"] = ppLibrary.libraryName;
                if (ppLibrary.importedFromOtherHMakePackage)
                {
                    consumeLibDepOject["PACKAGE_NAME"] = ppLibrary.packageName;
                    consumeLibDepOject["PACKAGE_VERSION"] = ppLibrary.packageVersion;
                    consumeLibDepOject["PACKAGE_PATH"] = ppLibrary.packagePath.generic_string();
                    consumeLibDepOject["USE_INDEX"] = ppLibrary.useIndex;
                    consumeLibDepOject["INDEX"] = ppLibrary.index;
                    consumeLibDepOject["PACKAGE_VARIANT_JSON"] = ppLibrary.packageVariantJson;
                }
            }
            consumerDependenciesArray.push_back(consumeLibDepOject);
            dependenciesArray.push_back(libDepObject);
        }
    }

    targetFileJson["TARGET_TYPE"] = targetType;
    targetFileJson["NAME"] = libraryName;
    targetFileJson["PATH"] = libraryPath.generic_string();
    targetFileJson["LIBRARY_DEPENDENCIES"] = dependenciesArray;
    targetFileJson["INCLUDE_DIRECTORIES"] = includeDirectoriesArray;

    Json consumerDependenciesJson;
    consumerDependenciesJson["LIBRARY_TYPE"] = libraryType;
    consumerDependenciesJson["LIBRARY_DEPENDENCIES"] = consumerDependenciesArray;
    consumerDependenciesJson["INCLUDE_DIRECTORIES"] = consumerIncludeDirectories;
    consumerDependenciesJson["COMPILER_TRANSITIVE_FLAGS"] = consumerCompilerFlags;
    consumerDependenciesJson["LINKER_TRANSITIVE_FLAGS"] = consumerLinkerFlags;
    consumerDependenciesJson["COMPILE_DEFINITIONS"] = consumerCompileDefinitionsArray;

    targetFileJson["CONSUMER_DEPENDENCIES"] = consumerDependenciesJson;
    targetFileJson["VARIANT"] = "PACKAGE";
    targetFileJson["PACKAGE_COPY"] = Cache::copyPackage;
    if (Cache::copyPackage)
    {
        targetFileJson["PACKAGE_NAME"] = package.name;
        targetFileJson["PACKAGE_VERSION"] = package.version;
        targetFileJson["PACKAGE_COPY_PATH"] = Cache::packageCopyPath;
        targetFileJson["PACKAGE_VARIANT_INDEX"] = count;
    }
    return targetFileJson;
}

void PLibrary::configure(const Package &package, const PackageVariant &variant, unsigned count) const
{
    Json targetFileJson;
    targetFileJson = convertToJson(package, variant, count);
    path targetFileBuildDir = Cache::configureDirectory.directoryPath / "Package" / to_string(count) / libraryName;
    create_directory(targetFileBuildDir);
    string fileExt;
    path filePath = targetFileBuildDir / (libraryName + ".hmake");
    ofstream(filePath.string()) << targetFileJson.dump(4);
    for (const auto &libDep : libraryDependencies)
    {
        if (libDep.ldlt == LDLT::LIBRARY)
        {
            libDep.library.setTargetType();
            libDep.library.configure(package, variant, count);
        }
        else if (libDep.ldlt == LDLT::PLIBRARY)
        {
            libDep.pLibrary.setTargetType();
            libDep.pLibrary.configure(package, variant, count);
        }
        else if (!libDep.ppLibrary.importedFromOtherHMakePackage)
        {
            libDep.ppLibrary.setTargetType();
            libDep.ppLibrary.configure(package, variant, count);
        }
    }
}

LibraryDependency::LibraryDependency(Library library_, DependencyType dependencyType_)
    : library(move(library_)), dependencyType(dependencyType_), ldlt(LDLT::LIBRARY)
{
}

LibraryDependency::LibraryDependency(PLibrary pLibrary_, DependencyType dependencyType_)
    : pLibrary(move(pLibrary_)), dependencyType(dependencyType_), ldlt(LDLT::PLIBRARY)
{
}

LibraryDependency::LibraryDependency(PPLibrary ppLibrary_, DependencyType dependencyType_)
    : ppLibrary(move(ppLibrary_)), dependencyType(dependencyType_), ldlt(LDLT::PPLIBRARY)
{
}

PPLibrary::PPLibrary(string libraryName_, const CPackage &cPackage, const CPVariant &cpVariant)
{

    libraryName = move(libraryName_);
    path libraryDirectoryPath;
    bool found = false;
    for (auto &i : directory_iterator(cpVariant.variantPath))
    {
        if (i.path().filename() == libraryName && i.is_directory())
        {
            libraryDirectoryPath = i;
            found = true;
            break;
        }
    }
    if (!found)
    {
        cerr << "Library " << libraryName << " Not Present In Package Variant" << endl;
        exit(EXIT_FAILURE);
    }

    path libraryFilePath = libraryDirectoryPath / (libraryName + ".hmake");
    if (!exists(libraryFilePath))
    {
        cerr << "Library " << libraryName << " Does Not Exists. Searched For File " << endl
             << writePath(libraryFilePath);
        exit(EXIT_FAILURE);
    }

    Json libraryFileJson;
    ifstream(libraryFilePath) >> libraryFileJson;

    if (libraryFileJson.at("LIBRARY_TYPE").get<string>() == "STATIC")
    {
        libraryType = LibraryType::STATIC;
    }
    else
    {
        libraryType = LibraryType::SHARED;
    }
    Json libDependencies = libraryFileJson.at("LIBRARY_DEPENDENCIES").get<Json>();
    for (auto &i : libDependencies)
    {
        bool isImported = i.at("IMPORTED_FROM_OTHER_HMAKE_PACKAGE").get<bool>();
        if (!isImported)
        {
            string dependencyLibraryName = i.at("NAME").get<string>();
            PPLibrary ppLibrary(dependencyLibraryName, cPackage, cpVariant);
            libraryDependencies.emplace_back(ppLibrary, DependencyType::PUBLIC);
        }
        else
        {
            CPackage pack(i.at("PACKAGE_PATH").get<string>());
            string libName = i.at("NAME").get<string>();
            if (i.at("USE_INDEX").get<bool>())
            {
                PPLibrary ppLibrary(libName, pack, pack.getVariant(i.at("INDEX").get<int>()));
                libraryDependencies.emplace_back(ppLibrary, DependencyType::PUBLIC);
            }
            else
            {
                PPLibrary ppLibrary(libName, pack, pack.getVariant(i.at("PACKAGE_VARIANT_JSON").get<Json>()));
                libraryDependencies.emplace_back(ppLibrary, DependencyType::PUBLIC);
            }
        }
    }
    vector<string> includeDirs = libraryFileJson.at("INCLUDE_DIRECTORIES").get<vector<string>>();
    for (auto &i : includeDirs)
    {
        Directory dir(libraryDirectoryPath / i);
        includeDirectoryDependencies.push_back(dir);
    }
    compilerFlagsDependencies = libraryFileJson.at("COMPILER_TRANSITIVE_FLAGS").get<string>();
    linkerFlagsDependencies = libraryFileJson.at("LINKER_TRANSITIVE_FLAGS").get<string>();
    if (!libraryFileJson.at("COMPILE_DEFINITIONS").empty())
    {
        compileDefinitionDependencies = libraryFileJson.at("COMPILE_DEFINITIONS").get<vector<CompileDefinition>>();
    }
    libraryPath = libraryDirectoryPath /
                  path(getActualNameFromTargetName(libraryType == LibraryType::STATIC ? TargetType::PLIBRARY_STATIC
                                                                                      : TargetType::PLIBRARY_SHARED,
                                                   Cache::osFamily, libraryName));
    packageName = cPackage.name;
    packageVersion = cPackage.version;
    packagePath = cPackage.packagePath;
    packageVariantJson = cpVariant.variantJson;
    index = cpVariant.index;
}

CPVariant::CPVariant(path variantPath_, Json variantJson_, unsigned index_)
    : variantPath(move(variantPath_)), variantJson(move(variantJson_)), index(index_)
{
}

CPackage::CPackage(path packagePath_) : packagePath(move(packagePath_))
{
    if (packagePath.is_relative())
    {
        packagePath = Cache::sourceDirectory.directoryPath / packagePath;
    }
    packagePath = packagePath.lexically_normal();
    path packageFilePath = packagePath / "cpackage.hmake";
    if (!exists(packageFilePath))
    {
        cerr << "Package File Path " << packageFilePath.generic_string() << " Does Not Exists " << endl;
        exit(-1);
    }
    Json packageFileJson;
    ifstream(packageFilePath) >> packageFileJson;
    name = packageFileJson["NAME"];
    version = packageFileJson["VERSION"];
    variantsJson = packageFileJson["VARIANTS"];
}

CPVariant CPackage::getVariant(const Json &variantJson_)
{
    int numberOfMatches = 0;
    int matchIndex;
    for (unsigned long i = 0; i < variantsJson.size(); ++i)
    {
        bool matched = true;
        for (const auto &keyValuePair : variantJson_.items())
        {
            if (!variantsJson[i].contains(keyValuePair.key()) ||
                variantsJson[i][keyValuePair.key()] != keyValuePair.value())
            {
                matched = false;
                break;
            }
        }
        if (matched)
        {
            ++numberOfMatches;
            matchIndex = i;
        }
    }
    if (numberOfMatches == 1)
    {
        string index = variantsJson[matchIndex].at("INDEX").get<string>();
        return CPVariant(packagePath / index, variantJson_, stoi(index));
    }
    else if (numberOfMatches == 0)
    {
        cerr << "No Json in package " << writePath(packagePath) << " matches \n" << variantJson_.dump(4) << endl;
        exit(EXIT_FAILURE);
    }
    else if (numberOfMatches > 1)
    {
        cerr << "More than 1 Jsons in package " + writePath(packagePath) + " matches \n"
             << endl
             << to_string(variantJson_);
        exit(EXIT_FAILURE);
    }
}

CPVariant CPackage::getVariant(const int index)
{
    int numberOfMatches = 0;
    for (const auto &i : variantsJson)
    {
        if (i.at("INDEX").get<string>() == to_string(index))
        {
            return CPVariant(packagePath / to_string(index), i, index);
        }
    }
    cerr << "No Json in package " << writePath(packagePath) << " has index " << to_string(index) << endl;
    exit(EXIT_FAILURE);
}
void to_json(Json &json, const PathPrint &pathPrint)
{
    json["PATH_PRINT_LEVEL"] = pathPrint.printLevel;
    json["DEPTH"] = pathPrint.depth;
    json["ADD_QUOTES"] = pathPrint.addQuotes;
}
void from_json(const Json &json, PathPrint &pathPrint)
{
    uint8_t level = json.at("PATH_PRINT_LEVEL").get<uint8_t>();
    if (level < 0 || level > 2)
    {
        cerr << "Level should be in range 0-2" << endl;
        exit(EXIT_FAILURE);
    }
    pathPrint.printLevel = (PathPrintLevel)level;
    pathPrint.depth = json.at("DEPTH").get<int>();
    pathPrint.addQuotes = json.at("ADD_QUOTES").get<bool>();
}
void to_json(Json &json, const CompileCommandPrintSettings &ccpSettings)
{
    json["TOOL"] = ccpSettings.tool;
    json["ENVIRONMENT_COMPILER_FLAGS"] = ccpSettings.environmentCompilerFlags;
    json["COMPILER_FLAGS"] = ccpSettings.compilerFlags;
    json["COMPILER_TRANSITIVE_FLAGS"] = ccpSettings.compilerTransitiveFlags;
    json["COMPILE_DEFINITIONS"] = ccpSettings.compileDefinitions;
    json["PROJECT_INCLUDE_DIRECTORIES"] = ccpSettings.projectIncludeDirectories;
    json["ENVIRONMENT_INCLUDE_DIRECTORIES"] = ccpSettings.environmentIncludeDirectories;
    json["REQUIRE_IFCS"] = ccpSettings.requireIFCs;
    json["SOURCE_FILE"] = ccpSettings.sourceFile;
    json["INFRASTRUCTURE_FLAGS"] = ccpSettings.infrastructureFlags;
    json["IFC_OUTPUT_FILE"] = ccpSettings.ifcOutputFile;
    json["OBJECT_FILE"] = ccpSettings.objectFile;
    json["OUTPUT_AND_ERROR_FILES"] = ccpSettings.outputAndErrorFiles;
    json["PRUNE_HEADER_DEPENDENCIES_FROM_MSVC_OUTPUT"] = ccpSettings.pruneHeaderDepsFromMSVCOutput;
    json["PRUNE_COMPILED_SOURCE_FILE_NAME_FROM_MSVC_OUTPUT"] = ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput;
    json["RATIO_FOR_HMAKE_TIME"] = ccpSettings.ratioForHMakeTime;
    json["SHOW_PERCENTAGE"] = ccpSettings.showPercentage;
}

void from_json(const Json &json, CompileCommandPrintSettings &ccpSettings)
{
    ccpSettings.tool = json.at("TOOL").get<PathPrint>();
    ccpSettings.tool.isTool = true;
    ccpSettings.environmentCompilerFlags = json.at("ENVIRONMENT_COMPILER_FLAGS").get<bool>();
    ccpSettings.compilerFlags = json.at("COMPILER_FLAGS").get<bool>();
    ccpSettings.compilerTransitiveFlags = json.at("COMPILER_TRANSITIVE_FLAGS").get<bool>();
    ccpSettings.compileDefinitions = json.at("COMPILE_DEFINITIONS").get<bool>();
    ccpSettings.projectIncludeDirectories = json.at("PROJECT_INCLUDE_DIRECTORIES").get<PathPrint>();
    ccpSettings.environmentIncludeDirectories = json.at("ENVIRONMENT_INCLUDE_DIRECTORIES").get<PathPrint>();
    ccpSettings.requireIFCs = json.at("REQUIRE_IFCS").get<PathPrint>();
    ccpSettings.sourceFile = json.at("SOURCE_FILE").get<PathPrint>();
    ccpSettings.infrastructureFlags = json.at("INFRASTRUCTURE_FLAGS").get<bool>();
    ccpSettings.ifcOutputFile = json.at("IFC_OUTPUT_FILE").get<PathPrint>();
    ccpSettings.objectFile = json.at("OBJECT_FILE").get<PathPrint>();
    ccpSettings.outputAndErrorFiles = json.at("OUTPUT_AND_ERROR_FILES").get<PathPrint>();
    ccpSettings.pruneHeaderDepsFromMSVCOutput = json.at("PRUNE_HEADER_DEPENDENCIES_FROM_MSVC_OUTPUT").get<bool>();
    ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput =
        json.at("PRUNE_COMPILED_SOURCE_FILE_NAME_FROM_MSVC_OUTPUT").get<bool>();
    ccpSettings.ratioForHMakeTime = json.at("RATIO_FOR_HMAKE_TIME").get<bool>();
    ccpSettings.showPercentage = json.at("SHOW_PERCENTAGE").get<bool>();
}

void to_json(Json &json, const ArchiveCommandPrintSettings &acpSettings)
{
    json["TOOL"] = acpSettings.tool;
    json["INFRASTRUCTURE_FLAGS"] = acpSettings.infrastructureFlags;
    json["OBJECT_FILES"] = acpSettings.objectFiles;
    json["ARCHIVE"] = acpSettings.archive;
    json["OUTPUT_AND_ERROR_FILES"] = acpSettings.outputAndErrorFiles;
}

void from_json(const Json &json, ArchiveCommandPrintSettings &acpSettings)
{
    acpSettings.tool = json.at("TOOL").get<PathPrint>();
    acpSettings.tool.isTool = true;
    acpSettings.infrastructureFlags = json.at("INFRASTRUCTURE_FLAGS").get<bool>();
    acpSettings.objectFiles = json.at("OBJECT_FILES").get<PathPrint>();
    acpSettings.archive = json.at("ARCHIVE").get<PathPrint>();
    acpSettings.outputAndErrorFiles = json.at("OUTPUT_AND_ERROR_FILES").get<PathPrint>();
}

void to_json(Json &json, const LinkCommandPrintSettings &lcpSettings)
{
    json["TOOL"] = lcpSettings.tool;
    json["INFRASTRUCTURE_FLAGS"] = lcpSettings.infrastructureFlags;
    json["OBJECT_FILES"] = lcpSettings.objectFiles;
    json["LIBRARY_DEPENDENCIES"] = lcpSettings.libraryDependencies;
    json["LIBRARY_DIRECTORIES"] = lcpSettings.libraryDirectories;
    json["ENVIRONMENT_LIBRARY_DIRECTORIES"] = lcpSettings.environmentLibraryDirectories;
    json["BINARY"] = lcpSettings.binary;
}

void from_json(const Json &json, LinkCommandPrintSettings &lcpSettings)
{
    lcpSettings.tool = json.at("TOOL").get<PathPrint>();
    lcpSettings.tool.isTool = true;
    lcpSettings.infrastructureFlags = json.at("INFRASTRUCTURE_FLAGS").get<bool>();
    lcpSettings.objectFiles = json.at("OBJECT_FILES").get<PathPrint>();
    lcpSettings.libraryDependencies = json.at("LIBRARY_DEPENDENCIES").get<PathPrint>();
    lcpSettings.libraryDirectories = json.at("LIBRARY_DIRECTORIES").get<PathPrint>();
    lcpSettings.environmentLibraryDirectories = json.at("ENVIRONMENT_LIBRARY_DIRECTORIES").get<PathPrint>();
    lcpSettings.binary = json.at("OBJECT_FILES").get<PathPrint>();
}

void to_json(Json &json, const PrintColorSettings &printColorSettings)
{
    json["COMPILE_COMMAND_COLOR"] = printColorSettings.compileCommandColor;
    json["ARCHIVE_COMMAND_COLOR"] = printColorSettings.archiveCommandColor;
    json["LINK_COMMAND_COLOR"] = printColorSettings.linkCommandColor;
    json["TOOL_ERROR_OUTPUT"] = printColorSettings.toolErrorOutput;
    json["HBUILD_STATEMENT_OUTPUT"] = printColorSettings.hbuildStatementOutput;
    json["HBUILD_SEQUENCE_OUTPUT"] = printColorSettings.hbuildSequenceOutput;
    json["HBUILD_ERROR_OUTPUT"] = printColorSettings.hbuildErrorOutput;
}

void from_json(const Json &json, PrintColorSettings &printColorSettings)
{
    printColorSettings.compileCommandColor = json.at("COMPILE_COMMAND_COLOR").get<int>();
    printColorSettings.archiveCommandColor = json.at("ARCHIVE_COMMAND_COLOR").get<int>();
    printColorSettings.linkCommandColor = json.at("LINK_COMMAND_COLOR").get<int>();
    printColorSettings.toolErrorOutput = json.at("TOOL_ERROR_OUTPUT").get<int>();
    printColorSettings.hbuildStatementOutput = json.at("HBUILD_STATEMENT_OUTPUT").get<int>();
    printColorSettings.hbuildSequenceOutput = json.at("HBUILD_SEQUENCE_OUTPUT").get<int>();
    printColorSettings.hbuildErrorOutput = json.at("HBUILD_ERROR_OUTPUT").get<int>();
}

void to_json(Json &json, const GeneralPrintSettings &generalPrintSettings)
{
    json["PRE_BUILD_COMMANDS_STATEMENT"] = generalPrintSettings.preBuildCommandsStatement;
    json["PRE_BUILD_COMMANDS"] = generalPrintSettings.preBuildCommands;
    json["POST_BUILD_COMMANDS_STATEMENT"] = generalPrintSettings.postBuildCommandsStatement;
    json["POST_BUILD_COMMANDS"] = generalPrintSettings.postBuildCommands;
    json["COPYING_PACKAGE"] = generalPrintSettings.copyingPackage;
    json["COPYING_TARGET"] = generalPrintSettings.copyingTarget;
    json["THREAD_ID"] = generalPrintSettings.threadId;
}

void from_json(const Json &json, GeneralPrintSettings &generalPrintSettings)
{
    generalPrintSettings.preBuildCommandsStatement = json.at("PRE_BUILD_COMMANDS_STATEMENT").get<bool>();
    generalPrintSettings.preBuildCommands = json.at("PRE_BUILD_COMMANDS").get<bool>();
    generalPrintSettings.postBuildCommandsStatement = json.at("POST_BUILD_COMMANDS_STATEMENT").get<bool>();
    generalPrintSettings.postBuildCommands = json.at("POST_BUILD_COMMANDS").get<bool>();
    generalPrintSettings.copyingPackage = json.at("COPYING_PACKAGE").get<bool>();
    generalPrintSettings.copyingTarget = json.at("COPYING_TARGET").get<bool>();
    generalPrintSettings.threadId = json.at("THREAD_ID").get<bool>();
}

void to_json(Json &json, const Settings &settings)
{
    json["COMPILE_PRINT_SETTINGS"] = settings.ccpSettings;
    json["ARCHIVE_PRINT_SETTINGS"] = settings.acpSettings;
    json["LINK_PRINT_SETTINGS"] = settings.lcpSettings;
    json["PRINT_COLOR_SETTINGS"] = settings.pcSettings;
    json["GENERAL_PRINT_SETTINGS"] = settings.gpcSettings;
}

void from_json(const Json &json, Settings &settings)
{
    settings.ccpSettings = json.at("COMPILE_PRINT_SETTINGS").get<CompileCommandPrintSettings>();
    settings.acpSettings = json.at("ARCHIVE_PRINT_SETTINGS").get<ArchiveCommandPrintSettings>();
    settings.lcpSettings = json.at("LINK_PRINT_SETTINGS").get<LinkCommandPrintSettings>();
    settings.pcSettings = json.at("PRINT_COLOR_SETTINGS").get<PrintColorSettings>();
    settings.gpcSettings = json.at("GENERAL_PRINT_SETTINGS").get<GeneralPrintSettings>();
}

string file_to_string(const string &file_name)
{
    ifstream file_stream{file_name};

    if (file_stream.fail())
    {
        // Error opening file.
        cerr << "Error opening file in file_to_string function " << file_name << endl;
        exit(-1);
    }

    std::ostringstream str_stream{};
    file_stream >> str_stream.rdbuf(); // NOT str_stream << file_stream.rdbuf()

    if (file_stream.fail() && !file_stream.eof())
    {
        // Error reading file.
        cerr << "Error reading file in file_to_string function " << file_name << endl;
        exit(-1);
    }

    return str_stream.str();
}

vector<string> split(string str, const string &token)
{
    vector<string> result;
    while (!str.empty())
    {
        unsigned long index = str.find(token);
        if (index != string::npos)
        {
            result.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.empty())
            {
                result.push_back(str);
            }
        }
        else
        {
            result.push_back(str);
            str = "";
        }
    }
    return result;
}