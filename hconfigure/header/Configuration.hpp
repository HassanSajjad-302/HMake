#ifndef HMAKE_CONFIGURATION_HPP
#define HMAKE_CONFIGURATION_HPP

/**
 * @file Configuration.hpp
 * @brief Per-configuration settings and the primary C++ target factories.
 *
 * User build specifications normally include `Configure.hpp`, create configurations with
 * `getConfiguration()`, and set typed properties with `Configuration::assign()`. Target
 * declarations belong in `configurationSpecification(Configuration&)`.
 *
 * Properties passed to `assign()` are applied from left to right. Presets such as
 * `ConfigType::RELEASE` update several lower-level features, so put intentional overrides
 * after the preset.
 */

#include "BTarget.hpp"
#include "DSC.hpp"
#include "Features.hpp"
#include "IPCManagerCompiler.hpp"
#include <memory>

using std::shared_ptr, P2978::FileType;

class CppTarget;

/// Controls whether high-level `DSC<CppTarget>` factories add the configuration's standard C++ target.
enum class AssignStandardCppTarget : uint8_t
{
    NO,
    YES,
};

/// Enables test targets created by integrations such as `BoostCppTarget`.
enum class BuildTests : uint8_t
{
    NO,
    YES,
};

/// Enables example targets created by integrations such as `BoostCppTarget`.
enum class BuildExamples : uint8_t
{
    NO,
    YES,
};

/// Makes enabled test targets opt-in: name the target on the `hbuild` command line to build it.
enum class TestsExplicit : uint8_t
{
    NO,
    YES,
};

/// Makes enabled example targets opt-in: name the target on the `hbuild` command line to build it.
enum class ExamplesExplicit : uint8_t
{
    NO,
    YES,
};

/// Convenience property combining `BuildTests` and `TestsExplicit`.
/// `YES` enables tests as explicit targets; `NO` disables test creation.
enum class BuildTestsExplicitBuild : uint8_t
{
    NO,
    YES,
};

/// Convenience property combining `BuildExamples` and `ExamplesExplicit`.
/// `YES` enables examples as explicit targets; `NO` disables example creation.
enum class BuildExamplesExplicitBuild : uint8_t
{
    NO,
    YES,
};

/// Convenience property that enables or disables tests and examples together.
enum class BuildTestsAndExamples : uint8_t
{
    NO,
    YES,
};

/// Convenience property that enables tests and examples as explicit targets, or disables both.
enum class BuildTestsAndExamplesExplicitBuild : uint8_t
{
    NO,
    YES,
};

enum class IsCppMod : bool
{
    NO,
    YES,
};

enum class StdAsHeaderUnit : bool
{
    NO,
    YES,
};

enum class BigHeaderUnit : bool
{
    NO,
    YES,
};

/// Places eligible C++ implementation files into build-time generated jumbo translation units.
enum class JumboBuild : bool
{
    NO,
    YES,
};

/// Source-control query used by adaptive jumbo builds to keep locally edited files standalone.
enum class WorkingSetProvider : uint8_t
{
    NONE,
    GIT,
    PERFORCE,
};

/// Process-wide because every configuration in one invocation observes the same repository working set.
inline WorkingSetProvider adaptiveBuildWorkingSetProvider = WorkingSetProvider::NONE;

/// Controls whether targets discover and compile source files. Include paths, headers, and header units remain active.
enum class AddCppSource : bool
{
    NO,
    YES,
};

enum class TreatHUAsHeaderFile : bool
{
    NO,
    YES,
};

enum class SystemTarget : bool
{
    NO,
    YES,
};

enum class UseIPC : bool
{
    NO,
    YES,
};

enum class UseConfigurationScope : bool
{
    NO,
    YES,
};

/// Runs this configuration's initialization/specification even when the active build directory is outside it.
/// This is useful when another configuration consumes one of its generated outputs.
enum class AlwaysConfigureThis : bool
{
    NO,
    YES,
};

enum class StandAloneCommand : bool
{
    NO,
    YES,
};

enum class DuplicationWarning : bool
{
    NO,
    YES,
};

class CSourceTarget;
class PLOAT;
class LOAT;
class Node;

/// Internal lookup entry used while resolving header files, header units, and modules.
struct HfOrCppMod
{
    union CppModNodeUnion {
        class CppMod *cppMod;
        Node *node;

        constexpr CppModNodeUnion(Node *node_) : node(node_)
        {
        }

        constexpr CppModNodeUnion(CppMod *cppMod_) : cppMod(cppMod_)
        {
        }
    };

    CppModNodeUnion data;
    // Only used at build-time
    uint32_t targetIndex;
    FileType type;
    bool isSystem;
    // Following two are only used at build-time

    HfOrCppMod(const CppModNodeUnion cppModNodeUnion_, const FileType type_, const bool isSystem_)
        : data{cppModNodeUnion_}, type(type_), isSystem(isSystem_)
    {
    }

    // Only used at build-time
    HfOrCppMod(const uint32_t targetIndex_, const CppModNodeUnion cppModNodeUnion_, const FileType type_,
               const bool isSystem_)
        : data{cppModNodeUnion_}, targetIndex(targetIndex_), type(type_), isSystem(isSystem_)
    {
    }
};

/**
 * @brief Owns the targets and feature values for one named build configuration.
 *
 * Prefer the `getCpp*DSC()` family for normal C++ targets. A `DSC<CppTarget>` keeps
 * compilation and linking together and provides `privateDeps()`, `publicDeps()`, and
 * `interfaceDeps()`. The lower-level `getCppObject()`/`get*LOAT()` factories are available
 * when those two stages need to be assembled manually.
 *
 * Factory names are configuration-scoped by default: requesting `"app"` from configuration
 * `"Debug"` creates targets below `"Debug/app"`. The `*NoName()` variants preserve the
 * supplied name verbatim.
 *
 * @note `getConfiguration()` creates and registers a new object on every call; keep the
 * returned reference instead of using it as a name-based lookup.
 */
class Configuration : public BTarget
{
    // TODO:
    // Use alignas for those that are accessed at build-time to bring them in one cache line

  public:
    /// Targets owned by this configuration. These collections are primarily used by HMake internals.
    vector<class BoostCppTarget *> boostCppTargets;
    vector<CppTarget *> cppTargets;
    vector<LOAT *> loats;
    vector<PLOAT *> ploats;

    /// Typed compile, prebuilt-link, and link settings. `assign()` is the usual user-facing entry point.
    CppCompilerFeatures compilerFeatures;
    string cppCompileCommand;
    string cCompileCommand;
    string assemblyCompileCommand;
    /// ISPC policy is shared by every ISPC producer in this configuration. Targets append only their effective
    /// include paths and definitions to this invariant prefix.
    IspcCompilerFeatures ispcCompilerFeatures;
    string ispcCompileCommand;
    string ispcObjectCommandSuffix;
    PrebuiltLinkerFeatures ploatFeatures;
    LinkerFeatures linkerFeatures;
    string linkCommand;
    // Optional text placed immediately before and after linked dependency
    // libraries. Integrations can use this for linker groups or target-specific
    // runtime/system libraries while LOAT still owns object/library enumeration.
    string linkDependenciesPrefix;
    string linkCommandSuffix;
    string archiveCommand;

    /// Commands at or below this size are launched directly. Larger compile/link/archive commands use a response
    /// file in the owning CppTarget/LOAT build directory. Zero disables automatic response files.
    uint64_t responseFileThreshold = os == OS::NT ? 24 * 1024 : 128 * 1024;

    /// Standard-library dependency automatically attached by high-level target factories when enabled.
    DSC<CppTarget> *stdCppTarget = nullptr;

    /// Library kind selected by the generic `getCppTargetDSC*()` factories.
    TargetType targetType = TargetType::LIBRARY_STATIC;
    AssignStandardCppTarget assignStandardCppTarget = AssignStandardCppTarget::YES;
    BuildTests buildTests = BuildTests::NO;
    BuildExamples buildExamples = BuildExamples::NO;
    TestsExplicit testsExplicit = TestsExplicit::NO;
    ExamplesExplicit examplesExplicit = ExamplesExplicit::NO;
    IsCppMod isCppMod = IsCppMod::NO;
    StdAsHeaderUnit stdAsHeaderUnit = StdAsHeaderUnit::YES;
    BigHeaderUnit bigHeaderUnit = BigHeaderUnit::NO;
    JumboBuild jumboBuild = JumboBuild::NO;
    /// Approximate source-byte budget for each generated jumbo translation unit.
    uint64_t jumboFileSize = 384 * 1024;
    AddCppSource addCppSource = AddCppSource::YES;
    TreatHUAsHeaderFile treatHuAsHeaderFile = TreatHUAsHeaderFile::NO;
    SystemTarget systemTarget = SystemTarget::NO;
    UseIPC useIPC = UseIPC::YES;
    UseConfigurationScope useConfigurationScope = UseConfigurationScope::NO;
    AlwaysConfigureThis alwaysConfigureThis = AlwaysConfigureThis::NO;
    StandAloneCommand standAloneCommand = StandAloneCommand::NO;
    DuplicationWarning duplicationWarning = DuplicationWarning::NO;

    // todo
    // add CppTarget::imodNames map here as-well.

    // Following is used to have one hash-map search instead of going over every dependency. If useConfigurationScope ==
    // UseConfigurationScope::YES, then there must not be more than one value in the vector and the following is used at
    // config-time to check for duplication errors instead of CppTarget::reqHeaderNameMapping and
    // CppTarget::useReqHeaderNameMapping.
    flat_hash_map<string_view, vector<HfOrCppMod>> headerNameMapping;

    // only used at config-time
    // If useConfigurationScope == UseConfigurationScope::YES, then the following is used at config-time to check for
    // duplication errors instead of CppTarget::reqHeaderNameMapping and CppTarget::useReqHeaderNameMapping
    flat_hash_map<const Node *, FileType> nodesType;

    bool archiving = false;

    /**
     * @name Low-level object and linker factories
     *
     * The overloads taking `explicitBuild` and `myBuildDir` let integrations control
     * selective-build behavior and the target's build directory. Application code should
     * normally use the simpler overload or a high-level DSC factory.
     */
    /// @{
    CppTarget &getCppObject(const string &name_);
    CppTarget &getCppObject(bool explicitBuild, Node *myBuildDir, const string &name_);
    CppTarget &getCppObjectAddStdTarget(bool explicitBuild, Node *myBuildDir, const string &name_);

    LOAT &GetExeLOAT(const string &name_);
    LOAT &GetExeLOAT(bool explicitBuild, Node *myBuildDir, const string &name_);
    LOAT &getStaticLOAT(const string &name_);
    LOAT &getStaticLOAT(bool explicitBuild, Node *myBuildDir, const string &name_);
    LOAT &getSharedLOAT(const string &name_);
    LOAT &getSharedLOAT(bool explicitBuild, Node *myBuildDir, const string &name_);

    PLOAT &getPLOAT(const string &name_, Node *myBuildDir, TargetType linkTargetType_);
    PLOAT &getStaticPLOAT(const string &name_, Node *myBuildDir);
    PLOAT &getSharedPLOAT(const string &name_, Node *myBuildDir);
    /// @}

    /// Adds the configured standard C++ target when `AssignStandardCppTarget::YES`.
    CppTarget &addStdCppDep(CppTarget &target) const;
    DSC<CppTarget> &addStdDSCCppDep(DSC<CppTarget> &target) const;

    // CSourceTarget &GetCPT();

    /**
     * @name High-level C++ target factories
     *
     * These are the preferred factories for user specifications. `getCppExeDSC()` creates
     * an executable; `getCppStaticDSC()` and `getCppSharedDSC()` select a fixed library
     * kind; `getCppTargetDSC()` uses `targetType`; and `getCppObjectDSC()` creates a
     * compile-only target.
     *
     * When `defines` is true, the DSC configures the export definition named by `define`
     * (or its generated default when `define` is empty).
     */
    /// @{
    DSC<CppTarget> &getCppObjectDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppObjectDSC(bool explicitBuild, Node *myBuildDir, const string &name_, bool defines = false,
                                    string define = "");
    DSC<CppTarget> &getCppExeDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppExeDSC(bool explicitBuild, Node *myBuildDir, const string &name_, bool defines = false,
                                 string define = "");
    DSC<CppTarget> &getCppTargetDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppTargetDSC(bool explicitBuild, Node *myBuildDir, const string &name_, bool defines = false,
                                    string define = "");
    DSC<CppTarget> &getCppStaticDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppStaticDSC(bool explicitBuild, Node *myBuildDir, const string &name_, bool defines = false,
                                    string define = "");
    DSC<CppTarget> &getCppSharedDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppSharedDSC(bool explicitBuild, Node *myBuildDir, const string &name_, bool defines = false,
                                    string define = "");
    /// @}

    /**
     * @name Prebuilt-library factories
     * `_P` factories pair a source/interface target with a `PLOAT` instead of building the
     * linked artifact with a `LOAT`. `myBuildDir` identifies the directory containing the
     * prebuilt artifact.
     */
    /// @{
    DSC<CppTarget> &getCppTargetDSC_P(const string &name_, Node *myBuildDir, bool defines = false, string define = "");
    DSC<CppTarget> &getCppTargetDSC_P(const string &name_, const string &prebuiltName, Node *myBuildDir,
                                      bool defines = false, string define = "");
    DSC<CppTarget> &getCppStaticDSC_P(const string &name_, Node *myBuildDir, bool defines = false, string define = "");

    DSC<CppTarget> &getCppSharedDSC_P(const string &name_, Node *myBuildDir, bool defines = false, string define = "");
    /// @}

    /**
     * @name Unscoped-name factories
     * These variants do not prepend the configuration name. They are useful when an
     * externally fixed target/output name is required; callers are responsible for avoiding
     * collisions between configurations.
     */
    /// @{
    CppTarget &getCppObjectNoName(const string &name_);
    // Non-DSC functions do not add the standard target automatically.
    CppTarget &getCppObjectNoName(bool explicitBuild, Node *myBuildDir, const string &name_);
    CppTarget &getCppObjectNoNameAddStdTarget(bool explicitBuild, Node *myBuildDir, const string &name_);

    LOAT &GetExeLOATNoName(const string &name_);
    LOAT &GetExeLOATNoName(bool explicitBuild, Node *myBuildDir, const string &name_);
    LOAT &getStaticLOATNoName(const string &name_);
    LOAT &getStaticLOATNoName(bool explicitBuild, Node *myBuildDir, const string &name_);
    LOAT &getSharedLOATNoName(const string &name_);
    LOAT &getSharedLOATNoName(bool explicitBuild, Node *myBuildDir, const string &name_);

    PLOAT &getPLOATNoName(const string &name_, Node *myBuildDir, TargetType linkTargetType_);
    PLOAT &getStaticPLOATNoName(const string &name_, Node *myBuildDir);
    PLOAT &getSharedPLOATNoName(const string &name_, Node *myBuildDir);
    // CSourceTarget &GetCPTNoName();

    DSC<CppTarget> &getCppObjectDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppObjectDSCNoName(bool explicitBuild, Node *myBuildDir, const string &name_,
                                          bool defines = false, string define = "");
    DSC<CppTarget> &getCppExeDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppExeDSCNoName(bool explicitBuild, Node *myBuildDir, const string &name_, bool defines = false,
                                       string define = "");
    DSC<CppTarget> &getCppTargetDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppTargetDSCNoName(bool explicitBuild, Node *myBuildDir, const string &name_,
                                          bool defines = false, string define = "");
    DSC<CppTarget> &getCppStaticDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppStaticDSCNoName(bool explicitBuild, Node *myBuildDir, const string &name_,
                                          bool defines = false, string define = "");
    DSC<CppTarget> &getCppSharedDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppTarget> &getCppSharedDSCNoName(bool explicitBuild, Node *myBuildDir, const string &name_,
                                          bool defines = false, string define = "");

    // _P means it will use PLOAT instead of LOAT

    DSC<CppTarget> &getCppTargetDSC_PNoName(const string &name_, Node *myBuildDir, bool defines = false,
                                            string define = "");
    DSC<CppTarget> &getCppTargetDSC_PNoName(const string &name_, const string &prebuiltName, Node *myBuildDir,
                                            bool defines = false, string define = "");
    DSC<CppTarget> &getCppStaticDSC_PNoName(const string &name_, Node *myBuildDir, bool defines = false,
                                            string define = "");

    DSC<CppTarget> &getCppSharedDSC_PNoName(const string &name_, Node *myBuildDir, bool defines = false,
                                            string define = "");
    /// @}

    /// Creates the adapter used to configure a Boost-style project and its optional tests/examples.
    BoostCppTarget &getBoostCppTarget(const string &name, bool headerOnly = true, bool hasBigHeader = true,
                                      bool createTestsTarget = false, bool createExamplesTarget = false);

    /// Internal round-one hook; users normally declare work in `configurationSpecification()`.
    void completeRoundOne() override;

    /// Constructs a named configuration. Prefer `getConfiguration()` in build specifications.
    explicit Configuration(const string &name_);

    /// Finalizes target state that depends on the complete configuration specification.
    void postConfigurationSpecification() const;

    /// Copies user-configurable policy while leaving identity, targets, and materialized runtime state untouched.
    void copySettingsFrom(const Configuration &other);

    /// Resolves tool commands and creates the default standard C++ target.
    virtual void initialize();

    /// Reserved archive boundary hook. It currently has no effect.
    static void markArchivePoint();

    /**
     * Applies one or more typed properties from left to right.
     *
     * @code{.cpp}
     * auto &debug = getConfiguration("Debug");
     * debug.assign(ConfigType::DEBUG, Warnings::EXTRA, WarningsAsErrors::ON);
     * @endcode
     *
     * Place specific overrides after broad presets such as `ConfigType`.
     */
    template <typename T, typename... Property> Configuration &assign(T property, Property... properties);

    /// Returns whether a supported typed property matches this configuration.
    template <typename T> bool evaluate(T property) const;
};
bool operator<(const Configuration &lhs, const Configuration &rhs);

/// Registry of configurations in declaration order. Prefer retaining references returned by `getConfiguration()`.
extern vector<Configuration *> allConfigurations;

/**
 * Creates and registers a configuration.
 *
 * The default name is `"Release"`. This function does not search for an existing configuration
 * with the same name.
 */
Configuration &getConfiguration(const string &name = "Release");

template <typename T> bool Configuration::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), DSC<CppTarget> *>)
    {
        return stdCppTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AssignStandardCppTarget>)
    {
        return assignStandardCppTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTests>)
    {
        return buildTests == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildExamples>)
    {
        return buildExamples == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TestsExplicit>)
    {
        return testsExplicit == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExamplesExplicit>)
    {
        return examplesExplicit == property;
    }
    else if constexpr (std::is_same_v<decltype(property), IsCppMod>)
    {
        return isCppMod == property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdAsHeaderUnit>)
    {
        return stdAsHeaderUnit == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BigHeaderUnit>)
    {
        return bigHeaderUnit == property;
    }
    else if constexpr (std::is_same_v<decltype(property), JumboBuild>)
    {
        return jumboBuild == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddCppSource>)
    {
        return addCppSource == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TreatHUAsHeaderFile>)
    {
        return treatHuAsHeaderFile == property;
    }
    else if constexpr (std::is_same_v<decltype(property), SystemTarget>)
    {
        return systemTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UseIPC>)
    {
        return useIPC == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UseConfigurationScope>)
    {
        return useConfigurationScope == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AlwaysConfigureThis>)
    {
        return alwaysConfigureThis == property;
    }
    else if constexpr (std::is_same_v<decltype(property), StandAloneCommand>)
    {
        return standAloneCommand == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DuplicationWarning>)
    {
        return duplicationWarning == property;
    }
    // CppCompilerFeatures
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        return compilerFeatures.cxxStd == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExceptionHandling>)
    {
        return compilerFeatures.exceptionHandling == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RTTI>)
    {
        return compilerFeatures.rtti == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        static_assert(false, "Unsupported property passed to Configuration::evaluate");
    }
}

#endif // HMAKE_CONFIGURATION_HPP
