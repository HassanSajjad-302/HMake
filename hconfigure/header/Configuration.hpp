#ifndef HMAKE_CONFIGURATION_HPP
#define HMAKE_CONFIGURATION_HPP

#include "BTarget.hpp"
#include "DSC.hpp"
#include "Features.hpp"
#include "TargetCache.hpp"
#include <memory>

using std::shared_ptr;

class CppTarget;

enum class AssignStandardCppTarget : uint8_t
{
    NO,
    YES,
};

// Whether tests should be built
enum class BuildTests : uint8_t
{
    NO,
    YES,
};

// Whether Examples should be built
enum class BuildExamples : uint8_t
{
    NO,
    YES,
};

// Setting this to YES will not build tests by-default and the test target will have to be named on the hbuild
// command-line to be built.
enum class TestsExplicit : uint8_t
{
    NO,
    YES,
};

// Seeting this to YES will not build examples by-default and the example target will have to be named on the hbuild
// command-line to be built.
enum class ExamplesExplicit : uint8_t
{
    NO,
    YES,
};

enum class BuildTestsExplicitBuild : uint8_t
{
    NO,
    YES,
};

enum class BuildExamplesExplicitBuild : uint8_t
{
    NO,
    YES,
};

enum class BuildTestsAndExamples : uint8_t
{
    NO,
    YES,
};

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

enum class SystemTarget : bool
{
    NO,
    YES,
};

enum class IgnoreHeaderDeps : bool
{
    NO,
    YES,
};

class CSourceTarget;
class PLOAT;
class LOAT;
class Node;

class Configuration : public BTarget
{
  public:
    vector<class BoostCppTarget *> boostCppTargets;
    vector<CppTarget *> cppTargets;
    vector<LOAT *> loats;
    vector<PLOAT *> ploats;
    CppCompilerFeatures compilerFeatures;
    CompilerFlags compilerFlags;
    PrebuiltLinkerFeatures ploatFeatures;
    LinkerFeatures linkerFeatures;
    LinkerFlags linkerFlags;
    DSC<CppTarget> *stdCppTarget = nullptr;
    TargetType targetType = TargetType::LIBRARY_STATIC;
    AssignStandardCppTarget assignStandardCppTarget = AssignStandardCppTarget::YES;
    BuildTests buildTests = BuildTests::NO;
    BuildExamples buildExamples = BuildExamples::NO;
    TestsExplicit testsExplicit = TestsExplicit::NO;
    ExamplesExplicit examplesExplicit = ExamplesExplicit::NO;
    IsCppMod isCppMod = IsCppMod::NO;
    StdAsHeaderUnit stdAsHeaderUnit = StdAsHeaderUnit::YES;
    BigHeaderUnit bigHeaderUnit = BigHeaderUnit::NO;
    SystemTarget systemTarget = SystemTarget::NO;
    IgnoreHeaderDeps ignoreHeaderDeps = IgnoreHeaderDeps::NO;

    bool archiving = false;

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
    CppTarget &addStdCppDep(CppTarget &target);
    DSC<CppTarget> &addStdDSCCppDep(DSC<CppTarget> &target) const;

    // CSourceTarget &GetCPT();

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

    // _P means it will use PLOAT instead of LOAT

    DSC<CppTarget> &getCppTargetDSC_P(const string &name_, Node *myBuildDir, bool defines = false, string define = "");
    DSC<CppTarget> &getCppTargetDSC_P(const string &name_, const string &prebuiltName, Node *myBuildDir,
                                      bool defines = false, string define = "");
    DSC<CppTarget> &getCppStaticDSC_P(const string &name_, Node *myBuildDir, bool defines = false, string define = "");

    DSC<CppTarget> &getCppSharedDSC_P(const string &name_, Node *myBuildDir, bool defines = false, string define = "");

    // These NoName functions do not prepend configuration name to the target name.

    CppTarget &getCppObjectNoName(const string &name_);
    // non-DSC functions do not add the Std target as dependency, so we define a new function with
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

    BoostCppTarget &getBoostCppTarget(const string &name, bool headerOnly = true, bool hasBigHeader = true,
                                      bool createTestsTarget = false, bool createExamplesTarget = false);

    explicit Configuration(const string &name_);
    void postConfigurationSpecification() const;
    void initialize();
    static void markArchivePoint();
    template <typename T, typename... Property> Configuration &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
};
bool operator<(const Configuration &lhs, const Configuration &rhs);
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
    else if constexpr (std::is_same_v<decltype(property), SystemTarget>)
    {
        return systemTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), IgnoreHeaderDeps>)
    {
        return ignoreHeaderDeps == property;
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
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        static_assert(false);
    }
}

#endif // HMAKE_CONFIGURATION_HPP
