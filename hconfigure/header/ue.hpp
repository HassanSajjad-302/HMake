#ifndef HMAKE_UE_HPP
#define HMAKE_UE_HPP

#include "CppTarget.hpp"
#include "IspcTarget.hpp"
#include <concepts>
#include <optional>
#include <span>
#include <unordered_set>

/**
 * UE-oriented values exposed to decentralized *.hmake.hpp specification functions.
 *
 * These types intentionally contain only the subset needed by the first UE5/HMake
 * experiment. They are not replacements for the complete UBT types. The comments
 * below name the closest UBT type/property so that a Build.cs or Target.cs rule can
 * be translated without guessing what a value represents.
 */

/**
 * Concrete operating-system/toolchain platform.
 *
 * UBT equivalent: UnrealTargetPlatform, normally available to ModuleRules through
 * ReadOnlyTargetRules.Platform and to TargetRules through TargetInfo.Platform.
 * UBT uses Win64; HMake currently calls that value Windows because x64/arm64 is
 * represented separately by UeArchitecture.
 */
enum class UePlatform : uint8_t
{
    Linux,   // UBT: UnrealTargetPlatform.Linux
    Windows, // UBT: UnrealTargetPlatform.Win64
    Mac,     // UBT: UnrealTargetPlatform.Mac
    Android, // UBT: UnrealTargetPlatform.Android
    IOS,     // UBT: UnrealTargetPlatform.IOS
};

/**
 * A capability/vendor/family group to which a concrete platform belongs.
 *
 * UBT equivalent: UnrealPlatformGroup. UBT obtains membership with
 * UEBuildPlatform.GetPlatformGroups(Target.Platform), and rules commonly test it
 * with Target.Platform.IsInGroup(...). HMake receives the ordered membership list
 * in UeConfiguration::setPlatform().
 *
 * Groups overlap. For example, Linux may be in Unix, Desktop, and Linux. They must
 * therefore not be modeled as a single mutually-exclusive platform value.
 */
enum class UePlatformGroup : uint8_t
{
    Unix,      // UBT: UnrealPlatformGroup.Unix
    Windows,   // UBT: UnrealPlatformGroup.Windows
    Microsoft, // UBT: UnrealPlatformGroup.Microsoft
    Apple,     // UBT: UnrealPlatformGroup.Apple
    Desktop,   // UBT: UnrealPlatformGroup.Desktop
    Linux,     // UBT: UnrealPlatformGroup.Linux
    Android,   // UBT: UnrealPlatformGroup.Android
};

/**
 * CPU architecture selected independently of UePlatform.
 *
 * UBT equivalent: UnrealArch (usually held in UnrealArchitectures). The first
 * implementation supports one architecture per HMake configuration.
 */
enum class UeArchitecture : uint8_t
{
    x64,   // UBT: UnrealArch.X64 ("x64")
    arm64, // UBT: UnrealArch.Arm64 ("arm64")
};

/**
 * User-facing build configuration.
 *
 * UBT equivalent: UnrealTargetConfiguration. This is distinct from HMake's
 * ConfigType; UeConfiguration::assign() translates the UE selection into HMake's
 * lower-level compiler/linker configuration.
 */
enum class UeBuildConfiguration : uint8_t
{
    Debug,       // UBT: UnrealTargetConfiguration.Debug
    DebugGame,   // UBT: UnrealTargetConfiguration.DebugGame
    Development, // UBT: UnrealTargetConfiguration.Development
    Test,        // UBT: UnrealTargetConfiguration.Test
    Shipping,    // UBT: UnrealTargetConfiguration.Shipping
};

/**
 * Kind of top-level UE product being built.
 *
 * UBT equivalent: TargetRules.Type / UnrealBuildTool.TargetType. This is not the
 * same as HMake TargetType, which describes an executable/static/shared output.
 */
enum class UeTargetType : uint8_t
{
    Game,    // UBT: TargetType.Game
    Client,  // UBT: TargetType.Client
    Server,  // UBT: TargetType.Server
    Editor,  // UBT: TargetType.Editor
    Program, // UBT: TargetType.Program
};

/**
 * Says whether a decentralized specification describes a UE module, external
 * prebuilt dependency, or top-level target.
 *
 * Module corresponds to a *.Build.cs/ModuleRules declaration and ultimately a
 * UEBuildModuleCPP. Prebuilt corresponds to a source-less ModuleType.External
 * ModuleRules declaration. Target corresponds to a *.Target.cs/TargetRules
 * declaration and ultimately a UEBuildTarget.
 */
enum class UeFileKind : uint8_t
{
    Module,
    Prebuilt,
    Target,
};

/**
 * Compiler-semantics profile selected by scanner metadata.
 *
 * Default uses the ordinary UE configuration. RttiExcept names the few modules that require RTTI and exceptions;
 * those are archived by a separate producer configuration and linked into the Default build.
 */
enum class UeConfProfile : uint8_t
{
    Default,
    RttiExcept,
};

/**
 * State used while lazily expanding the module dependency graph.
 *
 * This serves the same broad purpose as UBT's "find or create module" cache:
 * one UeCppTarget is created per logical module, and Configuring allows circular
 * references to return the in-progress target instead of configuring it twice.
 */
enum class UeTargetState : uint8_t
{
    Unconfigured,
    Configuring,
    Configured,
};

class UeConfiguration;

/**
 * HMake build node for the C++ sources belonging to one logical UE module/target.
 *
 * Closest UBT equivalent for a module: UEBuildModuleCPP. ModuleRules is only the
 * user's declaration; UEBuildModuleCPP is the evaluated object that owns sources,
 * include paths, definitions, and compile behavior. UeCppTarget similarly stores
 * the HMake-side evaluated C++ data after a specify() function has run.
 *
 * A Target specification uses a source-less UeCppTarget as the HMake graph anchor
 * for its executable PLOAT. As in UBT, the Launch module owns the ordinary C++
 * entry-point sources; TargetRules does not enumerate them.
 */
class UeCppTarget : public CppTarget
{
  public:
    // UE module/target name, such as "Core" or "UnrealServer".
    string logicalName;

    // UBT: ModuleRules.bAddDefaultIncludePaths. When true, the backend adds the
    // conventional Classes/Public/Internal/Private include directories for every
    // selected module directory. Source discovery is automatic regardless of this
    // setting, matching UEBuildModuleCPP.FindInputFiles().
    bool bAddDefaultIncludePaths = true;

    UeCppTarget(const string &hmakeName, string logicalName_, UeConfiguration *configuration);

    template <DepType dependency = DepType::PRIVATE, typename T, typename... Property>
    UeCppTarget &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;

    void completeRoundOne() override;

    // UBT: ModuleRules.ConditionalAddModuleDirectory(). Ordinary module roots are
    // automatic; this is only for an exceptional additional source directory.
    // Returns false without registering anything when the directory does not exist.
    bool conditionalAddModuleDirectory(const NodeOrStr &directory);

    // UBT: ModuleRules.ShortName. UBT uses this value for intermediate paths,
    // including the existing Inc/<ShortName>/UHT directory consumed here.
    UeCppTarget &setShortName(string_view value);

    // TODO(UE cycles): Remove these APIs after UE's circular module graph is eliminated.
    // A cyclic relation preserves semantic requirements without adding its scheduler back-edge.
    // These preserve compile requirements without adding scheduler edges. New code
    // should use ordinary DSC dependencies; the long-term goal is to remove the UE
    // cycles and these four functions with them.
    //
    // UBT: a cyclic PublicDependencyModuleNames/PrivateDependencyModuleNames entry. The dependency contributes a
    // linker input, so reaching it this way also requests its implementation.
    UeCppTarget &addPrivateCycleDependency(string_view dependency);
    UeCppTarget &addPublicCycleDependency(string_view dependency);

    // UBT: a cyclic PublicIncludePathModuleNames/PrivateIncludePathModuleNames entry. These grant include-path
    // visibility only, so the dependency's sources stay unrequested exactly as with the *OpDeps functions.
    UeCppTarget &addPrivateCycleOpDependency(string_view dependency);
    UeCppTarget &addPublicCycleOpDependency(string_view dependency);

  private:
    // UBT: ModuleRules.GetAllModuleDirectories(). HMake derives these directories
    // from the selected base/platform *.hmake.hpp files instead of asking
    // the user to register the conventional source root.
    vector<Node *> moduleDirectories;

    // Defaults to logicalName and is overridden through ModuleRules.ShortName.
    string intermediateName;

    // UBT: SourceFileMetadataCache.GetListOfInlinedGeneratedCppFiles(). Names in
    // this set are included by handwritten .cpp files and must not be compiled
    // again as independent *.gen.cpp translation units.
    std::unordered_set<string> inlinedGeneratedCppNames;

    // Include-only module relations still prepare the module's public interface, but source discovery is deferred
    // until a link relation reaches the implementation. This reachability is independent of AddCppSource: a
    // source-less target must still propagate implementation reachability to the modules it links against.
    vector<Node *> generatedCodeDirectories;
    bool moduleDirectoriesReady = false;
    bool moduleIncludesPrepared = false;
    bool implementationRequested = false;
    bool sourceInputsPrepared = false;

    // Link dependencies declared by this module, in declaration order. A module reached through include-only
    // relations declares its own dependencies without requesting their implementations; if a later link relation
    // promotes this module, the same promotion has to travel down this list.
    vector<UeCppTarget *> linkDependencies;

    // UE source discovery creates one ISPC object producer per module when .ispc inputs are present. This stays in
    // the UE frontend; the generic CppTarget has no ISPC-specific state.
    IspcTarget *ispcTarget = nullptr;
    bool ispcOutputDirectoryAdded = false;

    // TODO(UE cycles): Remove this guard with the manual selective-build propagation workaround.
    bool selectiveBuildSet = false;

    void propagateSelectiveBuild();
    void prepareModuleIncludes();
    void prepareModuleSources();
    UeCppTarget &addCycleDependency(DepType depType, bool link, string_view dependency);

    // Marks this module's link closure as required. implementationRequested doubles as the recursion guard, so UE's
    // circular module relations terminate even when this target has AddCppSource::NO.
    void requestImplementation();
    void findInputFiles(Node *moduleDirectory, vector<Node *> &sourceNodes, vector<Node *> &ispcSources);
    void addIspcSource(Node *source);
    void addDefaultIncludePaths(Node *moduleDirectory);

    // Registers only standalone *.gen.cpp files. The directory has already been added to the module's public include
    // interface. Files named by UE_INLINE_GENERATED_CPP_BY_NAME remain in their handwritten translation unit.
    UeCppTarget &addGeneratedCode(Node *directory);

    friend class UeConfiguration;
    template <typename, typename> friend struct DSCExtension;
};

/**
 * Adds UE name-based dependency functions to DSC<UeCppTarget>.
 *
 * The closest UBT ModuleRules mappings are:
 *   publicDeps         -> PublicDependencyModuleNames
 *   privateDeps        -> PrivateDependencyModuleNames
 *   publicOpDeps       -> PublicIncludePathModuleNames (compile/object-producer visibility only)
 *   privateOpDeps      -> PrivateIncludePathModuleNames (compile/object-producer visibility only)
 *
 * HMake's interface and link-only forms have no single exact ModuleRules property;
 * they expose HMake's more precise dependency model for later specifications.
 * Resolving a name calls UeConfiguration::getOrAddTarget(), equivalent in spirit
 * to UEBuildTarget.FindOrCreateModuleByName() recursively discovering a module.
 */
template <typename Derived> struct DSCExtension<UeCppTarget, Derived>
{
    // Owning graph/configuration. DSC stores this pointer so "Core" can be resolved
    // lazily without a global module registry.
    UeConfiguration *configuration = nullptr;

    Derived &publicDeps(string_view dependency);
    Derived &privateDeps(string_view dependency);
    Derived &interfaceDeps(string_view dependency);
    Derived &publicOpDeps(string_view dependency);
    Derived &privateOpDeps(string_view dependency);
    Derived &interfaceOpDeps(string_view dependency);
    Derived &publicLinkDeps(string_view dependency);
    Derived &privateLinkDeps(string_view dependency);
    Derived &interfaceLinkDeps(string_view dependency);

    // UBT ModuleRules mapping for external/prebuilt modules:
    // PublicAdditionalLibraries -> public/private/interface prebuilt library edges.
    // Include paths and definitions remain ordinary UeCppTarget properties.
    Derived &publicPrebuiltStaticLibrary(NodeOrStr library);
    Derived &privatePrebuiltStaticLibrary(NodeOrStr library);
    Derived &interfacePrebuiltStaticLibrary(NodeOrStr library);
    Derived &publicPrebuiltSharedLibrary(NodeOrStr library);
    Derived &privatePrebuiltSharedLibrary(NodeOrStr library);
    Derived &interfacePrebuiltSharedLibrary(NodeOrStr library);

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &publicDeps(string_view dependency, Names &&...dependencies)
    {
        publicDeps(dependency);
        (publicDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &privateDeps(string_view dependency, Names &&...dependencies)
    {
        privateDeps(dependency);
        (privateDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &interfaceDeps(string_view dependency, Names &&...dependencies)
    {
        interfaceDeps(dependency);
        (interfaceDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &publicOpDeps(string_view dependency, Names &&...dependencies)
    {
        publicOpDeps(dependency);
        (publicOpDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &privateOpDeps(string_view dependency, Names &&...dependencies)
    {
        privateOpDeps(dependency);
        (privateOpDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &interfaceOpDeps(string_view dependency, Names &&...dependencies)
    {
        interfaceOpDeps(dependency);
        (interfaceOpDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &publicLinkDeps(string_view dependency, Names &&...dependencies)
    {
        publicLinkDeps(dependency);
        (publicLinkDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &privateLinkDeps(string_view dependency, Names &&...dependencies)
    {
        privateLinkDeps(dependency);
        (privateLinkDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &interfaceLinkDeps(string_view dependency, Names &&...dependencies)
    {
        interfaceLinkDeps(dependency);
        (interfaceLinkDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

  private:
    Derived &derived()
    {
        return static_cast<Derived &>(*this);
    }

    Derived &addNamedDependency(DepType depType, bool objectProducer, bool link, string_view dependency);
    Derived &addPrebuiltLibrary(DepType depType, TargetType libraryType, NodeOrStr library);
};

using UeSpecifyFunction = void (*)(UeConfiguration &);

/**
 * Typed information scanner.py emits for one included *.hmake.hpp file.
 *
 * C++ cannot recover a function symbol from an #include path at runtime, so the
 * generated translation unit pairs the path with the included file's specify()
 * function and the metadata parsed from its leading comment block.
 */
struct UeIncludedFile
{
    string_view path;
    string_view logicalName;
    UeFileKind kind = UeFileKind::Module;
    UeConfProfile ueConfProfile = UeConfProfile::Default;
    std::optional<UePlatformGroup> platformGroup;
    std::optional<UePlatform> platform;
    UeSpecifyFunction func = nullptr;
};

/**
 * Common data for one parsed specify function.
 *
 * `func` is the decentralized specify(UeConfiguration&) function. `file` plays the
 * role of ModuleRules.File or the corresponding TargetRules file in UBT.
 */
struct UeSpecifyFunctionBase
{
    Node *file = nullptr;
    UeSpecifyFunction func = nullptr;
};

/** A specify function selected by UnrealPlatformGroup membership. */
struct UePlatformGroupSpecifyFunc : UeSpecifyFunctionBase
{
    UePlatformGroup platformGroup = UePlatformGroup::Unix;
};

/** A specify function selected by one exact UnrealTargetPlatform equivalent. */
struct UePlatformSpecifyFunc : UeSpecifyFunctionBase
{
    UePlatform platform = UePlatform::Linux;
};

/**
 * All discovered implementations for one logical name, shared globally.
 *
 * UBT RulesAssembly stores mappings from rule type names to compiled C# Types.
 * This is the HMake equivalent registry, storing C++ function pointers instead.
 * Vectors are used because a platform can belong to several overlapping groups.
 */
struct UeSpecifyFunctionSet
{
    string logicalName;
    UeFileKind kind = UeFileKind::Module;
    UeConfProfile ueConfProfile = UeConfProfile::Default;
    std::optional<UeSpecifyFunctionBase> base;
    vector<UePlatformGroupSpecifyFunc> platformGroups;
    vector<UePlatformSpecifyFunc> platforms;
};

/**
 * Bootstrap command templates for one UE configuration.
 *
 * A generated external table can map platform/configuration/architecture tuples to
 * these values. Empty members retain HMake's ordinary command for that language or
 * build stage. CppSrc/CppMod and LOAT append sources, outputs and dependencies.
 */
struct UeBuildCommands
{
    string cppCompileCommand;
    string cCompileCommand;
    string linkCommand;
    string linkDependenciesPrefix;
    string linkCommandSuffix;
    string archiveCommand;
    /// Target-wide UBT CppCompileEnvironment inputs that also apply to ISPC. Module-local inputs remain structured
    /// CppTarget usage requirements and are appended by IspcTarget after dependency propagation.
    vector<string> ispcIncludeDirectories;
    /// Generated UBT `-D...` arguments; UeConfiguration validates and strips the prefix into structured definitions.
    vector<string> ispcDefinitionArguments;
};

/** One selectable row in a command table shared by several UE configurations. */
struct UeBuildCommandEntry
{
    UePlatform platform = UePlatform::Linux;
    UeArchitecture architecture = UeArchitecture::x64;
    UeBuildConfiguration buildConfiguration = UeBuildConfiguration::Development;
    UeTargetType targetType = UeTargetType::Program;
    UeBuildCommands commands;
};

/**
 * UE-aware build context that consumes the shared specification registry.
 *
 * Values visible to a *.hmake.hpp function correspond mostly to values exposed by
 * UBT's ReadOnlyTargetRules. The shared registry corresponds broadly to
 * RulesAssembly, while each UeConfiguration owns only its UEBuildTarget-like
 * evaluated graph. HMake deliberately does not copy UBT's class hierarchy.
 */
class UeConfiguration : public Configuration
{
  public:
    // UBT: ReadOnlyTargetRules.Platform / TargetRules.Platform.
    UePlatform platform = UePlatform::Linux;

    // UBT: UEBuildPlatform.GetPlatformGroups(Platform). The list is searched only
    // when no exact-platform specification exists; more than one matching group is
    // rejected, matching RulesAssembly specialization selection.
    vector<UePlatformGroup> platformGroups{UePlatformGroup::Unix, UePlatformGroup::Desktop, UePlatformGroup::Linux};

    // UBT: one selected UnrealArch from TargetRules.Architectures.
    UeArchitecture architecture = UeArchitecture::x64;

    // UBT: ReadOnlyTargetRules.Configuration.
    UeBuildConfiguration buildConfiguration = UeBuildConfiguration::Development;

    // UBT: ReadOnlyTargetRules.Type.
    UeTargetType ueTargetType = UeTargetType::Program;

    // Root above <ModuleName>/UHT. This consumes already-generated UHT headers; it
    // does not run UHT. UBT normally records these paths while setting up UHT.
    Node *generatedIncludeRoot = nullptr;

    // Compiler-semantics profile this configuration provides. A module registered under a different profile is built
    // by the matching producer configuration and consumed here as a static archive.
    UeConfProfile ueConfProfile = UeConfProfile::Default;

    explicit UeConfiguration(const string &name);

    // Adds a graph root, analogous to the target descriptors passed to UBT.
    UeConfiguration &requestTarget(string logicalName);
    UeConfiguration &setPlatform(UePlatform value, vector<UePlatformGroup> groups = {});
    UeConfiguration &setArchitecture(UeArchitecture value);
    UeConfiguration &setBuildConfiguration(UeBuildConfiguration value);
    UeConfiguration &setUeTargetType(UeTargetType value);
    UeConfiguration &setGeneratedIncludeRoot(Node *value);
    /// Enables UE ISPC discovery with the given host compiler. The remaining policy belongs to Configuration.
    UeConfiguration &setIspcCompiler(Node *value);
    UeConfiguration &setBuildCommands(UeBuildCommands value);
    UeConfiguration &setBuildCommands(std::span<const UeBuildCommandEntry> entries);

    // Creates the RttiExcept companion configuration before graph expansion. It differs from its consumer only in the
    // compiler semantics its profile names and lazily archives matching modules as the consumer reaches them. The
    // producer receives no configurationSpecification() call.
    UeConfiguration &createProducerConfigurations();
    // Finalizes dynamically created producers after target expansion. The framework finalizes this consumer itself.
    void finalizeProducerConfigurations() const;
    void initialize() override;

    /// Assigns UE-specific or ordinary HMake properties from left to right.
    template <typename T, typename... Property> UeConfiguration &assign(T property, Property... properties);

    /// Supports common Build.cs conditions and delegates ordinary HMake properties to Configuration.
    template <typename T> bool evaluate(T property) const;

    // Returns the target whose specify() function is currently running. A
    // decentralized module file uses this instead of receiving its target directly.
    DSC<UeCppTarget> &currentTarget() const;

    // Lazily creates/configures a named module. requestImplementation is false for include-path-only relations; an
    // already configured module can later be promoted when a link relation reaches it.
    DSC<UeCppTarget> &getOrAddTarget(string_view logicalName, bool requestImplementation = true);

    // Top-level graph roots supplied with requestTarget(). The generated project entry point expands these before
    // handing control to the ordinary configuration lifecycle.
    vector<string> requestedTargets;

  private:
    // Cache record for one evaluated module. UBT similarly keeps a name-to-module
    // map so repeated dependencies reuse the same UEBuildModule instance.
    struct ConfiguredTarget
    {
        UeTargetState state = UeTargetState::Unconfigured;
        UeFileKind kind = UeFileKind::Module;
        DSC<UeCppTarget> *dsc = nullptr;
    };

    // Evaluated graph objects: closest UBT analogue is UEBuildTarget's module cache.
    flat_hash_map<string, ConfiguredTarget> configuredTargets;

    // Establishes currentTarget() while nested dependency specifications execute.
    vector<DSC<UeCppTarget> *> currentTargetStack;

  public:
    // Optional commands selected from a generated UBT bootstrap table.
    std::optional<UeBuildCommands> buildCommands;

  private:
    // One PLOAT per physical prebuilt library, shared by all modules in this
    // UeConfiguration.
    flat_hash_map<string, PLOAT *> prebuiltLibraries;

    // Configurations created by createProducerConfigurations(), keyed by the profile each one provides. Empty in a
    // producer configuration, because profiles do not nest.
    flat_hash_map<UeConfProfile, UeConfiguration *> producerConfigurations;

    DSC<UeCppTarget> &makeDscUeCppTarget(string logicalName, UeFileKind fileKind,
                                         UeConfProfile moduleUeConfProfile);
    PLOAT &getOrAddPrebuiltLibrary(Node *libraryFile, TargetType libraryType);
    void initializeApiMacro(DSC<UeCppTarget> &target, bool defines) const;

    // Resolves the configuration that archives modules registered under the given profile. Errors when this
    // configuration has no producer for it.
    UeConfiguration &getProducerConfiguration(UeConfProfile producerUeConfProfile) const;

    // Creates the consumer-side stand-in for a module archived by a producer configuration. The returned PLOAT
    // resolves to the producer's archive file and carries only a scheduler edge to it.
    PLOAT &addProducerArchive(const string &logicalName, UeConfProfile producerUeConfProfile);

    friend class UeCppTarget;
    template <typename, typename> friend struct DSCExtension;
};

template <DepType dependency, typename T, typename... Property>
UeCppTarget &UeCppTarget::assign(T property, Property... properties)
{
    CppTarget::assign<dependency>(property);
    if constexpr (sizeof...(properties))
    {
        return assign<dependency>(properties...);
    }
    return *this;
}

template <typename T> bool UeCppTarget::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), JumboBuild>)
    {
        return jumboBuild == property;
    }
    else
    {
        return static_cast<const UeConfiguration *>(configuration)->evaluate(property);
    }
}

template <typename T, typename... Property>
UeConfiguration &UeConfiguration::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), UePlatform>)
    {
        platform = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UePlatformGroup>)
    {
        if (std::ranges::find(platformGroups, property) == platformGroups.end())
        {
            platformGroups.emplace_back(property);
        }
    }
    else if constexpr (std::is_same_v<decltype(property), UeArchitecture>)
    {
        architecture = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UeBuildConfiguration>)
    {
        buildConfiguration = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UeTargetType>)
    {
        ueTargetType = property;
    }
    else
    {
        Configuration::assign(property);
    }

    if constexpr (sizeof...(properties))
    {
        return assign(properties...);
    }
    return *this;
}

template <typename T> bool UeConfiguration::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), UePlatform>)
    {
        return platform == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UePlatformGroup>)
    {
        return std::ranges::find(platformGroups, property) != platformGroups.end();
    }
    else if constexpr (std::is_same_v<decltype(property), UeArchitecture>)
    {
        return architecture == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UeBuildConfiguration>)
    {
        return buildConfiguration == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UeTargetType>)
    {
        return ueTargetType == property;
    }
    else
    {
        return Configuration::evaluate(property);
    }
}

template <typename Derived>
Derived &DSCExtension<UeCppTarget, Derived>::addNamedDependency(const DepType depType, const bool objectProducer,
                                                                const bool link, const string_view dependency)
{
    if (!configuration)
    {
        printErrorMessage("DSC<UeCppTarget> has no owning UeConfiguration.");
    }

    // A link relation reaches the dependency's implementation only when this module's implementation is itself
    // reachable. AddCppSource is deliberately absent from this decision: source-less producer dependencies still
    // forward reachability through their link relations.
    UeCppTarget &consumer = derived().getSourceTarget();
    DSC<UeCppTarget> &dependencyTarget =
        configuration->getOrAddTarget(dependency, link && consumer.implementationRequested);
    if (link)
    {
        consumer.linkDependencies.emplace_back(&dependencyTarget.getSourceTarget());
        derived().linkDeps(depType, dependencyTarget);
    }
    if (objectProducer)
    {
        derived().opDeps(depType, dependencyTarget);
    }
    return derived();
}

template <typename Derived>
Derived &DSCExtension<UeCppTarget, Derived>::addPrebuiltLibrary(DepType depType, const TargetType libraryType,
                                                                const NodeOrStr library)
{
    if (!configuration)
    {
        printErrorMessage("DSC<UeCppTarget> has no owning UeConfiguration.");
    }

    Node *libraryFile = library.resolve(true);
    PLOAT &prebuilt = configuration->getOrAddPrebuiltLibrary(libraryFile, libraryType);
    derived().linkDeps(depType, prebuilt);
    return derived();
}

#define UE_DEFINE_DSC_NAMED_DEPENDENCY(FunctionName, DependencyType, ObjectProducer, Link)                             \
    template <typename Derived>                                                                                        \
    Derived &DSCExtension<UeCppTarget, Derived>::FunctionName(const string_view dependency)                            \
    {                                                                                                                  \
        return addNamedDependency(DependencyType, ObjectProducer, Link, dependency);                                   \
    }

UE_DEFINE_DSC_NAMED_DEPENDENCY(publicDeps, DepType::PUBLIC, true, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(privateDeps, DepType::PRIVATE, true, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(interfaceDeps, DepType::INTERFACE, true, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(publicOpDeps, DepType::PUBLIC, true, false)
UE_DEFINE_DSC_NAMED_DEPENDENCY(privateOpDeps, DepType::PRIVATE, true, false)
UE_DEFINE_DSC_NAMED_DEPENDENCY(interfaceOpDeps, DepType::INTERFACE, true, false)
UE_DEFINE_DSC_NAMED_DEPENDENCY(publicLinkDeps, DepType::PUBLIC, false, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(privateLinkDeps, DepType::PRIVATE, false, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(interfaceLinkDeps, DepType::INTERFACE, false, true)

#undef UE_DEFINE_DSC_NAMED_DEPENDENCY

#define UE_DEFINE_DSC_PREBUILT_LIBRARY(FunctionName, DependencyType, LibraryType)                                      \
    template <typename Derived> Derived &DSCExtension<UeCppTarget, Derived>::FunctionName(const NodeOrStr library)     \
    {                                                                                                                  \
        return addPrebuiltLibrary(DependencyType, LibraryType, library);                                               \
    }

UE_DEFINE_DSC_PREBUILT_LIBRARY(publicPrebuiltStaticLibrary, DepType::PUBLIC, TargetType::PLIBRARY_STATIC)
UE_DEFINE_DSC_PREBUILT_LIBRARY(privatePrebuiltStaticLibrary, DepType::PRIVATE, TargetType::PLIBRARY_STATIC)
UE_DEFINE_DSC_PREBUILT_LIBRARY(interfacePrebuiltStaticLibrary, DepType::INTERFACE, TargetType::PLIBRARY_STATIC)
UE_DEFINE_DSC_PREBUILT_LIBRARY(publicPrebuiltSharedLibrary, DepType::PUBLIC, TargetType::PLIBRARY_SHARED)
UE_DEFINE_DSC_PREBUILT_LIBRARY(privatePrebuiltSharedLibrary, DepType::PRIVATE, TargetType::PLIBRARY_SHARED)
UE_DEFINE_DSC_PREBUILT_LIBRARY(interfacePrebuiltSharedLibrary, DepType::INTERFACE, TargetType::PLIBRARY_SHARED)

#undef UE_DEFINE_DSC_PREBUILT_LIBRARY

UeConfiguration &getUeConfiguration(const string &name = "Release");

// Parses the compact scanner-generated file list once. The resulting global
// registry is reused by every UeConfiguration in this process.
void registerGeneratedUeSpecifyFuncs(std::span<const UeIncludedFile> files);

#endif // HMAKE_UE_HPP
