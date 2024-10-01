#ifndef HMAKE_LINKORARCHIVETARGET_HPP
#define HMAKE_LINKORARCHIVETARGET_HPP
#ifdef USE_HEADER_UNITS
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "HashedCommand";
import "Node.hpp";
import "RunCommand.hpp";
import "PrebuiltLinkOrArchiveTarget.hpp";
import "Utilities.hpp";
import <stack>;
#else
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "HashedCommand.hpp"
#include "RunCommand.hpp"
#include "PrebuiltLinkOrArchiveTarget.hpp"
#include <stack>
#endif
#include <ObjectFile.hpp>

using std::stack, std::filesystem::create_directories, std::shared_ptr;

struct LinkerFlags
{
    // GCC
    pstring OPTIONS;
    pstring OPTIONS_LINK;
    pstring LANG;
    pstring RPATH_OPTION_LINK;
    pstring FINDLIBS_ST_PFX_LINK;
    pstring FINDLIBS_SA_PFX_LINK;
    pstring HAVE_SONAME_LINK;
    pstring SONAME_OPTION_LINK;
    pstring DOT_IMPLIB_COMMAND_LINK_DLL;

    // Following two are directly used instead of being set
    pstring RPATH_LINK;
    pstring RPATH_LINK_LINK;

    bool isRpathOs = false;
    // MSVC
    pstring FINDLIBS_SA_LINK;
    pstring DOT_LD_LINK;
    pstring DOT_LD_ARCHIVE;
    pstring LINKFLAGS_LINK;
    pstring PDB_CFLAG;
    pstring ASMFLAGS_ASM;
    pstring PDB_LINKFLAG;
    pstring LINKFLAGS_MSVC;
};

class LinkOrArchiveTarget : public PrebuiltLinkOrArchiveTarget,
                            public LinkerFeatures,
                            public FeatureConvenienceFunctions<LinkOrArchiveTarget>
{
    using BaseType = PrebuiltLinkOrArchiveTarget;

  public:
    pstring buildCacheFilesDirPath;

    pstring linkOrArchiveCommandWithoutTargets;
    pstring linkOrArchiveCommandWithTargets;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    HashedCommand commandWithoutTargetsWithTool;

    vector<PrebuiltLinkOrArchiveTarget *> dllsToBeCopied;

    bool archiving = false;
    bool archived = false;

    LinkOrArchiveTarget(const pstring &name_, TargetType targetType);
    LinkOrArchiveTarget(bool buildExplicit, const pstring &name_, TargetType targetType);

    virtual pstring getLinkOrArchiveCommandWithoutTargets();

    void setOutputName(pstring outputName_);
    void setFileStatus(RealBTarget &realBTarget);
    void updateBTarget(Builder &builder, unsigned short round) override;

    LinkerFlags getLinkerFlags();
    pstring getTarjanNodeName() const override;
    RunCommand Archive();
    RunCommand Link();
    void setLinkOrArchiveCommands();
    pstring getLinkOrArchiveCommandPrint();
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    LinkOrArchiveTarget &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
};

bool operator<(const LinkOrArchiveTarget &lhs, const LinkOrArchiveTarget &rhs);

template <Dependency dependency, typename T, typename... Property>
LinkOrArchiveTarget &LinkOrArchiveTarget::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), Linker>)
    {
        linker = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BTFamily>)
    {
        linker.bTFamily = property;
    }
    else if constexpr (std::is_same_v<decltype(property), path>)
    {
        linker.bTPath = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LinkFlags>)
    {
        if constexpr (dependency == Dependency::PRIVATE)
        {
            requirementLinkerFlags += property;
        }
        else if constexpr (dependency == Dependency::INTERFACE)
        {

            usageRequirementLinkerFlags += property;
        }
        else
        {
            requirementLinkerFlags += property;
            usageRequirementLinkerFlags += property;
        }
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
    else if constexpr (std::is_same_v<decltype(property), Visibility>)
    {
        visibility = property;
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
        return property;
    }
    else
    {
        return PrebuiltLinkOrArchiveTarget::assign(property);
    }
    if constexpr (sizeof...(properties))
    {
        return assign(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool LinkOrArchiveTarget::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), Linker>)
    {
        return linker == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BTFamily>)
    {
        return linker.bTFamily == property;
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
    else if constexpr (std::is_same_v<decltype(property), Visibility>)
    {
        return visibility == property;
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
        return PrebuiltLinkOrArchiveTarget::evaluate(property);
    }
}

#endif // HMAKE_LINKORARCHIVETARGET_HPP
