#ifndef HMAKE_CONFIGURATION_HPP
#define HMAKE_CONFIGURATION_HPP
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "LinkOrArchiveTarget.hpp";
import <memory>;
#else
#include "BTarget.hpp"
#include "LinkOrArchiveTarget.hpp"
#include <memory>
#endif

using std::shared_ptr;

class CppSourceTarget;

enum class AssignStandardCppTarget : char
{
    NO,
    YES,
};

enum class BuildTests : char
{
    NO,
    YES,
};

enum class BuildExamples : char
{
    NO,
    YES,
};

enum class TestsExplicit : char
{
    NO,
    YES,
};

enum class ExamplesExplicit : char
{
    NO,
    YES,
};

enum class BuildTestsExplicitBuild : char
{
    NO,
    YES,
};

enum class BuildExamplesExplicitBuild : char
{
    NO,
    YES,
};

enum class BuildTestsAndExamples : char
{
    NO,
    YES,
};

enum class BuildTestsAndExamplesExplicitBuild : char
{
    NO,
    YES,
};

struct CppTargetAndParentDirNode
{
    CppSourceTarget *target;
    Node *incl;
};

class CSourceTarget;
class Configuration : public BTarget
{
  public:
    flat_hash_map<Node *, CppTargetAndParentDirNode> moduleFilesToTarget;
    vector<CppSourceTarget *> cppSourceTargets;
    vector<LinkOrArchiveTarget *> linkOrArchiveTargets;
    vector<PrebuiltLinkOrArchiveTarget *> prebuiltLinkOrArchiveTargets;
    vector<CSourceTarget *> prebuiltTargets;
    CppCompilerFeatures compilerFeatures;
    CompilerFlags compilerFlags;
    PrebuiltLinkerFeatures prebuiltLinkOrArchiveTargetFeatures;
    LinkerFeatures linkerFeatures;
    DSC<CppSourceTarget> *stdCppTarget = nullptr;
    TargetType targetType = TargetType::LIBRARY_STATIC;
    AssignStandardCppTarget assignStandardCppTarget = AssignStandardCppTarget::YES;
    BuildTests buildTests = BuildTests::NO;
    BuildExamples buildExamples = BuildExamples::NO;
    TestsExplicit testsExplicit = TestsExplicit::NO;
    ExamplesExplicit examplesExplicit = ExamplesExplicit::NO;

    bool archiving = false;

    CppSourceTarget &getCppObject(const string &name_);
    CppSourceTarget &getCppObject(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_);
    CppSourceTarget &getCppObjectAddStdTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                              const string &name_);

    LinkOrArchiveTarget &GetExeLinkOrArchiveTarget(const string &name_);
    LinkOrArchiveTarget &GetExeLinkOrArchiveTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                   const string &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(const string &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                      const string &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(const string &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                      const string &name_);

    PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget(const string &name_, const string &directory,
                                                                TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTarget(const string &name_, const string &directory);
    PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTarget(const string &name_, const string &directory);
    CppSourceTarget &addStdCppDep(CppSourceTarget &target);
    DSC<CppSourceTarget> &addStdDSCCppDep(DSC<CppSourceTarget> &target);

    // CSourceTarget &GetCPT();

    DSC<CppSourceTarget> &getCppExeDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppExeDSC(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_,
                                       bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppTargetDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppTargetDSC(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                          const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppStaticDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppStaticDSC(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                          const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppSharedDSC(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppSharedDSC(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                          const string &name_, bool defines = false, string define = "");

    // _P means it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget

    DSC<CppSourceTarget> &getCppTargetDSC_P(const string &name_, const string &directory, bool defines = false,
                                            string define = "");
    DSC<CppSourceTarget> &getCppTargetDSC_P(const string &name_, const string &prebuiltName, const string &directory,
                                            bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppStaticDSC_P(const string &name_, const string &directory, bool defines = false,
                                            string define = "");

    DSC<CppSourceTarget> &getCppSharedDSC_P(const string &name_, const string &directory, bool defines = false,
                                            string define = "");

    // These NoName functions do not prepend configuration name to the target name.

    CppSourceTarget &getCppObjectNoName(const string &name_);
    // non-DSC functions do not add the Std target as dependency, so we define a new function with
    CppSourceTarget &getCppObjectNoName(bool explicitBuild, const string &buildCacheFilesDirPath_, const string &name_);
    CppSourceTarget &getCppObjectNoNameAddStdTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                    const string &name_);

    LinkOrArchiveTarget &GetExeLinkOrArchiveTargetNoName(const string &name_);
    LinkOrArchiveTarget &GetExeLinkOrArchiveTargetNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                         const string &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTargetNoName(const string &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTargetNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                            const string &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTargetNoName(const string &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTargetNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                            const string &name_);

    PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTargetNoName(const string &name_, const string &directory,
                                                                      TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTargetNoName(const string &name_,
                                                                            const string &directory);
    PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTargetNoName(const string &name_,
                                                                            const string &directory);
    // CSourceTarget &GetCPTNoName();

    DSC<CppSourceTarget> &getCppExeDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppExeDSCNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                             const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppTargetDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppTargetDSCNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppStaticDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppStaticDSCNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppSharedDSCNoName(const string &name_, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppSharedDSCNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                const string &name_, bool defines = false, string define = "");

    // _P means it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget

    DSC<CppSourceTarget> &getCppTargetDSC_PNoName(const string &name_, const string &directory, bool defines = false,
                                                  string define = "");
    DSC<CppSourceTarget> &getCppTargetDSC_PNoName(const string &name_, const string &prebuiltName,
                                                  const string &directory, bool defines = false, string define = "");
    DSC<CppSourceTarget> &getCppStaticDSC_PNoName(const string &name_, const string &directory, bool defines = false,
                                                  string define = "");

    DSC<CppSourceTarget> &getCppSharedDSC_PNoName(const string &name_, const string &directory, bool defines = false,
                                                  string define = "");

    explicit Configuration(const string &name_);
    void initialize();
    static void markArchivePoint();
    template <typename T, typename... Property> Configuration &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
};
bool operator<(const Configuration &lhs, const Configuration &rhs);
Configuration &getConfiguration(const string &name = "Release");

template <typename T> bool Configuration::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), DSC<CppSourceTarget> *>)
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
