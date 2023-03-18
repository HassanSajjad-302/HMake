#ifndef HMAKE_LINKORARCHIVETARGET_HPP
#define HMAKE_LINKORARCHIVETARGET_HPP
#ifdef USE_HEADER_UNITS
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "PostBasic.hpp";
#else
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "PostBasic.hpp"
#endif

struct LinkerFlags
{
    // GCC
    string OPTIONS;
    string OPTIONS_LINK;
    string LANG;
    string RPATH_LINK;
    string RPATH_OPTION_LINK;
    string FINDLIBS_ST_PFX_LINK;
    string FINDLIBS_SA_PFX_LINK;
    string HAVE_SONAME_LINK;
    string SONAME_OPTION_LINK;

    // MSVC
    string FINDLIBS_SA_LINK;
    string DOT_LD_LINK;
    string DOT_LD_ARCHIVE;
    string LINKFLAGS_LINK;
    string PDB_CFLAG;
    string ASMFLAGS_ASM;
    string PDB_LINKFLAG;
    string LINKFLAGS_MSVC;
};

using std::shared_ptr;
class LinkOrArchiveTarget : public CTarget,
                            public BTarget,
                            public LinkerFeatures,
                            public FeatureConvenienceFunctions<LinkOrArchiveTarget>,
                            public DS<LinkOrArchiveTarget>
{
  public:
    TargetType linkTargetType;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    string linkOrArchiveCommand;
    string linkOrArchiveCommandPrint;
    set<Node *> libraryDirectories;

    set<ObjectFile *> objectFiles;
    set<ObjectFileProducer *> objectFileProducers;
    set<string> cachedObjectFiles;

    string buildCacheFilesDirPath;
    string actualOutputName;

    LinkOrArchiveTarget(string name, TargetType targetType);
    LinkOrArchiveTarget(string name, TargetType targetType, class CTarget &other, bool hasFile = true);
    void initializeForBuild();
    void populateObjectFiles();
    void preSort(Builder &builder, unsigned short round) override;
    LinkerFlags getLinkerFlags();
    void updateBTarget(unsigned short round, Builder &builder) override;
    void setJson() override;
    BTarget *getBTarget() override;
    string getTarjanNodeName() override;
    PostBasic Archive();
    PostBasic Link();
    void addRequirementDepsToBTargetDependencies();
    void populateRequirementAndUsageRequirementProperties();
    void duringSort(Builder &builder, unsigned short round, unsigned int indexInTopologicalSortComparator) override;
    void setLinkOrArchiveCommandPrint();
    string getLinkOrArchiveCommand(bool ignoreTargets);
    string &getLinkOrArchiveCommandPrint();
    void checkForPreBuiltAndCacheDir(Builder &builder);
    string outputName;
    string outputDirectory;
    string usageRequirementLinkerFlags;
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    LinkOrArchiveTarget &ASSIGN(T property, Property... properties);
    template <typename T> bool EVALUATE(T property) const;
};
void to_json(Json &json, const LinkOrArchiveTarget &linkOrArchiveTarget);

template <Dependency dependency, typename T, typename... Property>
LinkOrArchiveTarget &LinkOrArchiveTarget::ASSIGN(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), Linker>)
    {
        linker = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        targetOs = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        threading = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        cxxStd = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        cxxStdDialect = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        debugSymbols = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        profiling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LocalVisibility>)
    {
        localVisibility = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        addressSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        leakSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        threadSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        undefinedSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        coverage = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        lto = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        ltoMode = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        runtimeLink = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        runtimeDebugging = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        arch = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        addModel = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        linkTargetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        debugStore = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        userInterface = property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        instructionSet = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        cpuType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Strip>)
    {
        strip = property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        linker = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
    if constexpr (sizeof...(properties))
    {
        return ASSIGN(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool LinkOrArchiveTarget::EVALUATE(T property) const
{
    if constexpr (std::is_same_v<decltype(property), Linker>)
    {
        return linker == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        return targetOs == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        return threading == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        return cxxStd == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        return cxxStdDialect == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        return debugSymbols == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        return profiling == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LocalVisibility>)
    {
        return localVisibility == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        return addressSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        return leakSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        return threadSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        return undefinedSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        return coverage == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        return lto == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        return ltoMode == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        return runtimeLink == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        return runtimeDebugging == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        return arch == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        return addModel == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        return debugStore == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        return userInterface == property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        return instructionSet == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        return cpuType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Strip>)
    {
        return strip == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        linker = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

#endif // HMAKE_LINKORARCHIVETARGET_HPP
