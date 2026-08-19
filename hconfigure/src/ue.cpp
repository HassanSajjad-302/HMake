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

int compareAsciiCaseInsensitive(const string_view left, const string_view right)
{
    const size_t commonSize = std::min(left.size(), right.size());
    for (size_t i = 0; i < commonSize; ++i)
    {
        unsigned char leftChar = left[i];
        unsigned char rightChar = right[i];
        if (leftChar >= 'A' && leftChar <= 'Z')
        {
            leftChar += 'a' - 'A';
        }
        if (rightChar >= 'A' && rightChar <= 'Z')
        {
            rightChar += 'a' - 'A';
        }
        if (leftChar != rightChar)
        {
            return leftChar < rightChar ? -1 : 1;
        }
    }
    return left.size() == right.size() ? 0 : (left.size() < right.size() ? -1 : 1);
}

bool ueSourceLess(const Node *left, const Node *right, const uint64_t jumboFileSize)
{
    // UBT places oversized sources last, then compares normalized absolute paths without case sensitivity.
    const bool leftOversized = left->fileSize > jumboFileSize;
    const bool rightOversized = right->fileSize > jumboFileSize;
    if (leftOversized != rightOversized)
    {
        return !leftOversized;
    }
    if (const int comparison = compareAsciiCaseInsensitive(left->filePath, right->filePath); comparison != 0)
    {
        return comparison < 0;
    }
    return left->filePath < right->filePath;
}

string getNearestPluginRoot(const string &moduleDirectory)
{
    // Cache the nearest plugin root rather than only whether one directory owns a descriptor. Modules in the same
    // plugin then resolve after one hash lookup, and path compression also makes shared Engine ancestors cheap.
    static flat_hash_map<string, string> cache;
    vector<string> uncachedDirectories;
    string directory = moduleDirectory;
    string pluginRoot;
    while (!directory.empty())
    {
        if (const auto cached = cache.find(directory); cached != cache.end())
        {
            pluginRoot = cached->second;
            break;
        }
        uncachedDirectories.emplace_back(directory);

        std::error_code error;
        std::filesystem::directory_iterator iterator(directory, error);
        const std::filesystem::directory_iterator end;
        while (!error && iterator != end)
        {
            if (iterator->path().extension() == ".uplugin")
            {
                pluginRoot = directory;
                break;
            }
            iterator.increment(error);
        }
        if (!pluginRoot.empty())
        {
            break;
        }

        const string parent = path(directory).parent_path().string();
        if (parent.empty() || parent == directory)
        {
            break;
        }
        directory = parent;
    }

    for (string &uncached : uncachedDirectories)
    {
        cache.emplace(std::move(uncached), pluginRoot);
    }
    return pluginRoot;
}

path getModuleGeneratedIncludeRoot(const path &configuredRoot, const Node *moduleDirectory)
{
    // UBT stores a plugin module's generated headers under the plugin's own Intermediate directory and every other
    // module's under Engine/Intermediate. The enclosing *.uplugin file identifies a plugin; a directory named "Source"
    // does not, because those nest freely inside Engine/Source (Runtime/CUDA/Source, Experimental/FieldSystem/Source,
    // and much of ThirdParty). setGeneratedIncludeRoot() supplies the engine-anchored path, so only a plugin module
    // needs its target-specific suffix re-anchored.
    const string pluginRoot = getNearestPluginRoot(moduleDirectory->filePath);
    if (pluginRoot.empty())
    {
        return configuredRoot;
    }

    // All modules in one configuration share this suffix. Find it once by segment boundary instead of rebuilding it
    // component-by-component for every module.
    static flat_hash_map<string, string> suffixCache;
    const string configuredRootString = configuredRoot.string();
    auto [suffix, inserted] = suffixCache.try_emplace(configuredRootString);
    if (inserted)
    {
        const string_view intermediateName = os == OS::NT ? "intermediate" : "Intermediate";
        for (size_t intermediate = configuredRootString.find(intermediateName); intermediate != string::npos;
             intermediate = configuredRootString.find(intermediateName, intermediate + 1))
        {
            const size_t afterIntermediate = intermediate + intermediateName.size();
            const bool startsAtBoundary = intermediate == 0 || configuredRootString[intermediate - 1] == slashc;
            const bool endsAtBoundary = afterIntermediate == configuredRootString.size() ||
                                        configuredRootString[afterIntermediate] == slashc;
            if (startsAtBoundary && endsAtBoundary)
            {
                suffix->second = configuredRootString.substr(intermediate);
                break;
            }
        }
    }
    return suffix->second.empty() ? configuredRoot : path(pluginRoot) / suffix->second;
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

void addSpecifyFunc(string logicalName, const UeFileKind kind, const UeConfProfile ueConfProfile,
                    const std::optional<UePlatformGroup> platformGroup, const std::optional<UePlatform> platform,
                    const UeSpecifyFunction func, Node *file)
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
        set.ueConfProfile = ueConfProfile;
    }
    else if (set.kind != kind)
    {
        printErrorMessage(
            FORMAT("UE specification '{}' was registered as both a module and a target.", set.logicalName));
    }
    else if (set.ueConfProfile != ueConfProfile)
    {
        printErrorMessage(FORMAT("UE specification files disagree about their configuration profile.\n"
                                 "Target: {}\nFile: {}",
                                 set.logicalName, file->filePath));
    }
    if (!platformGroup && !platform)
    {
        // Exactly one unselected specification is required, matching UBT's required
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
    specifyFunctionSets.reserve(files.size());
    for (const UeIncludedFile &includedFile : files)
    {
        if (includedFile.path.empty() || includedFile.logicalName.empty() || includedFile.func == nullptr)
        {
            printErrorMessage("A generated UE file registration requires a path, logical name, and specify function.");
        }

        Node *file = Node::getNode(includedFile.path, true);
        addSpecifyFunc(string(includedFile.logicalName), includedFile.kind, includedFile.ueConfProfile,
                       includedFile.platformGroup, includedFile.platform, includedFile.func, file);
    }
}

UeCppTarget::UeCppTarget(const string &hmakeName, string logicalName_, UeConfiguration *configuration)
    : CppTarget(hmakeName, configuration), logicalName(std::move(logicalName_))
{
    isUeCppTarget = true;
    intermediateName = logicalName;
    if constexpr (bsMode == BSMode::BUILD)
    {
        const auto dependency = nameToIndexMap.find(IspcTarget::getCacheName(this));
        const bool hasCachedIspcTarget =
            dependency != nameToIndexMap.end() &&
            std::ranges::any_of(cachedReqObjectFileProducers, [&](const uint32_t packed) {
                return OpDepInfo::getCacheIndex(packed) == dependency->second;
            });
        if (hasCachedIspcTarget)
        {
            ispcTarget = new IspcTarget(this);
        }
    }
}

UeCppTarget &UeCppTarget::setShortName(const string_view value)
{
    intermediateName = value;
    return *this;
}

bool UeCppTarget::conditionalAddModuleDirectory(const NodeOrStr &directory)
{
    const string directoryPath =
        directory.hasNode_ ? directory.node_->filePath : getNormalizedPath(path(directory.str_));
    if (!std::filesystem::is_directory(directoryPath))
    {
        return false;
    }

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *directoryNode = Node::getNode(directoryPath, false);
        if (std::ranges::find(moduleDirectories, directoryNode) == moduleDirectories.end())
        {
            moduleDirectories.emplace_back(directoryNode);
        }
    }
    return true;
}

// TODO(UE cycles): Delete all four cycle-dependency implementations when UE module cycles are removed.
UeCppTarget &UeCppTarget::addCycleDependency(const DepType depType, const bool link, const string_view dependency)
{
    auto &ueConfiguration = *static_cast<UeConfiguration *>(configuration);

    // Same reachability rule as DSCExtension::addNamedDependency.
    DSC<UeCppTarget> &dependencyTarget =
        ueConfiguration.getOrAddTarget(dependency, link && implementationRequested);
    if (link)
    {
        linkDependencies.emplace_back(&dependencyTarget.getSourceTarget());
    }

    // Retain the ordinary producer/PLOAT semantics, but omit scheduler edges that would close the UE module
    // cycle. An include-path relation carries no linker input, matching the *OpDeps functions.
    ueConfiguration.currentTarget().deps<false>(depType, true, link, dependencyTarget);
    return *this;
}

UeCppTarget &UeCppTarget::addPrivateCycleDependency(const string_view dependency)
{
    return addCycleDependency(DepType::PRIVATE, true, dependency);
}

UeCppTarget &UeCppTarget::addPublicCycleDependency(const string_view dependency)
{
    return addCycleDependency(DepType::PUBLIC, true, dependency);
}

UeCppTarget &UeCppTarget::addPrivateCycleOpDependency(const string_view dependency)
{
    return addCycleDependency(DepType::PRIVATE, false, dependency);
}

UeCppTarget &UeCppTarget::addPublicCycleOpDependency(const string_view dependency)
{
    return addCycleDependency(DepType::PUBLIC, false, dependency);
}

void UeCppTarget::requestImplementation()
{
    if (implementationRequested)
    {
        return;
    }
    // Set before recursing. UE's module cycles can reach this target again, and this flag terminates that walk in
    // both modes. prepareModuleSources() separately decides whether this target is allowed to materialize sources.
    implementationRequested = true;
    prepareModuleSources();
    for (UeCppTarget *dependency : linkDependencies)
    {
        dependency->requestImplementation();
    }
}

void UeCppTarget::propagateSelectiveBuild()
{
    // TODO(UE cycles): Remove this manual propagation when every UE module relation is a scheduler edge.
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
    FOR_REQ_OBJECT_FILE_PRODUCERS(this, producer, dependency)
    {
        if (dependency.isOpDependency() && producer->isUeCppTarget)
        {
            auto &ueDependency = static_cast<UeCppTarget &>(*producer);
            ueDependency.selectiveBuild = true;
            ueDependency.propagateSelectiveBuild();
        }
    }
}

void UeCppTarget::completeRoundOne()
{
    // TODO(UE cycles): Remove guarded recursion and use ordinary scheduler completion after UE cycles are removed.
    // selectiveBuild belongs to round 0, but explicit UE cycle dependencies have
    // no RealBTarget edge through which Builder could propagate it. Do it before
    // the completion guard so a later selected parent can still reach this target.
    propagateSelectiveBuild();

    if (roundOneCalled)
    {
        return;
    }
    roundOneCalled = true;

    // Ordinary dependencies have already completed through scheduler ordering.
    // An explicit cycle dependency has no scheduler edge, so this guarded walk
    // completes it here. Marking roundOneCalled before recursion breaks the cycle.
    FOR_REQ_OBJECT_FILE_PRODUCERS(this, producer, dependency)
    {
        if (dependency.isOpDependency() && producer->isUeCppTarget)
        {
            static_cast<UeCppTarget *>(producer)->completeRoundOne();
        }
    }

    // Ordinary CppTargets reach this manager through its scheduler edge. The guarded UE recursion above can enter a
    // target directly, so complete adaptive partitioning locally before CppTarget consumes the selected compile units.
    if (adaptiveManager)
    {
        adaptiveManager->completeRoundOne();
    }

    CppTarget::completeRoundOne();
}

void UeCppTarget::prepareModuleIncludes()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (!moduleDirectoriesReady || moduleIncludesPrepared)
        {
            return;
        }

        for (Node *moduleDirectory : moduleDirectories)
        {
            if (bAddDefaultIncludePaths)
            {
                addDefaultIncludePaths(moduleDirectory);
            }
        }

        const auto &ueConfiguration = *static_cast<UeConfiguration *>(configuration);
        path uhtDirectory;
        path vniDirectory;
        if (ueConfiguration.generatedIncludeRoot != nullptr && !moduleDirectories.empty())
        {
            const path generatedIncludeRoot = getModuleGeneratedIncludeRoot(
                ueConfiguration.generatedIncludeRoot->filePath, moduleDirectories.front());
            const path moduleGeneratedRoot = generatedIncludeRoot / intermediateName;
            uhtDirectory = moduleGeneratedRoot / "UHT";
            vniDirectory = moduleGeneratedRoot / "VNI";
        }

        if (!uhtDirectory.empty() && std::filesystem::is_directory(uhtDirectory))
        {
            Node *directory = Node::getNode(uhtDirectory.string(), false);
            publicIncludesSource(directory);
            generatedCodeDirectories.emplace_back(directory);
        }

        // VNI headers are generated beside UHT output. UBT adds this directory to the module compile environment but
        // does not compile sources from it.
        if (!vniDirectory.empty() && std::filesystem::is_directory(vniDirectory))
        {
            publicIncludesSource(Node::getNode(vniDirectory.string(), false));
        }
        moduleIncludesPrepared = true;
    }
}

void UeCppTarget::prepareModuleSources()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        // Includes and generated-code directories must be known before source discovery, even when this function was
        // reached through a late promotion from an include-only dependency.
        prepareModuleIncludes();
        if (!moduleDirectoriesReady || !implementationRequested || sourceInputsPrepared)
        {
            return;
        }
        sourceInputsPrepared = true;
        if (addCppSource == AddCppSource::NO)
        {
            return;
        }

        // Source scanning must precede generated-code scanning: handwritten sources identify generated .cpp files
        // included inline and therefore excluded from standalone compilation. Gather every module directory before
        // scheduling so platform extensions participate in the same deterministic UBT-compatible ordering.
        vector<Node *> sourceNodes;
        vector<Node *> ispcSources;
        for (Node *moduleDirectory : moduleDirectories)
        {
            findInputFiles(moduleDirectory, sourceNodes, ispcSources);
        }
        std::ranges::sort(sourceNodes, [this](const Node *left, const Node *right) {
            return ueSourceLess(left, right, jumboFileSize);
        });
        for (Node *source : sourceNodes)
        {
            // moduleFiles() falls back to sourceFiles() for IsCppMod::NO.
            moduleFiles(source);
        }

        std::ranges::sort(ispcSources, {}, [](const Node *node) { return node->filePath; });
        for (Node *source : ispcSources)
        {
            addIspcSource(source);
        }
        for (Node *directory : generatedCodeDirectories)
        {
            addGeneratedCode(directory);
        }
    }
}

void UeCppTarget::findInputFiles(Node *moduleDirectory, vector<Node *> &sourceNodes, vector<Node *> &ispcSources)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        // UBT equivalent: UEBuildModuleCPP.FindInputFiles() and
        // FindInputFilesFromDirectoryRecursive(). A *.hmake.hpp user does
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
                if (extension == ".ispc")
                {
                    ispcSources.emplace_back(Node::getNode(*iterator));
                }
                else if (isUeSource && !fileName.ends_with(".gen.cpp"))
                {
                    Node *source = Node::getNode(*iterator);
                    if (extension == ".cpp")
                    {
                        // UBT's SourceFileMetadataCache records each
                        //   #include UE_INLINE_GENERATED_CPP_BY_NAME(Name)
                        // and UEBuildModuleCPP removes Name.gen.cpp from the list of
                        // independently compiled generated files. Keep the same split
                        // so generated code sees any prerequisite includes/declarations
                        // supplied by its owning handwritten translation unit.
                        constexpr string_view marker = "UE_INLINE_GENERATED_CPP_BY_NAME(";
                        std::ifstream sourceFile(iterator->path());
                        string line;
                        while (std::getline(sourceFile, line))
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

                    sourceNodes.emplace_back(source);
                }
            }
            ++iterator;
        }

    }
}

void UeCppTarget::addIspcSource(Node *source)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const auto &ueConfiguration = *static_cast<UeConfiguration *>(configuration);
        if (ueConfiguration.ispcCompilerFeatures.compiler == nullptr)
        {
            return;
        }
        if (!ispcOutputDirectoryAdded)
        {
            // UBT adds the module's ISPC intermediate directory to the private compile environment so handwritten C++
            // can include <SourceName>.ispc.generated.h. The dedicated IspcHeader actions populate that directory
            // before any ordinary or adaptive C++ compile action starts.
            privateIncludesSource(myBuildDir);
            ispcOutputDirectoryAdded = true;
        }

        IspcTarget *target = ispcTarget;
        if (target == nullptr)
        {
            target = new IspcTarget(this);
            ispcTarget = target;
        }
        target->addSource(source);
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

            Node *includeDirectory = Node::getNode(directoryPath.string(), false);
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

        // Classes is UBT's compatibility public UObject-header directory. Public is a
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

UeCppTarget &UeCppTarget::addGeneratedCode(Node *directory)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        vector<Node *> standaloneGeneratedSources;
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

                // UBT compiles generated implementations not claimed by an UE_INLINE_GENERATED_CPP_BY_NAME include
                // as separate inputs. This includes each module's *.init.gen.cpp.
                standaloneGeneratedSources.emplace_back(Node::getNode(entry));
            }
        }

        std::ranges::sort(standaloneGeneratedSources, {}, &Node::myId);
        if (!standaloneGeneratedSources.empty())
        {
            // UBT's unity builder keeps generated implementation files in their own unity blobs. Preserve that
            // separation while still allowing size-based partitioning within this generated-code group.
            startJumboGroup();
            for (Node *source : standaloneGeneratedSources)
            {
                moduleFiles(source);
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
    if (ispcCompilerFeatures.compiler != nullptr)
    {
        if (buildCommands)
        {
            ispcCompilerFeatures.includeDirectories.clear();
            ispcCompilerFeatures.includeDirectories.reserve(buildCommands->ispcIncludeDirectories.size());
            for (const string &directory : buildCommands->ispcIncludeDirectories)
            {
                ispcCompilerFeatures.includeDirectories.emplace_back(Node::getNode(directory, false));
            }
            ispcCompilerFeatures.compileDefinitions.clear();
            ispcCompilerFeatures.compileDefinitions.reserve(buildCommands->ispcDefinitionArguments.size() + 1);
            for (const string &argument : buildCommands->ispcDefinitionArguments)
            {
                if (!argument.starts_with("-D") || argument.size() == 2)
                {
                    printErrorMessage(FORMAT("Invalid exported UE ISPC definition.\nConfiguration: {}\nArgument: {}",
                                             name, argument));
                }
                ispcCompilerFeatures.compileDefinitions.emplace_back(argument.substr(2));
            }
        }
        std::erase_if(ispcCompilerFeatures.compileDefinitions, [](const string &definition) {
            return definition.starts_with("PLATFORM_EXCEPTIONS_DISABLED=");
        });
        ispcCompilerFeatures.compileDefinitions.emplace_back(
            evaluate(ExceptionHandling::ON) ? "PLATFORM_EXCEPTIONS_DISABLED=0"
                                            : "PLATFORM_EXCEPTIONS_DISABLED=1");
    }
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

        if (!buildCommands->cppCompileCommand.empty())
        {
            if (compilerFeatures.compiler.bTFamily != BTFamily::GCC)
            {
                printErrorMessage(FORMAT("UE semantic compile-command policy currently supports GCC-family command "
                                         "rows only.\nConfiguration: {}",
                                         name));
            }

            cppCompileCommand += evaluate(RTTI::ON) ? "-frtti " : "-fno-rtti ";
            cppCompileCommand += evaluate(ExceptionHandling::ON) ? "-fexceptions " : "-fno-exceptions ";
            const string_view exceptionDefinition = evaluate(ExceptionHandling::ON)
                                                        ? "-DPLATFORM_EXCEPTIONS_DISABLED=0 "
                                                        : "-DPLATFORM_EXCEPTIONS_DISABLED=1 ";
            cppCompileCommand += exceptionDefinition;
            if (!buildCommands->cCompileCommand.empty())
            {
                cCompileCommand += exceptionDefinition;
            }
        }
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
    assign(value);
    switch (value)
    {
    case UePlatform::Linux:
        ispcCompilerFeatures.targetOs = TargetOS::LINUX_;
        break;
    case UePlatform::Windows:
        ispcCompilerFeatures.targetOs = TargetOS::WINDOWS;
        break;
    case UePlatform::Mac:
        ispcCompilerFeatures.targetOs = TargetOS::DARWIN;
        break;
    case UePlatform::Android:
        ispcCompilerFeatures.targetOs = TargetOS::ANDROID;
        break;
    case UePlatform::IOS:
        ispcCompilerFeatures.targetOs = TargetOS::IPHONE;
        break;
    }
    if (!groups.empty())
    {
        platformGroups = std::move(groups);
    }
    return *this;
}

UeConfiguration &UeConfiguration::setArchitecture(const UeArchitecture value)
{
    assign(value);
    ispcCompilerFeatures.addressModel = AddressModel::A_64;
    if (value == UeArchitecture::x64)
    {
        ispcCompilerFeatures.arch = Arch::X86;
        ispcCompilerFeatures.targets = {"avx512skx-i32x8", "avx2", "avx", "sse4"};
    }
    else
    {
        ispcCompilerFeatures.arch = Arch::ARM;
        ispcCompilerFeatures.targets = {"neon"};
    }
    return *this;
}

UeConfiguration &UeConfiguration::setBuildConfiguration(const UeBuildConfiguration value)
{
    return assign(value);
}

UeConfiguration &UeConfiguration::setUeTargetType(const UeTargetType value)
{
    return assign(value);
}

UeConfiguration &UeConfiguration::setGeneratedIncludeRoot(Node *value)
{
    generatedIncludeRoot = value;
    return *this;
}

UeConfiguration &UeConfiguration::setIspcCompiler(Node *value)
{
    if (value == nullptr)
    {
        printErrorMessage(FORMAT("UE ISPC requires a compiler.\nConfiguration: {}", name));
    }
    ispcCompilerFeatures.compiler = value;
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

UeConfiguration &UeConfiguration::createProducerConfigurations()
{
    if (ueConfProfile != UeConfProfile::Default)
    {
        printErrorMessage(FORMAT("Only a Default UE configuration can create producer configurations.\n"
                                 "Configuration: {}",
                                 name));
    }
    if (!producerConfigurations.empty() || !configuredTargets.empty())
    {
        printErrorMessage(FORMAT("UE producer configurations must be created once, before configuring targets.\n"
                                 "Configuration: {}",
                                 name));
    }

    UeConfiguration &rttiExcept = getUeConfiguration(name + "RttiExcept");
    rttiExcept.copySettingsFrom(*this);
    rttiExcept.platform = platform;
    rttiExcept.platformGroups = platformGroups;
    rttiExcept.architecture = architecture;
    rttiExcept.buildConfiguration = buildConfiguration;
    rttiExcept.ueTargetType = ueTargetType;
    rttiExcept.generatedIncludeRoot = generatedIncludeRoot;
    rttiExcept.buildCommands = buildCommands;

    rttiExcept.ueConfProfile = UeConfProfile::RttiExcept;
    // The profile exists for these two semantics; everything else is inherited so both configurations agree on ABI.
    rttiExcept.assign(RTTI::ON, ExceptionHandling::ON);
    // A producer archives only the modules registered under its profile. Its transitive dependencies still provide
    // include paths and header-units under these semantics, but their translation units belong to the consumer.
    rttiExcept.addCppSource = AddCppSource::NO;

    // callConfigurationSpecification() deliberately does not reach a configuration created during its own loop, so
    // the creator owns the producer's lifecycle.
    rttiExcept.initialize();
    producerConfigurations.emplace(UeConfProfile::RttiExcept, &rttiExcept);
    return *this;
}

void UeConfiguration::finalizeProducerConfigurations() const
{
    for (const auto &producer : producerConfigurations)
    {
        producer.second->postConfigurationSpecification();
    }
}

UeConfiguration &UeConfiguration::getProducerConfiguration(const UeConfProfile producerUeConfProfile) const
{
    const auto producer = producerConfigurations.find(producerUeConfProfile);
    if (producer == producerConfigurations.end())
    {
        printErrorMessage(FORMAT("UE configuration has no producer for a module's configuration profile.\n"
                                 "Configuration: {}\nProfile: {}\n"
                                 "Hint: call createProducerConfigurations() before expanding targets.",
                                 name, static_cast<uint8_t>(producerUeConfProfile)));
    }
    return *producer->second;
}

PLOAT &UeConfiguration::addProducerArchive(const string &logicalName, const UeConfProfile producerUeConfProfile)
{
    UeConfiguration &producer = getProducerConfiguration(producerUeConfProfile);
    DSC<UeCppTarget> &implementation = producer.getOrAddTarget(logicalName);
    if (implementation.ploat == nullptr || implementation.ploat->linkTargetType != TargetType::LIBRARY_STATIC)
    {
        printErrorMessage(FORMAT("A UE module built by a producer configuration has no static archive.\n"
                                 "Configuration: {}\nProducer: {}\nModule: {}",
                                 name, producer.name, logicalName));
    }
    LOAT &archive = implementation.getLOAT();

    // PLOAT::completeRoundOne() resolves a LOAT's output directory from myBuildDir, so at configure time that is the
    // only member holding the archive's location. At build time the location comes from the restored output node.
    Node *archiveDirectory;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        archiveDirectory = archive.myBuildDir;
    }
    else
    {
        archiveDirectory = Node::getNode(string(archive.getOutputDirectoryV()), false);
    }

    // A prebuilt library resolved purely by path. LIBRARY_STATIC and PLIBRARY_STATIC share one filename convention, so
    // the proxy names the same file the producer archives. Nothing about the producer's link closure or object-file
    // producers crosses the configuration boundary; this configuration computes its own closure from its own graph.
    PLOAT &proxy = targets<PLOAT>.emplace_back(*this, archive.getOutputName(), archiveDirectory,
                                               TargetType::PLIBRARY_STATIC,
                                               name + slashc + logicalName + "-producer-archive", false, false);
    ploats.emplace_back(&proxy);

    // Makes dependents create their ordinary round-zero edge to this proxy, which in turn waits for the real archive.
    // PLIBRARY_STATIC initializes hasObjectFiles in PLOAT itself.
    proxy.realBTargets[0].addDep<BTargetType::UNKNOWN>(&archive.realBTargets[0]);
    return proxy;
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
    const string outputName = getTargetNameFromActualName(libraryType, os, fileName);
    if (getActualNameFromTargetName(libraryType, os, outputName) != fileName)
    {
        printErrorMessage(FORMAT("Unsupported prebuilt UE library filename.\nLibrary: {}\nType: {}\n"
                                 "Expected the conventional filename for the configured platform.",
                                 libraryFile->filePath, static_cast<uint8_t>(libraryType)));
    }

    Node *directory = Node::getNode(libraryPath.parent_path().string(), false);
    PLOAT &library = targets<PLOAT>.emplace_back(*this, outputName, directory, libraryType,
                                                 name + slashc + "prebuilt-" + std::to_string(prebuiltLibraries.size()),
                                                 false, false);
    ploats.emplace_back(&library);
    prebuiltLibraries.emplace(key, &library);
    return library;
}

DSC<UeCppTarget> &UeConfiguration::makeDscUeCppTarget(string logicalName, const UeFileKind fileKind,
                                                     const UeConfProfile moduleUeConfProfile)
{
    // At this point the scanner registry has selected a logical rules declaration,
    // but its specify() functions have not yet populated the target.
    UeCppTarget &cppTarget =
        targets<UeCppTarget>.emplace_back(name + slashc + logicalName + dashCpp, logicalName, this);
    cppTarget.isSystem = fileKind == UeFileKind::Prebuilt;
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
    switch (fileKind)
    {
    case UeFileKind::Target:
        // The top-level target owns the one monolithic executable output.
        output = &GetExeLOAT(logicalName);
        break;
    case UeFileKind::Module:
        if (moduleUeConfProfile != ueConfProfile)
        {
            // This module needs compiler semantics this configuration does not provide, so it is not archived here.
            // Either way the local target still owns the compile interface its dependents include.
            cppTarget.assign(AddCppSource::NO);
            if (producerConfigurations.contains(moduleUeConfProfile))
            {
                // A producer configuration archives it. Only the objects come from elsewhere, through an archive this
                // configuration references by path.
                output = &addProducerArchive(logicalName, moduleUeConfProfile);
            }
            // Otherwise this is that producer, reached through a dependency of the module it exists to archive. The
            // consumer compiles and links this module itself, so there is deliberately no local output.
            break;
        }
        if (ueConfProfile != UeConfProfile::Default)
        {
            // Producer configurations default to AddCppSource::NO. Only a module registered for this producer's
            // profile materializes translation units; its transitive dependencies still build header units only.
            cppTarget.assign(AddCppSource::YES);
        }
        switch (targetType)
        {
        case TargetType::LIBRARY_SHARED:
            // Modular mode remains one shared output per C++ module for now.
            output = &getSharedLOAT(logicalName);
            break;
        case TargetType::LIBRARY_STATIC:
            output = &getStaticLOAT(logicalName);
            break;
        case TargetType::LIBRARY_OBJECT:
            // Object-only modules contribute their objects directly to the eventual executable.
            break;
        default:
            printErrorMessage(FORMAT("Unsupported HMake target type for a UE module.\n"
                                     "Configuration: {}\n"
                                     "Module: {}\n"
                                     "Target type: {}",
                                     name, logicalName, static_cast<uint8_t>(targetType)));
        }
        break;
    case UeFileKind::Prebuilt:
        break;
    }
    // DSC carries compile visibility and, when present, the module's physical output. LIBRARY_OBJECT modules have no
    // PLOAT; their object-producer graph is resolved transitively by the eventual executable.
    DSC<UeCppTarget> &dsc = targets<DSC<UeCppTarget>>.emplace_back(&cppTarget, output, defines, apiMacro);
    dsc.configuration = this;
    initializeApiMacro(dsc, defines);
    if (stdCppTarget && evaluate(AssignStandardCppTarget::YES))
    {
        // Apply centralized compiler/toolchain requirements to every UE module.
        // This is analogous to UBT's shared target compile environment.
        dsc.privateOpDeps(*stdCppTarget);
    }
    return dsc;
}

DSC<UeCppTarget> &UeConfiguration::getOrAddTarget(const string_view logicalName, const bool requestImplementation)
{
    // Lazy dependency expansion, comparable to
    // UEBuildTarget.FindOrCreateModuleByName(). No module is configured merely
    // because scanner.py registered it; it must be requested or reached by a dep.
    const string targetName(logicalName);

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

    auto [iterator, inserted] = configuredTargets.try_emplace(targetName);
    ConfiguredTarget &configuredTarget = iterator->second;
    UeTargetState &state = configuredTarget.state;
    UeFileKind &kind = configuredTarget.kind;
    DSC<UeCppTarget> *&dsc = configuredTarget.dsc;
    if (!inserted && (state == UeTargetState::Configured || state == UeTargetState::Configuring))
    {
        // Configuring is returned as well as Configured so circular module references
        // reuse the object already in progress instead of recursively creating it.
        if (kind == UeFileKind::Module)
        {
            UeCppTarget &existing = dsc->getSourceTarget();
            if (requestImplementation)
            {
                existing.requestImplementation();
            }
        }
        return *dsc;
    }

    kind = functions.kind;
    dsc = &makeDscUeCppTarget(targetName, functions.kind, functions.ueConfProfile);
    state = UeTargetState::Configuring;
    DSC<UeCppTarget> *const target = dsc;
    // Record reachability before the specify functions run because their dependencies consult it. The target-local
    // AddCppSource policy independently decides whether prepareModuleSources() materializes this target's sources.
    target->getSourceTarget().implementationRequested = requestImplementation;

    // Dependencies declared by this function may recursively call getOrAddTarget().
    // The stack makes currentTarget() restore the parent correctly afterward.
    currentTargetStack.emplace_back(target);

    // A specialized UBT rules class derives from the base class, so apply the base
    // first. Then follow RulesAssembly precedence: exact platform wins; otherwise
    // at most one matching platform-group specialization is allowed.
    const UeSpecifyFunctionBase *selectedSpecialization = nullptr;
    {
        const UePathNormalizationScope pathScope(functions.base->file);
        functions.base->func(*this);
    }

    const auto platformFunction = std::ranges::find_if(
        functions.platforms, [this](const UePlatformSpecifyFunc &candidate) { return candidate.platform == platform; });
    if (platformFunction != functions.platforms.end())
    {
        selectedSpecialization = &*platformFunction;
        {
            const UePathNormalizationScope pathScope(platformFunction->file);
            platformFunction->func(*this);
        }
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
            const UePathNormalizationScope pathScope(selectedGroupFunction->file);
            selectedGroupFunction->func(*this);
        }
    }

    currentTargetStack.pop_back();

    if (functions.kind == UeFileKind::Module)
    {
        // UBT obtains these from ModuleRules.GetAllModuleDirectories() and performs
        // source/default-include discovery in UEBuildModuleCPP, not in Build.cs.
        // Base and selected platform extension directories both contribute.
        UeCppTarget &cppTarget = target->getSourceTarget();
        cppTarget.conditionalAddModuleDirectory(Node::getNode(functions.base->file->getDirectoryStringView(), false));
        if (selectedSpecialization != nullptr)
        {
            cppTarget.conditionalAddModuleDirectory(
                Node::getNode(selectedSpecialization->file->getDirectoryStringView(), false));
        }
        cppTarget.moduleDirectoriesReady = true;
    }

    // A nested dependency may have rehashed configuredTargets, invalidating
    // iterator/record. Reacquire the entry before marking this target complete.
    configuredTargets.find(targetName)->second.state = UeTargetState::Configured;
    if (functions.kind == UeFileKind::Module)
    {
        UeCppTarget &cppTarget = target->getSourceTarget();
        cppTarget.prepareModuleIncludes();
        cppTarget.prepareModuleSources();
    }
    return *target;
}

UeConfiguration &getUeConfiguration(const string &name)
{
    // Register this UE-aware configuration with HMake's ordinary configuration
    // collection so later build-system phases process it normally.
    UeConfiguration &configuration = targets<UeConfiguration>.emplace_back(name);
    allConfigurations.emplace_back(&configuration);
    return configuration;
}
