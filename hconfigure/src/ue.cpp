#include "ue.hpp"
#include "BuildSystemFunctions.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

/*
 * Runtime implementation of the UE-oriented layer declared in ue.hpp.
 *
 * A useful UBT-to-HMake mental model is:
 *
 *   RulesAssembly registration        -> addSpecifyFunc()
 *   TargetRules/ModuleRules execution  -> getOrAddTarget()/specify()
 *   UEBuildTarget module cache         -> configuredTargets/getOrAddTarget()
 *   UEBuildModuleCPP                   -> UeCppTarget plus DSC<UeCppTarget>
 *   ReadOnlyTargetRules                -> values/evaluate() on UeConfiguration
 *
 * These are conceptual mappings. HMake retains its own graph and target types
 * instead of reproducing UBT's object hierarchy.
 */
namespace
{
flat_hash_map<string, UeSpecifyFunctionSet> specifyFunctionSets;
bool specifyFunctionsRegistered = false;

class UePathNormalizationScope
{
    string_view previousBase;

  public:
    explicit UePathNormalizationScope(const Node *file) : previousBase(normalizationBasePath)
    {
        normalizationBasePath = file->getDirectoryStringView();
    }

    ~UePathNormalizationScope()
    {
        normalizationBasePath = previousBase;
    }
};

struct ParsedUeFile
{
    string_view logicalName;
    UeFileKind kind;
    std::optional<UePlatformGroup> platformGroup;
    std::optional<UePlatform> platform;
};

UePlatform parsePlatform(const string_view name, const string_view file)
{
    if (name == "Linux")
        return UePlatform::Linux;
    if (name == "Windows" || name == "Win64")
        return UePlatform::Windows;
    if (name == "Mac")
        return UePlatform::Mac;
    if (name == "Android")
        return UePlatform::Android;
    if (name == "IOS")
        return UePlatform::IOS;
    printErrorMessage(FORMAT("Unknown UE platform '{}' in generated file registration.\nFile: {}", name, file));
}

UePlatformGroup parsePlatformGroup(const string_view name, const string_view file)
{
    if (name == "Unix")
        return UePlatformGroup::Unix;
    if (name == "Windows")
        return UePlatformGroup::Windows;
    if (name == "Microsoft")
        return UePlatformGroup::Microsoft;
    if (name == "Apple")
        return UePlatformGroup::Apple;
    if (name == "Desktop")
        return UePlatformGroup::Desktop;
    if (name == "Linux")
        return UePlatformGroup::Linux;
    if (name == "Android")
        return UePlatformGroup::Android;
    printErrorMessage(FORMAT("Unknown UE platform group '{}' in generated file registration.\nFile: {}", name, file));
}

ParsedUeFile parseUeFile(const string_view file)
{
    constexpr string_view hmakeSuffix = ".hmake.hpp";
    constexpr string_view moduleSuffix = ".module";
    constexpr string_view prebuiltSuffix = ".prebuilt";
    constexpr string_view targetSuffix = ".target";
    constexpr string_view groupMarker = ".group.";
    constexpr string_view platformMarker = ".platform.";

    const size_t separator = file.find_last_of("/\\");
    string_view name = separator == string_view::npos ? file : file.substr(separator + 1);
    if (!name.ends_with(hmakeSuffix))
    {
        printErrorMessage(FORMAT("Generated UE file does not end in '.hmake.hpp'.\nFile: {}", file));
    }
    name.remove_suffix(hmakeSuffix.size());

    UeFileKind kind;
    if (name.ends_with(moduleSuffix))
    {
        kind = UeFileKind::Module;
        name.remove_suffix(moduleSuffix.size());
    }
    else if (name.ends_with(prebuiltSuffix))
    {
        kind = UeFileKind::Prebuilt;
        name.remove_suffix(prebuiltSuffix.size());
    }
    else if (name.ends_with(targetSuffix))
    {
        kind = UeFileKind::Target;
        name.remove_suffix(targetSuffix.size());
    }
    else
    {
        printErrorMessage(
            FORMAT("Generated UE filename must contain '.module', '.prebuilt', or '.target'.\nFile: {}", file));
    }

    string_view logicalName = name;
    std::optional<UePlatformGroup> parsedPlatformGroup;
    std::optional<UePlatform> parsedPlatform;
    const size_t group = name.rfind(groupMarker);
    const size_t platform = name.rfind(platformMarker);
    if (group != string_view::npos && platform != string_view::npos)
    {
        printErrorMessage(FORMAT("Generated UE filename contains both group and platform selectors.\nFile: {}", file));
    }
    if (group != string_view::npos)
    {
        logicalName = name.substr(0, group);
        parsedPlatformGroup = parsePlatformGroup(name.substr(group + groupMarker.size()), file);
    }
    else if (platform != string_view::npos)
    {
        logicalName = name.substr(0, platform);
        parsedPlatform = parsePlatform(name.substr(platform + platformMarker.size()), file);
    }

    if (logicalName.empty())
    {
        printErrorMessage(FORMAT("Generated UE filename has no logical module or target name.\nFile: {}", file));
    }
    return {.logicalName = logicalName, .kind = kind, .platformGroup = parsedPlatformGroup, .platform = parsedPlatform};
}

string makeApiMacro(const string_view logicalName)
{
    // UBT's UEBuildModule constructor performs the same basic transformation:
    // module "CoreUObject" owns the compile-time macro "COREUOBJECT_API".
    string result;
    result.reserve(logicalName.size() + 4);
    for (const unsigned char c : logicalName)
    {
        result += std::isalnum(c) ? static_cast<char>(std::toupper(c)) : '_';
    }
    result += "_API";
    return result;
}
} // namespace

namespace
{
[[noreturn]] void reportDuplicateSpecifyFunc(const UeSpecifyFunctionSet &set, const string_view selector,
                                             const Node *firstFile, const Node *secondFile)
{
    printErrorMessage(
        FORMAT("Duplicate specialized UE specify function.\nTarget: {}\nSelector: {}\nFirst: {}\nSecond: {}",
               set.logicalName, selector, firstFile->filePath, secondFile->filePath));
}

void addSpecifyFunc(string logicalName, const UeFileKind kind, const std::optional<UePlatformGroup> platformGroup,
                    const std::optional<UePlatform> platform, const UeSpecifyFunction func, Node *file)
{
    // registerGeneratedUeSpecifyFuncs() calls this once per discovered *.hmake.hpp
    // file. UBT obtains comparable metadata while RulesCompiler builds a
    // RulesAssembly containing the classes declared by *.Build.cs and *.Target.cs.
    if (logicalName.empty() || func == nullptr || file == nullptr)
    {
        printErrorMessage("A UE specify-function registration requires a target name, function, and file.");
    }
    if (platformGroup && platform)
    {
        printErrorMessage(
            FORMAT("UE specify function '{}' has both a platform group and exact platform.", logicalName));
    }

    auto [iterator, inserted] = specifyFunctionSets.try_emplace(logicalName);
    UeSpecifyFunctionSet &set = iterator->second;
    if (inserted)
    {
        // First registration establishes whether this logical name is a ModuleRules-
        // like module or a TargetRules-like top-level target.
        set.logicalName = std::move(logicalName);
        set.kind = kind;
    }
    else if (set.kind != kind)
    {
        printErrorMessage(
            FORMAT("UE specification '{}' was registered as both a module and a target.", set.logicalName));
    }

    if (!platformGroup && !platform)
    {
        // Exactly one unsuffixed specification is required, matching UBT's required
        // base ModuleName/TargetName rules type.
        if (set.base)
        {
            printErrorMessage(FORMAT("Duplicate base UE specify function.\nTarget: {}\nFirst: {}\nSecond: {}",
                                     set.logicalName, set.base->file->filePath, file->filePath));
        }
        set.base = UeSpecifyFunctionBase{.file = file, .func = func};
        return;
    }

    // Specialized registrations correspond to UBT classes named approximately
    // ModuleName_<Group> and ModuleName_<Platform>.
    if (platformGroup)
    {
        for (const UePlatformGroupSpecifyFunc &existing : set.platformGroups)
        {
            if (existing.platformGroup == *platformGroup)
            {
                reportDuplicateSpecifyFunc(set, FORMAT("platform group {}", static_cast<uint8_t>(*platformGroup)),
                                           existing.file, file);
            }
        }
        UePlatformGroupSpecifyFunc entry;
        entry.file = file;
        entry.func = func;
        entry.platformGroup = *platformGroup;
        set.platformGroups.emplace_back(entry);
        return;
    }

    for (const UePlatformSpecifyFunc &existing : set.platforms)
    {
        if (existing.platform == *platform)
        {
            reportDuplicateSpecifyFunc(set, FORMAT("platform {}", static_cast<uint8_t>(*platform)), existing.file,
                                       file);
        }
    }
    UePlatformSpecifyFunc entry;
    entry.file = file;
    entry.func = func;
    entry.platform = *platform;
    set.platforms.emplace_back(entry);
}

} // namespace

void registerGeneratedUeSpecifyFuncs(const std::span<const UeIncludedFile> files)
{
    if (specifyFunctionsRegistered)
    {
        return;
    }

    specifyFunctionSets.reserve(files.size());
    for (const auto &[path, func] : files)
    {
        if (path.empty() || func == nullptr)
        {
            printErrorMessage("A generated UE file registration requires a path and specify function.");
        }

        const auto [logicalName, kind, platformGroup, platform] = parseUeFile(path);
        Node *file = Node::getNodeNonNormalized(string(path), true);
        addSpecifyFunc(string(logicalName), kind, platformGroup, platform, func, file);
    }
    specifyFunctionsRegistered = true;
}

UeCppTarget::UeCppTarget(const string &hmakeName, string logicalName_, UeConfiguration *configuration)
    : CppTarget(hmakeName, configuration), logicalName(std::move(logicalName_))
{
    bTargetType = BTargetType::UE_CPP_TARGET;
    intermediateName = logicalName;
}

UeCppTarget &UeCppTarget::setShortName(const string_view value)
{
    intermediateName = value;
    return *this;
}

bool UeCppTarget::conditionalAddModuleDirectory(const NodeOrStr directory)
{
    const string directoryPath =
        directory.hasNode_ ? directory.node_->filePath : getNormalizedPath(path(directory.str_));
    if (!std::filesystem::is_directory(directoryPath))
    {
        return false;
    }

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *directoryNode = Node::getNodeNonNormalized(directoryPath, false);
        if (std::ranges::find(moduleDirectories, directoryNode) == moduleDirectories.end())
        {
            moduleDirectories.emplace_back(directoryNode);
        }
    }
    return true;
}

UeCppTarget &UeCppTarget::addPrivateCycleDependency(const string_view dependency)
{
    UeCppTarget &dependencyTarget =
        static_cast<UeConfiguration *>(configuration)->getOrAddTarget(dependency).getSourceTarget();
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        reqDeps.emplace(&dependencyTarget);
    }
    return *this;
}

UeCppTarget &UeCppTarget::addPublicCycleDependency(const string_view dependency)
{
    UeCppTarget &dependencyTarget =
        static_cast<UeConfiguration *>(configuration)->getOrAddTarget(dependency).getSourceTarget();
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        reqDeps.emplace(&dependencyTarget);
        useReqDeps.emplace(&dependencyTarget);
    }
    return *this;
}

void UeCppTarget::propagateSelectiveBuild()
{
    // Builder::setSelectiveBuild() may run after a parent reached this target
    // recursively and reset selectiveBuild. Preserve that earlier propagated state.
    if (selectiveBuildSet)
    {
        selectiveBuild = true;
        return;
    }
    if (!selectiveBuild)
    {
        return;
    }

    selectiveBuildSet = true;
    const auto propagate = [](CppTarget *dependency) {
        if (dependency != nullptr && dependency->bTargetType == BTargetType::UE_CPP_TARGET)
        {
            auto &ueDependency = static_cast<UeCppTarget &>(*dependency);
            ueDependency.selectiveBuild = true;
            ueDependency.propagateSelectiveBuild();
        }
    };

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (CppTarget *dependency : reqDeps)
        {
            propagate(dependency);
        }
    }
    else
    {
        for (const uint32_t dependencyIndex : cachedReqDeps)
        {
            propagate(static_cast<CppTarget *>(bTargetCaches[dependencyIndex].bTarget));
        }
    }
}

void UeCppTarget::completeRoundOne()
{
    // selectiveBuild belongs to round 0, but explicit UE cycle dependencies have
    // no RealBTarget edge through which Builder could propagate it. Do it before
    // the completion guard so a later selected parent can still reach this target.
    propagateSelectiveBuild();

    if (roundOneCalled)
    {
        return;
    }
    roundOneCalled = true;

    const auto complete = [](CppTarget *dependency) {
        if (dependency != nullptr && dependency->bTargetType == BTargetType::UE_CPP_TARGET)
        {
            static_cast<UeCppTarget *>(dependency)->completeRoundOne();
        }
    };

    // Ordinary dependencies have already completed through scheduler ordering.
    // An explicit cycle dependency has no scheduler edge, so this guarded walk
    // completes it here. Marking roundOneCalled before recursion breaks the cycle.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (CppTarget *dependency : reqDeps)
        {
            complete(dependency);
        }
    }
    else
    {
        for (const uint32_t dependencyIndex : cachedReqDeps)
        {
            complete(static_cast<CppTarget *>(bTargetCaches[dependencyIndex].bTarget));
        }
    }

    CppTarget::completeRoundOne();
}

void UeCppTarget::prepareModuleInputs(const bool compileSources)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (Node *moduleDirectory : moduleDirectories)
        {
            if (compileSources)
            {
                findInputFiles(moduleDirectory);
            }
            if (bAddDefaultIncludePaths)
            {
                addDefaultIncludePaths(moduleDirectory);
            }
        }

        const auto &ueConfiguration = *static_cast<UeConfiguration *>(configuration);
        if (ueConfiguration.generatedIncludeRoot != nullptr)
        {
            const path moduleGeneratedRoot = path(ueConfiguration.generatedIncludeRoot->filePath) / intermediateName;
            const path uhtDirectory = moduleGeneratedRoot / "UHT";
            if (std::filesystem::is_directory(uhtDirectory))
            {
                addGeneratedCode(Node::getNodeNonNormalized(uhtDirectory.string(), false), compileSources);
            }

            // VNI headers are generated beside UHT output. UBT adds this directory
            // to the module compile environment but does not compile sources from it.
            const path vniDirectory = moduleGeneratedRoot / "VNI";
            if (std::filesystem::is_directory(vniDirectory))
            {
                publicIncludesSource(Node::getNodeNonNormalized(vniDirectory.string(), false));
            }
        }
    }
}

void UeCppTarget::findInputFiles(Node *moduleDirectory)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        // UBT equivalent: UEBuildModuleCPP.FindInputFiles() and
        // FindInputFilesFromDirectoryRecursive(). A .module.hmake.hpp user does
        // not list ordinary module sources.
        if (std::filesystem::exists(path(moduleDirectory->filePath) / ".ubtignore"))
        {
            return;
        }

        const auto &ueConfiguration = *static_cast<UeConfiguration *>(configuration);
        const auto shouldSkipDirectory = [&ueConfiguration](const string_view directoryName) {
            if (directoryName == "Android")
                return !ueConfiguration.evaluate(UePlatform::Android) &&
                       !ueConfiguration.evaluate(UePlatformGroup::Android);
            if (directoryName == "Apple")
                return !ueConfiguration.evaluate(UePlatformGroup::Apple);
            if (directoryName == "IOS")
                return !ueConfiguration.evaluate(UePlatform::IOS);
            if (directoryName == "Linux")
                return !ueConfiguration.evaluate(UePlatform::Linux) &&
                       !ueConfiguration.evaluate(UePlatformGroup::Linux);
            if (directoryName == "Mac")
                return !ueConfiguration.evaluate(UePlatform::Mac);
            if (directoryName == "Microsoft")
                return !ueConfiguration.evaluate(UePlatformGroup::Microsoft);
            if (directoryName == "Unix")
                return !ueConfiguration.evaluate(UePlatformGroup::Unix);
            if (directoryName == "Windows" || directoryName == "Win64")
                return !ueConfiguration.evaluate(UePlatform::Windows) &&
                       !ueConfiguration.evaluate(UePlatformGroup::Windows);
            if (directoryName == "Desktop")
                return !ueConfiguration.evaluate(UePlatformGroup::Desktop);

            return directoryName == "FreeBSD" || directoryName == "HoloLens" || directoryName == "PS4" ||
                   directoryName == "PS5" || directoryName == "Switch" || directoryName == "TVOS" ||
                   directoryName == "VisionOS" || directoryName == "XboxOne" || directoryName == "XSX";
        };

        std::filesystem::recursive_directory_iterator iterator(moduleDirectory->filePath);
        const std::filesystem::recursive_directory_iterator end;
        while (iterator != end)
        {
            if (iterator->is_directory() && (shouldSkipDirectory(iterator->path().filename().string()) ||
                                             std::filesystem::exists(iterator->path() / ".ubtignore")))
            {
                iterator.disable_recursion_pending();
            }
            else if (iterator->is_regular_file() && !iterator->path().filename().string().starts_with('.'))
            {
                const string fileName = iterator->path().filename().string();
                const string extension = iterator->path().extension().string();
                const bool isUeSource =
                    extension == ".cpp" || extension == ".c" || extension == ".cc" || extension == ".cxx";
                if (isUeSource && !fileName.ends_with(".gen.cpp"))
                {
                    if (extension == ".cpp")
                    {
                        // UBT's SourceFileMetadataCache records each
                        //   #include UE_INLINE_GENERATED_CPP_BY_NAME(Name)
                        // and UEBuildModuleCPP removes Name.gen.cpp from the list of
                        // independently compiled generated files. Keep the same split
                        // so generated code sees any prerequisite includes/declarations
                        // supplied by its owning handwritten translation unit.
                        constexpr string_view marker = "UE_INLINE_GENERATED_CPP_BY_NAME(";
                        std::ifstream source(iterator->path());
                        string line;
                        while (std::getline(source, line))
                        {
                            const size_t markerPosition = line.find(marker);
                            if (markerPosition == string::npos)
                            {
                                continue;
                            }

                            const size_t hashPosition = line.find('#');
                            const size_t includePosition = line.find("include", hashPosition);
                            if (hashPosition == string::npos || includePosition == string::npos ||
                                includePosition > markerPosition)
                            {
                                continue;
                            }

                            const size_t nameBegin = markerPosition + marker.size();
                            const size_t nameEnd = line.find(')', nameBegin);
                            if (nameEnd == string::npos)
                            {
                                continue;
                            }

                            string_view name(line.data() + nameBegin, nameEnd - nameBegin);
                            while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
                            {
                                name.remove_prefix(1);
                            }
                            while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
                            {
                                name.remove_suffix(1);
                            }
                            if (!name.empty())
                            {
                                inlinedGeneratedCppNames.emplace(name);
                            }
                        }
                    }

                    // moduleFiles() falls back to sourceFiles() for IsCppMod::NO.
                    moduleFiles(Node::getNodeNonNormalized(iterator->path().string(), true));
                }
            }
            ++iterator;
        }
    }
}

void UeCppTarget::addDefaultIncludePaths(Node *moduleDirectory)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        // UBT equivalent: UEBuildModuleCPP.AddDefaultIncludePaths(). These paths
        // are conventions inferred by the backend, not declarations users repeat
        // in Build.cs files.
        const auto addDirectoryIfPresent = [this, moduleDirectory](const string_view name, const bool isPublic) {
            const path directoryPath = path(moduleDirectory->filePath) / name;
            if (!std::filesystem::is_directory(directoryPath))
            {
                return;
            }

            Node *includeDirectory = Node::getNodeNonNormalized(directoryPath.string(), false);
            if (isPublic)
            {
                publicIncludesSource(includeDirectory);
            }
            else
            {
                privateIncludesSource(includeDirectory);
            }
        };

        // Private must precede Public because UE modules can contain both a private
        // and public header with the same include name (for example Core's
        // HAL/ConsoleManager.h). UBT's private compile environment resolves the
        // module-private header first.
        addDirectoryIfPresent("Private", false);

        // Classes is UBT's legacy public UObject-header directory. Public is a
        // normal propagated include. UBT exposes Internal to consumers in the
        // same rules scope (and to engine modules). All modules in this first
        // engine-only stage are engine modules, so propagation is the correct
        // effective behavior; a later project/plugin scope model must narrow it.
        // Private is always local to this module.
        addDirectoryIfPresent("Classes", true);
        addDirectoryIfPresent("Public", true);
        addDirectoryIfPresent("Internal", true);
    }
}

UeCppTarget &UeCppTarget::addGeneratedCode(Node *directory, const bool compileSources)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        publicIncludesSource(directory);

        if (compileSources)
        {
            for (const std::filesystem::directory_entry &entry :
                 std::filesystem::recursive_directory_iterator(directory->filePath))
            {
                if (entry.is_regular_file() && entry.path().filename().string().ends_with(".gen.cpp"))
                {
                    const string fileName = entry.path().filename().string();
                    const string generatedName = fileName.substr(0, fileName.size() - string_view(".gen.cpp").size());
                    if (inlinedGeneratedCppNames.contains(generatedName))
                    {
                        continue;
                    }

                    // UBT compiles generated implementations not claimed by an
                    // UE_INLINE_GENERATED_CPP_BY_NAME include as separate inputs.
                    // This includes each module's *.init.gen.cpp.
                    moduleFiles(Node::getNodeNonNormalized(entry.path().string(), true));
                }
            }
        }
    }
    return *this;
}

UeConfiguration::UeConfiguration(const string &name) : Configuration(name)
{
}

void UeConfiguration::initialize()
{
    Configuration::initialize();
    if (buildCommands)
    {
        if (!buildCommands->cppCompileCommand.empty())
            cppCompileCommand = buildCommands->cppCompileCommand;
        if (!buildCommands->cCompileCommand.empty())
            cCompileCommand = buildCommands->cCompileCommand;
        if (!buildCommands->linkCommand.empty())
            linkCommand = buildCommands->linkCommand;
        linkDependenciesPrefix = buildCommands->linkDependenciesPrefix;
        linkCommandSuffix = buildCommands->linkCommandSuffix;
        if (!buildCommands->archiveCommand.empty())
            archiveCommand = buildCommands->archiveCommand;
    }
}

UeConfiguration &UeConfiguration::requestTarget(string logicalName)
{
    requestedTargets.emplace_back(std::move(logicalName));
    return *this;
}

UeConfiguration &UeConfiguration::setPlatform(const UePlatform value, vector<UePlatformGroup> groups)
{
    // UBT derives groups from its registered UEBuildPlatform. HMake accepts them
    // explicitly for now so the initial implementation needs no platform registry.
    platform = value;
    if (!groups.empty())
    {
        platformGroups = std::move(groups);
    }
    return *this;
}

UeConfiguration &UeConfiguration::setArchitecture(const UeArchitecture value)
{
    architecture = value;
    return *this;
}

UeConfiguration &UeConfiguration::setBuildConfiguration(const UeBuildConfiguration value)
{
    buildConfiguration = value;
    return *this;
}

UeConfiguration &UeConfiguration::setUeTargetType(const UeTargetType value)
{
    ueTargetType = value;
    return *this;
}

UeConfiguration &UeConfiguration::setGeneratedIncludeRoot(Node *value)
{
    generatedIncludeRoot = value;
    return *this;
}

UeConfiguration &UeConfiguration::setBuildCommands(UeBuildCommands value)
{
    buildCommands = std::move(value);
    return *this;
}

UeConfiguration &UeConfiguration::setBuildCommands(const std::span<const UeBuildCommandEntry> entries)
{
    const auto entry = std::ranges::find_if(entries, [this](const UeBuildCommandEntry &candidate) {
        return candidate.platform == platform && candidate.architecture == architecture &&
               candidate.buildConfiguration == buildConfiguration && candidate.targetType == ueTargetType;
    });
    if (entry == entries.end())
    {
        printErrorMessage(FORMAT("No UE build-command row matches configuration.\nConfiguration: {}\nPlatform: {}\n"
                                 "Architecture: {}\nBuild configuration: {}\nTarget type: {}",
                                 name, static_cast<uint8_t>(platform), static_cast<uint8_t>(architecture),
                                 static_cast<uint8_t>(buildConfiguration), static_cast<uint8_t>(ueTargetType)));
    }
    return setBuildCommands(entry->commands);
}

bool UeConfiguration::evaluate(const UePlatform value) const
{
    return platform == value;
}

bool UeConfiguration::evaluate(const UePlatformGroup value) const
{
    // Equivalent to the common UBT rule:
    // Target.Platform.IsInGroup(UnrealPlatformGroup.X)
    return std::ranges::find(platformGroups, value) != platformGroups.end();
}

bool UeConfiguration::evaluate(const UeArchitecture value) const
{
    return architecture == value;
}

bool UeConfiguration::evaluate(const UeBuildConfiguration value) const
{
    return buildConfiguration == value;
}

bool UeConfiguration::evaluate(const UeTargetType value) const
{
    return ueTargetType == value;
}

DSC<UeCppTarget> &UeConfiguration::currentTarget() const
{
    // specify() receives the configuration so all decentralized functions have the
    // same signature. The stack supplies the ModuleRules-like object currently
    // being populated, including during recursive dependency configuration.
    if (currentTargetStack.empty())
    {
        printErrorMessage(FORMAT("No UE target is currently being configured.\nConfiguration: {}", name));
    }
    return *currentTargetStack.back();
}

void UeConfiguration::initializeApiMacro(DSC<UeCppTarget> &target, const bool defines) const
{
    // UBT calls this name ModuleApiDefine. A module compiling its own shared library
    // sees dllexport/default visibility; static/monolithic builds see an empty macro.
    // Consumer-side dllimport selection is handled by HMake's DSC/output relationship.
    if (!defines)
    {
        return;
    }

    Define definition(target.define, "");
    if (target.ploat && target.ploat->evaluate(TargetType::LIBRARY_SHARED))
    {
        if (compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
        {
            definition.value = "__declspec(dllexport)";
        }
        else
        {
            definition.value = "\"__attribute__ ((visibility (\\\"default\\\")))\"";
        }
    }
    target.getSourceTarget().reqCompileDefinitions.emplace(std::move(definition));
}

PLOAT &UeConfiguration::getOrAddPrebuiltLibrary(Node *libraryFile, const TargetType libraryType)
{
    const string key = FORMAT("{}:{}", static_cast<uint8_t>(libraryType), libraryFile->filePath);
    if (const auto existing = prebuiltLibraries.find(key); existing != prebuiltLibraries.end())
    {
        return *existing->second;
    }

    const path libraryPath(libraryFile->filePath);
    const string fileName = libraryPath.filename().string();
    string outputName;
    if (libraryType == TargetType::PLIBRARY_STATIC)
    {
        if (fileName.starts_with("lib") && fileName.ends_with(".a"))
        {
            outputName = fileName.substr(3, fileName.size() - 5);
        }
        else if (fileName.ends_with(".lib"))
        {
            outputName = fileName.substr(0, fileName.size() - 4);
        }
    }
    else if (libraryType == TargetType::PLIBRARY_SHARED)
    {
        if (fileName.starts_with("lib") && fileName.ends_with(".so"))
        {
            outputName = fileName.substr(3, fileName.size() - 6);
        }
        else if (fileName.starts_with("lib") && fileName.ends_with(".dylib"))
        {
            outputName = fileName.substr(3, fileName.size() - 9);
        }
        else if (fileName.ends_with(".dll"))
        {
            outputName = fileName.substr(0, fileName.size() - 4);
        }
    }

    if (outputName.empty())
    {
        printErrorMessage(FORMAT("Unsupported prebuilt UE library filename.\nLibrary: {}\nType: {}\n"
                                 "Expected conventional lib<name>.a/.so/.dylib, <name>.lib, or <name>.dll naming.",
                                 libraryFile->filePath, static_cast<uint8_t>(libraryType)));
    }

    Node *directory = Node::getNodeNonNormalized(libraryPath.parent_path().string(), false);
    PLOAT &library = targets<PLOAT>.emplace_back(*this, outputName, directory, libraryType,
                                                 name + slashc + "prebuilt-" + std::to_string(prebuiltLibraries.size()),
                                                 false, false);
    ploats.emplace_back(&library);
    prebuiltLibraries.emplace(key, &library);
    return library;
}

DSC<UeCppTarget> &UeConfiguration::makeDscUeCppTarget(string logicalName, const UeFileKind fileKind)
{
    // At this point the scanner registry has selected a logical rules declaration,
    // but its specify() functions have not yet populated the target.
    UeCppTarget &cppTarget =
        targets<UeCppTarget>.emplace_back(name + slashc + logicalName + dashCpp, logicalName, this);
    cppTarget.isSystem = fileKind == UeFileKind::Prebuilt;
    // Stage one consumes no ISPC outputs. Keep this on each UE target rather than
    // HMake's standard-library target; UBT's bundled libc++ command intentionally
    // disables that host target for the UE bootstrap.
    cppTarget.privateCompileDefines("INTEL_ISPC", "0");
    cppTargets.emplace_back(&cppTarget);
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(UseConfigurationScope::YES))
        {
            realBTargets[1].addDep<BTargetType::CPP_TARGET>(&cppTarget.realBTargets[1]);
        }
    }

    const bool defines = fileKind == UeFileKind::Module;
    const string apiMacro = defines ? makeApiMacro(logicalName) : string();

    PLOAT *output = nullptr;
    if (fileKind == UeFileKind::Target)
    {
        // The top-level target owns the one monolithic executable output.
        output = &GetExeLOAT(logicalName);
    }
    else if (fileKind == UeFileKind::Module && targetType == TargetType::LIBRARY_SHARED)
    {
        // Modular mode remains one shared output per C++ module for now.
        output = &getSharedLOAT(logicalName);
    }
    else if (fileKind != UeFileKind::Prebuilt && targetType != TargetType::LIBRARY_STATIC)
    {
        printErrorMessage(
            FORMAT("Unsupported HMake target type for a UE module.\nConfiguration: {}\nModule: {}", name, logicalName));
    }
    // In the current monolithic mode, ordinary modules are object producers with
    // no intermediate static archive. finalizeMonolithicGraph() attaches every
    // reachable module directly to the executable, matching UBT and avoiding
    // artificial archive dependency cycles.

    // DSC carries compile visibility and, when present, the final binary association.
    // Monolithic module DSCs deliberately have no PLOAT; their object producers
    // are attached to the target executable after lazy graph expansion.
    DSC<UeCppTarget> &dsc = targets<DSC<UeCppTarget>>.emplace_back(&cppTarget, output, defines, apiMacro);
    dsc.configuration = this;
    initializeApiMacro(dsc, defines);
    if (stdCppTarget && evaluate(AssignStandardCppTarget::YES))
    {
        // Apply centralized compiler/toolchain requirements to every UE module.
        // This is analogous to UBT's shared target compile environment.
        dsc.privateCompileDeps(*stdCppTarget);
    }
    return dsc;
}

void UeConfiguration::markTargetForBinary(const string_view logicalName)
{
    const auto target = configuredTargets.find(string(logicalName));
    if (target == configuredTargets.end() || target->second.dsc == nullptr)
    {
        printErrorMessage(
            FORMAT("Cannot mark an unconfigured UE module for the target binary.\nModule: {}", logicalName));
    }
    if (target->second.kind == UeFileKind::Module)
    {
        target->second.buildModule = true;
    }
}

void UeConfiguration::finalizeMonolithicGraph()
{
    if (targetType != TargetType::LIBRARY_STATIC)
    {
        return;
    }

    // Explicit addPublic/PrivateCycleDependency() calls omit scheduler edges but
    // retain reqDeps/useReqDeps. Compute their semantic transitive closure to a
    // fixed point before CppTarget::completeRoundOne() consumes those properties.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        // Module creation and binary assignment are separate in UBT. Header-only
        // dependencies still receive their include paths, but only binary modules
        // discover/compile source and generated implementation files.
        for (auto &entry : configuredTargets)
        {
            ConfiguredTarget &configured = entry.second;
            if (configured.kind == UeFileKind::Module && configured.dsc != nullptr)
            {
                configured.dsc->getSourceTarget().prepareModuleInputs(configured.buildModule);
            }
        }

        bool changed;
        do
        {
            changed = false;
            for (auto &entry : configuredTargets)
            {
                ConfiguredTarget &configured = entry.second;
                if (configured.dsc == nullptr)
                {
                    continue;
                }
                CppTarget &cppTarget = configured.dsc->getSourceTarget();
                const size_t reqSize = cppTarget.reqDeps.size();
                const size_t useReqSize = cppTarget.useReqDeps.size();
                cppTarget.populateReqAndUseReqDeps();
                changed |= reqSize != cppTarget.reqDeps.size() || useReqSize != cppTarget.useReqDeps.size();
            }
        } while (changed);
    }

    for (const string &rootName : requestedTargets)
    {
        const auto root = configuredTargets.find(rootName);
        if (root == configuredTargets.end() || root->second.kind != UeFileKind::Target || root->second.dsc == nullptr ||
            root->second.dsc->ploat == nullptr)
        {
            printErrorMessage(FORMAT("A monolithic UE graph root must be a configured target with an executable.\n"
                                     "Configuration: {}\nRoot: {}",
                                     name, rootName));
        }

        PLOAT &executable = root->second.dsc->getPLOAT();
        for (auto &entry : configuredTargets)
        {
            ConfiguredTarget &configured = entry.second;
            if (configured.kind == UeFileKind::Module && configured.buildModule && configured.dsc != nullptr)
            {
                executable.objectFileProducers.emplace(configured.dsc->objectFileProducer);
            }
        }
        for (const auto &entry : prebuiltLibraries)
        {
            executable.publicDeps(*entry.second);
        }
    }
}

DSC<UeCppTarget> &UeConfiguration::getOrAddTarget(const string_view logicalName)
{
    // Lazy dependency expansion, comparable to
    // UEBuildTarget.FindOrCreateModuleByName(). No module is configured merely
    // because scanner.py registered it; it must be requested or reached by a dep.
    const string targetName(logicalName);
    auto [iterator, inserted] = configuredTargets.try_emplace(targetName);
    ConfiguredTarget &configuredTarget = iterator->second;
    UeTargetState &state = configuredTarget.state;
    UeFileKind &kind = configuredTarget.kind;
    DSC<UeCppTarget> *&dsc = configuredTarget.dsc;
    if (!inserted && (state == UeTargetState::Configured || state == UeTargetState::Configuring))
    {
        // Configuring is returned as well as Configured so circular module references
        // reuse the object already in progress instead of recursively creating it.
        return *dsc;
    }

    // Similar failure point to RulesAssembly.CreateModuleRules() not finding a
    // ModuleRules definition for a dependency name.
    const auto functionSetIterator = specifyFunctionSets.find(targetName);
    if (functionSetIterator == specifyFunctionSets.end())
    {
        printErrorMessage(FORMAT("Could not find registered UE specify functions.\nTarget: {}", logicalName));
    }
    const UeSpecifyFunctionSet &functions = functionSetIterator->second;
    if (!functions.base)
    {
        printErrorMessage(
            FORMAT("UE target has specialized specify functions but no base function.\nTarget: {}", logicalName));
    }

    kind = functions.kind;
    dsc = &makeDscUeCppTarget(targetName, functions.kind);
    state = UeTargetState::Configuring;
    DSC<UeCppTarget> *const target = dsc;

    // Dependencies declared by this function may recursively call getOrAddTarget().
    // The stack makes currentTarget() restore the parent correctly afterward.
    currentTargetStack.emplace_back(target);

    // A specialized UBT rules class derives from the base class, so apply the base
    // first. Then follow RulesAssembly precedence: exact platform wins; otherwise
    // at most one matching platform-group specialization is allowed.
    const UeSpecifyFunctionBase *selectedSpecialization = nullptr;
    const auto invokeSpecifyFunc = [this](const UeSpecifyFunctionBase &specifyFunction) {
        const UePathNormalizationScope pathScope(specifyFunction.file);
        specifyFunction.func(*this);
    };
    invokeSpecifyFunc(*functions.base);

    const auto platformFunction = std::ranges::find_if(
        functions.platforms, [this](const UePlatformSpecifyFunc &candidate) { return candidate.platform == platform; });
    if (platformFunction != functions.platforms.end())
    {
        selectedSpecialization = &*platformFunction;
        invokeSpecifyFunc(*platformFunction);
    }
    else
    {
        const UePlatformGroupSpecifyFunc *selectedGroupFunction = nullptr;
        for (const UePlatformGroup group : platformGroups)
        {
            const auto groupFunction =
                std::ranges::find_if(functions.platformGroups, [group](const UePlatformGroupSpecifyFunc &candidate) {
                    return candidate.platformGroup == group;
                });
            if (groupFunction == functions.platformGroups.end())
            {
                continue;
            }
            if (selectedGroupFunction != nullptr)
            {
                printErrorMessage(
                    FORMAT("Found multiple matching UE platform-group specify functions without an exact platform "
                           "override.\nTarget: {}\nFirst: {}\nSecond: {}",
                           logicalName, selectedGroupFunction->file->filePath, groupFunction->file->filePath));
            }
            selectedGroupFunction = &*groupFunction;
        }
        if (selectedGroupFunction != nullptr)
        {
            selectedSpecialization = selectedGroupFunction;
            invokeSpecifyFunc(*selectedGroupFunction);
        }
    }

    currentTargetStack.pop_back();

    if (functions.kind == UeFileKind::Module)
    {
        // UBT obtains these from ModuleRules.GetAllModuleDirectories() and performs
        // source/default-include discovery in UEBuildModuleCPP, not in Build.cs.
        // Base and selected platform extension directories both contribute.
        UeCppTarget &cppTarget = target->getSourceTarget();
        cppTarget.conditionalAddModuleDirectory(
            Node::getNodeNonNormalized(string(functions.base->file->getDirectoryStringView()), false));
        if (selectedSpecialization != nullptr)
        {
            cppTarget.conditionalAddModuleDirectory(
                Node::getNodeNonNormalized(string(selectedSpecialization->file->getDirectoryStringView()), false));
        }
    }

    // A nested dependency may have rehashed configuredTargets, invalidating
    // iterator/record. Reacquire the entry before marking this target complete.
    configuredTargets.find(targetName)->second.state = UeTargetState::Configured;
    return *target;
}

void UeConfiguration::configureRequestedTargets()
{
    // This is the graph-entry stage. It corresponds to UBT starting from requested
    // TargetDescriptors and discovering only modules reachable from those targets.
    for (const string &target : requestedTargets)
    {
        getOrAddTarget(target);
    }
    finalizeMonolithicGraph();
}

UeConfiguration &getUeConfiguration(const string &name)
{
    // Register this UE-aware configuration with HMake's ordinary configuration
    // collection so later build-system phases process it normally.
    UeConfiguration &configuration = targets<UeConfiguration>.emplace_back(name);
    allConfigurations.emplace_back(&configuration);
    return configuration;
}
