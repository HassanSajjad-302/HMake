
#include "Configure.hpp"

#include "algorithm"
#include "fstream"
#include "iostream"
#include "set"

using std::to_string, std::filesystem::directory_entry, std::filesystem::file_type, std::logic_error, std::ifstream,
    std::ofstream, std::move, std::filesystem::current_path, std::cout, std::endl, std::cerr, std::stringstream,
    std::make_tuple, std::filesystem::directory_iterator, std::size_t, std::filesystem::remove;

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
File::File(path filePath_)
{
    if (filePath_.is_relative())
    {
        filePath_ = Cache::sourceDirectory.directoryPath / filePath_;
    }
    std::error_code ec;
    filePath = canonical(filePath_, ec);
    if (!ec)
    {
        // I don't check whether it is a directory or not.
    }
    else if (ec == std::errc::no_such_file_or_directory)
    {
        cerr << writePath(filePath_) << " Does Not Exist." << endl;
        exit(EXIT_FAILURE);
    }
    else
    {
        cerr << writePath(filePath_) << " Some Error Occurred." << endl;
        exit(EXIT_FAILURE);
    }
}

Directory::Directory(path directoryPath_)
{
    if (directoryPath_.is_relative())
    {
        directoryPath_ = Cache::sourceDirectory.directoryPath / directoryPath_;
    }
    std::error_code ec;
    directoryPath_ = canonical(directoryPath_, ec);
    if (!ec)
    {
        // I don't check whether it is a directory or not.
    }
    else if (ec == std::errc::no_such_file_or_directory)
    {
        cerr << writePath(directoryPath_) << " Does Not Exist." << endl;
        exit(EXIT_FAILURE);
    }
    else
    {
        cerr << writePath(directoryPath_) << " Some Error Occurred." << endl;
        exit(EXIT_FAILURE);
    }
    if (directory_entry(directoryPath_).status().type() == file_type::directory)
    {
        directoryPath = directoryPath_.lexically_normal();
    }
    else
    {
        cerr << writePath(directoryPath_) << " Is Not a directory file";
        exit(EXIT_FAILURE);
    }
    directoryPath /= "";
}

void to_json(Json &json, const Directory &directory)
{
    json = directory.directoryPath.string();
}

void from_json(const Json &json, Directory &directory)
{
    directory = Directory(path(json.get<string>()));
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
    json["PATH"] = writePath(buildTool.bTPath);
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

path Target::getTargetVariantDirectoryPath(int variantCount) const
{
    return Cache::configureDirectory.directoryPath / "Package" / to_string(variantCount) / targetName;
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

Json Target::convertToJson(int variantIndex) const
{
    Json targetFileJson;

    vector<string> sourceFilesArray;
    vector<Json> sourceDirectoriesArray;
    vector<string> includeDirectories;
    string compilerFlags;
    string linkerFlags;
    vector<Json> compileDefinitionsArray;
    vector<Json> dependenciesArray;

    for (const auto &e : sourceFiles)
    {
        sourceFilesArray.push_back(writePath(e.filePath));
    }
    for (const auto &e : sourceDirectories)
    {
        Json sourceDirectory;
        sourceDirectory["PATH"] = e.sourceDirectory.directoryPath;
        sourceDirectory["REGEX_STRING"] = e.regex;
        sourceDirectoriesArray.push_back(sourceDirectory);
    }
    for (const auto &e : includeDirectoryDependencies)
    {
        includeDirectories.push_back(writePath(e.includeDirectory.directoryPath));
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
                    includeDirectories.push_back(writePath(e.includeDirectory.directoryPath));
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
            libDepObject["PATH"] = Cache::configureDirectory.directoryPath / to_string(variantIndex) /
                                   library.targetName / path(library.targetName + ".hmake");

            dependenciesArray.push_back(libDepObject);
        }
        else
        {
            auto populateDependencies = [&](const PLibrary &pLibrary) {
                for (const auto &e : pLibrary.includeDirectoryDependencies)
                {
                    includeDirectories.push_back(writePath(e.directoryPath));
                }
                compilerFlags.append(" " + pLibrary.compilerFlagsDependencies + " ");
                linkerFlags.append(" " + pLibrary.linkerFlagsDependencies + " ");

                for (const auto &e : pLibrary.compileDefinitionDependencies)
                {
                    Json compileDefinitionObject = e;
                    compileDefinitionsArray.push_back(compileDefinitionObject);
                }
                Json libDepObject;
                libDepObject["PREBUILT"] = true;
                libDepObject["PATH"] = pLibrary.libraryPath;
                dependenciesArray.push_back(libDepObject);
            };
            if (libraryDependency->ldlt == LDLT::PLIBRARY)
            {
                const PLibrary &pLibrary = libraryDependency->pLibrary;
                populateDependencies(pLibrary);
            }
            else
            {
                const PLibrary &ppLibrary = libraryDependency->ppLibrary;
                populateDependencies(ppLibrary);
            }
        }
    }

    targetFileJson["TARGET_TYPE"] = targetType;
    targetFileJson["OUTPUT_NAME"] = outputName;
    targetFileJson["OUTPUT_DIRECTORY"] = writePath(outputDirectory.directoryPath);
    targetFileJson["CONFIGURATION"] = configurationType;
    targetFileJson["COMPILER"] = compiler;
    targetFileJson["LINKER"] = linker;
    targetFileJson["ARCHIVER"] = archiver;
    targetFileJson["ENVIRONMENT"] = environment;
    targetFileJson["COMPILER_FLAGS"] = flags.compilerFlags[compiler.bTFamily][configurationType];
    targetFileJson["LINKER_FLAGS"] = flags.linkerFlags[linker.bTFamily][configurationType];
    targetFileJson["SOURCE_FILES"] = sourceFilesArray;
    targetFileJson["SOURCE_DIRECTORIES"] = sourceDirectoriesArray;
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

void to_json(Json &j, const TargetType &p)
{
    if (p == TargetType::EXECUTABLE)
    {
        j = "EXECUTABLE";
    }
    else if (p == TargetType::STATIC)
    {
        j = "STATIC";
    }
    else if (p == TargetType::SHARED)
    {
        j = "SHARED";
    }
    else if (p == TargetType::PLIBRARY_SHARED)
    {
        j = "PLIBRARY_STATIC";
    }
    else
    {
        j = "PLIBRARY_SHARED";
    }
}

void Target::configure(int variantIndex) const
{
    path targetFileDir = Cache::configureDirectory.directoryPath / to_string(variantIndex) / targetName;
    create_directory(targetFileDir);
    if (outputDirectory.directoryPath.empty())
    {
        const_cast<Directory &>(outputDirectory) = Directory(targetFileDir);
    }

    Json targetFileJson;
    targetFileJson = convertToJson(variantIndex);

    path targetFilePath = targetFileDir / (targetName + ".hmake");
    ofstream(targetFilePath) << targetFileJson.dump(4);

    for (const auto &libDep : libraryDependencies)
    {
        if (libDep.ldlt == LDLT::LIBRARY)
        {
            libDep.library.setTargetType();
            libDep.library.configure(variantIndex);
        }
    }
}

Json Target::convertToJson(const Package &package, const PackageVariant &variant, int count) const
{
    Json targetFileJson;

    vector<string> sourceFilesArray;
    vector<Json> sourceDirectoriesArray;
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

    for (const auto &e : sourceFiles)
    {
        sourceFilesArray.push_back(writePath(e.filePath));
    }
    for (const auto &e : sourceDirectories)
    {
        Json sourceDirectory;
        sourceDirectory["PATH"] = e.sourceDirectory.directoryPath;
        sourceDirectory["REGEX_STRING"] = e.regex;
        sourceDirectoriesArray.push_back(sourceDirectory);
    }
    for (const auto &e : includeDirectoryDependencies)
    {
        Json JIDDObject;
        JIDDObject["PATH"] = writePath(e.includeDirectory.directoryPath);
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
                    JIDDObject["PATH"] = writePath(e.includeDirectory.directoryPath);
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
            consumeLibDepOject["IMPORTED"] = false;
            consumeLibDepOject["NAME"] = library.targetName;
            consumerDependenciesArray.push_back(consumeLibDepOject);
            libDepObject["PATH"] = library.getTargetVariantDirectoryPath(count) / (library.targetName + ".hmake");
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
                    JIDDObject["PATH"] = e.directoryPath;
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
                libDepObject["PATH"] = pLibrary.libraryPath;
                libDepObject["IMPORTED"] = false;
                libDepObject["HMAKE_FILE_PATH"] =
                    pLibrary.getTargetVariantDirectoryPath(count) / (pLibrary.libraryName + ".hmake");
                consumeLibDepOject["IMPORTED"] = false;
                consumeLibDepOject["NAME"] = pLibrary.libraryName;
            }
            else
            {
                const auto &ppLibrary = libraryDependency->ppLibrary;
                for (const auto &e : ppLibrary.includeDirectoryDependencies)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e.directoryPath;
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
                libDepObject["PATH"] = ppLibrary.libraryPath;
                libDepObject["IMPORTED"] = ppLibrary.imported;
                if (!ppLibrary.imported)
                {
                    libDepObject["HMAKE_FILE_PATH"] =
                        ppLibrary.getTargetVariantDirectoryPath(count) / (ppLibrary.libraryName + ".hmake");
                }

                consumeLibDepOject["IMPORTED"] = ppLibrary.imported;
                consumeLibDepOject["NAME"] = ppLibrary.libraryName;
                if (ppLibrary.imported)
                {
                    consumeLibDepOject["PACKAGE_NAME"] = ppLibrary.packageName;
                    consumeLibDepOject["PACKAGE_VERSION"] = ppLibrary.packageVersion;
                    consumeLibDepOject["PACKAGE_PATH"] = ppLibrary.packagePath;
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
    targetFileJson["OUTPUT_DIRECTORY"] = getTargetVariantDirectoryPath(count);
    targetFileJson["CONFIGURATION"] = configurationType;
    targetFileJson["COMPILER"] = compiler;
    targetFileJson["LINKER"] = linker;
    targetFileJson["ARCHIVER"] = archiver;
    targetFileJson["ENVIRONMENT"] = environment;
    targetFileJson["COMPILER_FLAGS"] = flags.compilerFlags[compiler.bTFamily][configurationType];
    targetFileJson["LINKER_FLAGS"] = flags.linkerFlags[linker.bTFamily][configurationType];
    targetFileJson["SOURCE_FILES"] = sourceFilesArray;
    targetFileJson["SOURCE_DIRECTORIES"] = sourceDirectoriesArray;
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

void Target::configure(const Package &package, const PackageVariant &variant, int count) const
{
    Json targetFileJson;
    targetFileJson = convertToJson(package, variant, count);
    path targetFileBuildDir = getTargetVariantDirectoryPath(count);
    create_directory(targetFileBuildDir);
    path filePath = targetFileBuildDir / (targetName + ".hmake");
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
        else if (!libDep.ppLibrary.imported)
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
    Cache::environment = Environment::initializeEnvironmentFromVSBatchCommand(
        "\"C:\\Program Files\\Microsoft Visual "
        "Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat\" amd64");
}

void Cache::registerCacheVariables()
{
    path filePath = current_path() / "cache.hmake";
    Json cacheFileJson;
    ifstream(filePath) >> cacheFileJson;
    cacheFileJson["CACHE_VARIABLES"] = Cache::cacheVariables;
    ofstream(filePath) << cacheFileJson.dump(2);
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

Json Variant::convertToJson(VariantMode mode, int variantCount)
{
    Json projectJson;
    projectJson["CONFIGURATION"] = configurationType;
    projectJson["COMPILER"] = compiler;
    projectJson["LINKER"] = linker;
    projectJson["ARCHIVER"] = archiver;
    string compilerFlags = flags.compilerFlags[compiler.bTFamily][configurationType];
    projectJson["COMPILER_FLAGS"] = compilerFlags;
    string linkerFlags = flags.linkerFlags[linker.bTFamily][configurationType];
    projectJson["LINKER_FLAGS"] = linkerFlags;
    projectJson["LIBRARY_TYPE"] = libraryType;
    projectJson["ENVIRONMENT"] = environment;

    vector<Json> targetArray;
    for (const auto &exe : executables)
    {
        string path;
        if (mode == VariantMode::PROJECT)
        {
            path = writePath((Cache::configureDirectory.directoryPath / to_string(variantCount) /
                              std::filesystem::path(exe.targetName) / (exe.targetName + ".hmake")));
        }
        else
        {
            path = writePath((exe.getTargetVariantDirectoryPath(variantCount) / (exe.targetName + ".hmake")));
        }
        targetArray.emplace_back(path);
    }
    for (const auto &lib : libraries)
    {
        string path;
        if (mode == VariantMode::PROJECT)
        {
            path = writePath((Cache::configureDirectory.directoryPath / to_string(variantCount) /
                              std::filesystem::path(lib.targetName) / (lib.targetName + ".hmake")));
        }
        else
        {
            path = writePath((lib.getTargetVariantDirectoryPath(variantCount) / (lib.targetName + ".hmake")));
        }
        targetArray.emplace_back(path);
    }
    projectJson["TARGETS"] = targetArray;
    return projectJson;
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
    ofstream(temporaryBatchFilename) << "call " + command + "\n" + "echo %INCLUDE% > " + temporaryIncludeFilename +
                                            "\n" + "echo %LIB%;%LIBPATH% > " + temporaryLibFilename;

    if (int code = system(temporaryBatchFilename.c_str()); code == EXIT_FAILURE)
    {
        cout << "Error in Executing Batch File" << endl;
        exit(-1);
    }
    remove(temporaryBatchFilename);

    auto splitPathsAndAssignToVector = [](string &accumulatedPaths, vector<Directory> &separatePaths) {
        size_t pos;
        while ((pos = accumulatedPaths.find(';')) != std::string::npos)
        {
            std::string token = accumulatedPaths.substr(0, pos);
            bool contains = false;
            for (const auto &i : separatePaths)
            {
                if (i.directoryPath == Directory(token).directoryPath)
                {
                    contains = true;
                    break;
                }
            }
            if (!contains)
            {
                separatePaths.emplace_back(token);
            }
            accumulatedPaths.erase(0, pos + 1);
        }
    };

    Environment environment;
    std::string accumulatedPaths = slurp(ifstream(temporaryIncludeFilename));
    remove(temporaryIncludeFilename);
    splitPathsAndAssignToVector(accumulatedPaths, environment.includeDirectories);
    accumulatedPaths = slurp(ifstream(temporaryLibFilename));
    remove(temporaryLibFilename);
    splitPathsAndAssignToVector(accumulatedPaths, environment.libraryDirectories);
    environment.compilerFlags = " /EHsc /MD /nologo";
    environment.linkerFlags = " /SUBSYSTEM:CONSOLE /NOLOGO";
    return environment;
}

void to_json(Json &j, const Environment &p)
{
    vector<string> temp;
    auto directoriesPathsToVector = [&temp](const vector<Directory> &outDirectoriesVector) {
        for (const auto &dir : outDirectoriesVector)
        {
            temp.emplace_back(writePath(dir.directoryPath));
        }
    };
    directoriesPathsToVector(p.includeDirectories);
    j["INCLUDE_DIRECTORIES"] = temp;
    temp.clear();
    directoriesPathsToVector(p.libraryDirectories);
    j["LIBRARY_DIRECTORIES"] = temp;
    j["COMPILER_FLAGS"] = p.compilerFlags;
    j["LINKER_FLAGS"] = p.linkerFlags;
}

void from_json(const Json &j, Environment &environment)
{
    environment.includeDirectories = j.at("INCLUDE_DIRECTORIES").get<vector<Directory>>();
    environment.libraryDirectories = j.at("LIBRARY_DIRECTORIES").get<vector<Directory>>();
    environment.compilerFlags = j.at("COMPILER_FLAGS").get<string>();
    environment.linkerFlags = j.at("LINKER_FLAGS").get<string>();
}

void Project::configure()
{
    for (size_t i = 0; i < projectVariants.size(); ++i)
    {
        Json json = projectVariants[i].convertToJson(VariantMode::PROJECT, i);
        path variantFileDir = Cache::configureDirectory.directoryPath / to_string(i);
        create_directory(variantFileDir);
        path projectFilePath = variantFileDir / "projectVariant.hmake";
        ofstream(projectFilePath) << json.dump(4);
        for (auto &exe : projectVariants[i].executables)
        {
            exe.configure(i);
        }
        for (auto &lib : projectVariants[i].libraries)
        {
            lib.setTargetType();
            lib.configure(i);
        }
    }
    Json projectFileJson;
    vector<string> projectVariantsInt;
    projectVariantsInt.reserve(projectVariants.size());
    for (size_t i = 0; i < projectVariants.size(); ++i)
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
        int indexInPackageCommonTargetsVector;
    };
    struct CommonIncludeDir
    {
        vector<int> variantsIndices;
        vector<Directory *> directories;
    };
    vector<NonCommonIncludeDir> packageNonCommonIncludeDirs;
    vector<CommonIncludeDir> packageCommonIncludeDirs;

    auto isDirectoryCommonOrNonCommon = [&](Directory *includeDirectory, int index) {
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

    auto assignPackageCommonAndNonCommonIncludeDirsForSingleTarget = [&](Target *target, int index) {
        for (auto &idd : target->includeDirectoryDependencies)
        {
            if (idd.dependencyType == DependencyType::PUBLIC)
            {
                isDirectoryCommonOrNonCommon(&(idd.includeDirectory), index);
            }
        }
    };

    auto assignPackageCommonAndNonCommonIncludeDirsForSinglePLibrary = [&](PLibrary *pLibrary, int index) {
        for (auto &id : pLibrary->includeDirectoryDependencies)
        {
            isDirectoryCommonOrNonCommon(&id, index);
        }
    };

    auto assignPackageCommonAndNonCommonIncludeDirs = [&](const Target *target, int index) {
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

    for (size_t i = 0; i < packageVariants.size(); ++i)
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
    for (size_t i = 0; i < packageCommonIncludeDirs.size(); ++i)
    {
        for (auto j : packageCommonIncludeDirs[i].directories)
        {
            j->isCommon = true;
            j->commonDirectoryNumber = i;
        }

        Json commonDirJsonObject;
        commonDirJsonObject["INDEX"] = i;
        commonDirJsonObject["PATH"] = packageCommonIncludeDirs[i].directories[0]->directoryPath;
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
        if (i.json.contains("INDEX"))
        {
            cerr << "Package Variant Json can not have COUNT in it's Json." << endl;
            exit(EXIT_FAILURE);
        }
        i.json["INDEX"] = to_string(count);
        packageVariantJson.emplace_back(i.json);
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
        Json variantJsonFile = variant.convertToJson(VariantMode::PACKAGE, count);
        path variantFileDir = packageDirectory.directoryPath / to_string(count);
        create_directory(variantFileDir);
        path variantFilePath = variantFileDir / "packageVariant.hmake";
        ofstream(variantFilePath) << variantJsonFile.dump(4);
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
    if (libraryType == LibraryType::STATIC)
    {
        libraryName = libraryPath.filename().string();
        libraryName.erase(0, 3);
        libraryName.erase(libraryName.find('.'), 2);
    }
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

Json PLibrary::convertToJson(const Package &package, const PackageVariant &variant, int count) const
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
        JIDDObject["PATH"] = writePath(e.directoryPath);
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
                    JIDDObject["PATH"] = writePath(e.includeDirectory.directoryPath);
                    JIDDObject["COPY"] = false;
                    includeDirectoriesArray.push_back(JIDDObject);
                }
            }

            Json libDepObject;
            libDepObject["PREBUILT"] = false;
            Json consumeLibDepOject;
            consumeLibDepOject["IMPORTED"] = false;
            consumeLibDepOject["NAME"] = library.targetName;
            consumerDependenciesArray.push_back(consumeLibDepOject);
            libDepObject["PATH"] = library.getTargetVariantDirectoryPath(count) / (library.targetName + ".hmake");
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
                    JIDDObject["PATH"] = e.directoryPath;
                    JIDDObject["COPY"] = false;

                    includeDirectoriesArray.push_back(JIDDObject);
                }

                libDepObject["PATH"] = pLibrary.libraryPath;
                libDepObject["IMPORTED"] = false;
                libDepObject["HMAKE_FILE_PATH"] =
                    pLibrary.getTargetVariantDirectoryPath(count) / (pLibrary.libraryName + ".hmake");
                consumeLibDepOject["IMPORTED"] = false;
                consumeLibDepOject["NAME"] = pLibrary.libraryName;
            }
            else
            {
                const auto &ppLibrary = libraryDependency->ppLibrary;
                for (const auto &e : ppLibrary.includeDirectoryDependencies)
                {
                    Json JIDDObject;
                    JIDDObject["PATH"] = e.directoryPath;
                    JIDDObject["COPY"] = false;
                    includeDirectoriesArray.push_back(JIDDObject);
                }

                libDepObject["PATH"] = ppLibrary.libraryPath;
                libDepObject["IMPORTED"] = ppLibrary.imported;
                if (!ppLibrary.imported)
                {
                    libDepObject["HMAKE_FILE_PATH"] =
                        ppLibrary.getTargetVariantDirectoryPath(count) / (ppLibrary.libraryName + ".hmake");
                }

                consumeLibDepOject["IMPORTED"] = ppLibrary.imported;
                consumeLibDepOject["NAME"] = ppLibrary.libraryName;
                if (ppLibrary.imported)
                {
                    consumeLibDepOject["PACKAGE_NAME"] = ppLibrary.packageName;
                    consumeLibDepOject["PACKAGE_VERSION"] = ppLibrary.packageVersion;
                    consumeLibDepOject["PACKAGE_PATH"] = ppLibrary.packagePath;
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

void PLibrary::configure(const Package &package, const PackageVariant &variant, int count) const
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
        else if (!libDep.ppLibrary.imported)
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
        bool isImported = i.at("IMPORTED").get<bool>();
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
    libraryPath = libraryDirectoryPath / path("lib" + libraryName + ".a");
    packageName = cPackage.name;
    packageVersion = cPackage.version;
    packagePath = cPackage.packagePath;
    packageVariantJson = cpVariant.variantJson;
    index = cpVariant.index;
}

CPVariant::CPVariant(path variantPath_, Json variantJson_, int index_)
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
    for (size_t i = 0; i < variantsJson.size(); ++i)
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
        cerr << "No Json in package " << writePath(packagePath) << " matches \n" << variantJson_.dump(2) << endl;
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

string slurp(const std::ifstream &in)
{
    std::ostringstream str;
    str << in.rdbuf();
    return str.str();
};