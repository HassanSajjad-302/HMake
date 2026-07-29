#ifndef HMAKE_UE_HPP
#define HMAKE_UE_HPP

#include "CppTarget.hpp"
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

    void completeRoundOne() override;

    // UBT: ModuleRules.ConditionalAddModuleDirectory(). Ordinary module roots are
    // automatic; this is only for an exceptional additional source directory.
    // Returns false without registering anything when the directory does not exist.
    bool conditionalAddModuleDirectory(NodeOrStr directory);

    // UBT: ModuleRules.ShortName. UBT uses this value for intermediate paths,
    // including the existing Inc/<ShortName>/UHT directory consumed here.
    UeCppTarget &setShortName(string_view value);

    // Temporary compatibility escape hatch for UE's legacy circular module graph.
    // These preserve compile requirements without adding scheduler edges. New code
    // should use ordinary DSC dependencies; the long-term goal is to remove the UE
    // cycles and these two functions with them.
    UeCppTarget &addPrivateCycleDependency(string_view dependency);
    UeCppTarget &addPublicCycleDependency(string_view dependency);

  private:
    // UBT: ModuleRules.GetAllModuleDirectories(). HMake derives these directories
    // from the selected base/platform *.module.hmake.hpp files instead of asking
    // the user to register the conventional source root.
    vector<Node *> moduleDirectories;

    // Defaults to logicalName and is overridden through ModuleRules.ShortName.
    string intermediateName;

    // UBT: SourceFileMetadataCache.GetListOfInlinedGeneratedCppFiles(). Names in
    // this set are included by handwritten .cpp files and must not be compiled
    // again as independent *.gen.cpp translation units.
    std::unordered_set<string> inlinedGeneratedCppNames;

    bool roundOneCalled = false;
    bool selectiveBuildSet = false;

    void propagateSelectiveBuild();
    void prepareModuleInputs(bool compileSources);
    void findInputFiles(Node *moduleDirectory);
    void addDefaultIncludePaths(Node *moduleDirectory);

    // Adds the UHT include directory and registers every *.gen.cpp independently.
    // ObjectMacros.h turns UE_INLINE_GENERATED_CPP_BY_NAME into an empty include
    // when HMAKE_COMPILE_GENERATED_CPP_SEPARATELY is defined.
    UeCppTarget &addGeneratedCode(Node *directory, bool compileSources);

    friend class UeConfiguration;
};

/**
 * Adds UE name-based dependency functions to DSC<UeCppTarget>.
 *
 * The closest UBT ModuleRules mappings are:
 *   publicDeps         -> PublicDependencyModuleNames
 *   privateDeps        -> PrivateDependencyModuleNames
 *   publicCompileDeps  -> PublicIncludePathModuleNames (compile visibility only)
 *   privateCompileDeps -> PrivateIncludePathModuleNames (compile visibility only)
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
    Derived &publicCompileDeps(string_view dependency);
    Derived &privateCompileDeps(string_view dependency);
    Derived &interfaceCompileDeps(string_view dependency);
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
    Derived &publicCompileDeps(string_view dependency, Names &&...dependencies)
    {
        publicCompileDeps(dependency);
        (publicCompileDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &privateCompileDeps(string_view dependency, Names &&...dependencies)
    {
        privateCompileDeps(dependency);
        (privateCompileDeps(string_view(std::forward<Names>(dependencies))), ...);
        return derived();
    }

    template <typename... Names>
        requires(sizeof...(Names) > 0 && (std::convertible_to<Names, string_view> && ...))
    Derived &interfaceCompileDeps(string_view dependency, Names &&...dependencies)
    {
        interfaceCompileDeps(dependency);
        (interfaceCompileDeps(string_view(std::forward<Names>(dependencies))), ...);
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

    Derived &addNamedDependency(DepType depType, bool compile, bool link, string_view dependency);
    Derived &addPrebuiltLibrary(DepType depType, TargetType libraryType, NodeOrStr library);
};

using UeSpecifyFunction = void (*)(UeConfiguration &);

/**
 * Minimal information scanner.py must emit for one included *.hmake.hpp file.
 *
 * C++ cannot recover a function symbol from an #include path at runtime, so the
 * generated translation unit pairs the path with the included file's specify()
 * function. registerGeneratedUeSpecifyFuncs() derives all remaining metadata from
 * the filename.
 */
struct UeIncludedFile
{
    string_view path;
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
    using Configuration::assign;
    using Configuration::evaluate;

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

    explicit UeConfiguration(const string &name);

    // Adds a graph root, analogous to the target descriptors passed to UBT.
    UeConfiguration &requestTarget(string logicalName);
    UeConfiguration &setPlatform(UePlatform value, vector<UePlatformGroup> groups = {});
    UeConfiguration &setArchitecture(UeArchitecture value);
    UeConfiguration &setBuildConfiguration(UeBuildConfiguration value);
    UeConfiguration &setUeTargetType(UeTargetType value);
    UeConfiguration &setGeneratedIncludeRoot(Node *value);
    UeConfiguration &setBuildCommands(UeBuildCommands value);
    UeConfiguration &setBuildCommands(std::span<const UeBuildCommandEntry> entries);
    void initialize() override;

    // Convenient equivalents of common Build.cs conditions such as
    // Target.Platform == ... and Target.Platform.IsInGroup(...).
    bool evaluate(UePlatform value) const;
    bool evaluate(UePlatformGroup value) const;
    bool evaluate(UeArchitecture value) const;
    bool evaluate(UeBuildConfiguration value) const;
    bool evaluate(UeTargetType value) const;

    // Returns the target whose specify() function is currently running. A
    // decentralized module file uses this instead of receiving its target directly.
    DSC<UeCppTarget> &currentTarget() const;

    // Lazily creates/configures a named module. Closest UBT operation:
    // UEBuildTarget.FindOrCreateModuleByName().
    DSC<UeCppTarget> &getOrAddTarget(string_view logicalName);

    // Starts graph expansion from requestTarget() roots.
    void configureRequestedTargets();

  private:
    // Cache record for one evaluated module. UBT similarly keeps a name-to-module
    // map so repeated dependencies reuse the same UEBuildModule instance.
    struct ConfiguredTarget
    {
        UeTargetState state = UeTargetState::Unconfigured;
        UeFileKind kind = UeFileKind::Module;
        DSC<UeCppTarget> *dsc = nullptr;
        // UBT can create a UEBuildModule solely to consume its headers. Only a
        // module reached by a full/link dependency belongs to the monolithic binary.
        bool buildModule = false;
    };

    // Evaluated graph objects: closest UBT analogue is UEBuildTarget's module cache.
    flat_hash_map<string, ConfiguredTarget> configuredTargets;

    // Establishes currentTarget() while nested dependency specifications execute.
    vector<DSC<UeCppTarget> *> currentTargetStack;

    // Top-level graph roots supplied with requestTarget().
    vector<string> requestedTargets;

    // Optional commands selected from a generated UBT bootstrap table.
    std::optional<UeBuildCommands> buildCommands;

    // One PLOAT per physical prebuilt library, shared by all modules in this
    // UeConfiguration.
    flat_hash_map<string, PLOAT *> prebuiltLibraries;

    DSC<UeCppTarget> &makeDscUeCppTarget(string logicalName, UeFileKind fileKind);
    PLOAT &getOrAddPrebuiltLibrary(Node *libraryFile, TargetType libraryType);
    void markTargetForBinary(string_view logicalName);
    void finalizeMonolithicGraph();
    void initializeApiMacro(DSC<UeCppTarget> &target, bool defines) const;

    template <typename, typename> friend struct DSCExtension;
};

template <typename Derived>
Derived &DSCExtension<UeCppTarget, Derived>::addNamedDependency(const DepType depType, const bool compile,
                                                                const bool link, const string_view dependency)
{
    if (!configuration)
    {
        printErrorMessage("DSC<UeCppTarget> has no owning UeConfiguration.");
    }

    DSC<UeCppTarget> &dependencyTarget = configuration->getOrAddTarget(dependency);
    if (link)
    {
        // Monolithic UE modules are object targets, so no module-to-module LOAT
        // edge is created. Record binary membership for final attachment to the
        // one target executable instead.
        configuration->markTargetForBinary(dependency);
    }
    if (compile)
    {
        derived().compileDeps(depType, dependencyTarget);
    }
    if (link)
    {
        derived().linkDeps(depType, dependencyTarget);
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
    if (derived().ploat != nullptr)
    {
        PLOAT &consumer = derived().getPLOAT();

        // A static archive does not absorb its private link dependencies. They must
        // reach the final executable/shared library, matching DSC::addLinkDependency().
        if (depType == DepType::PRIVATE && consumer.linkTargetType != TargetType::LIBRARY_SHARED)
        {
            depType = DepType::PUBLIC;
        }
        consumer.deps(depType, prebuilt);
    }
    return derived();
}

#define UE_DEFINE_DSC_NAMED_DEPENDENCY(FunctionName, DependencyType, Compile, Link)                                    \
    template <typename Derived>                                                                                        \
    Derived &DSCExtension<UeCppTarget, Derived>::FunctionName(const string_view dependency)                            \
    {                                                                                                                  \
        return addNamedDependency(DependencyType, Compile, Link, dependency);                                          \
    }

UE_DEFINE_DSC_NAMED_DEPENDENCY(publicDeps, DepType::PUBLIC, true, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(privateDeps, DepType::PRIVATE, true, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(interfaceDeps, DepType::INTERFACE, true, true)
UE_DEFINE_DSC_NAMED_DEPENDENCY(publicCompileDeps, DepType::PUBLIC, true, false)
UE_DEFINE_DSC_NAMED_DEPENDENCY(privateCompileDeps, DepType::PRIVATE, true, false)
UE_DEFINE_DSC_NAMED_DEPENDENCY(interfaceCompileDeps, DepType::INTERFACE, true, false)
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
