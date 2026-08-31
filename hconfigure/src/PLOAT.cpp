#include "PLOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CppMod.hpp"
#include "Configuration.hpp"
#include "LOAT.hpp"
#include "ObjectFileProducer.hpp"
#include <algorithm>
#include <utility>

namespace
{
constexpr uint32_t ploatDependencyTypeBits = 1;
constexpr uint32_t acyclicPloatDependencyMask = 1;

uint32_t packPloatDependency(const PLOAT *dependency, const PloatDepInfo dependencyInfo)
{
    return dependency->cacheIndex << ploatDependencyTypeBits |
           (dependencyInfo.isAcyclicDependency() ? acyclicPloatDependencyMask : 0);
}
} // namespace

string PLOAT::getOutputName() const
{
#ifdef BUILD_MODE
    return getTargetNameFromActualName(linkTargetType, os, getLastNameAfterSlash(outputFileNode->filePath));
#else
    return outputName;
#endif
}

string PLOAT::getActualOutputName() const
{
#ifdef BUILD_MODE
    return getLastNameAfterSlash(outputFileNode->filePath);
#else
    return actualOutputName;
#endif
}

string_view PLOAT::getOutputDirectoryV() const
{
#ifdef BUILD_MODE
    return getNameBeforeLastSlashV(outputFileNode->filePath);
#else
    return outputDirectory->filePath;
#endif
}

static bool getLaunchesProcessPloat(const TargetType t)
{
    return t != TargetType::PLIBRARY_SHARED && t != TargetType::PLIBRARY_STATIC;
}

static BTargetType getBTargetTypePloat(const TargetType t)
{
    return t != TargetType::PLIBRARY_SHARED && t != TargetType::PLIBRARY_STATIC ? BTargetType::LOAT
                                                                                : BTargetType::PLOAT;
}

#ifdef BUILD_MODE
PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, const TargetType linkTargetType_)
    : BTarget(outputName_, getLaunchesProcessPloat(linkTargetType_), getBTargetTypePloat(linkTargetType_), false,
              false),
      config(config_), linkTargetType{linkTargetType_}
{
    initializePLOAT();
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, const TargetType linkTargetType_,
             string name_, bool buildExplicit, bool makeDirectory)
    : BTarget(name_, getLaunchesProcessPloat(linkTargetType_), getBTargetTypePloat(linkTargetType_), buildExplicit,
              makeDirectory),
      config(config_), linkTargetType(linkTargetType_)

{
    initializePLOAT();
}

void PLOAT::initializePLOAT()
{
    const char *ptr = bTargetCaches[cacheIndex].configCache.data();
    outputFileNode = readHalfNode(ptr, configCacheBytesRead);
    if constexpr (os == OS::NT)
    {
        if (config.linkerFeatures.linker.bTFamily == BTFamily::MSVC &&
            (linkTargetType == TargetType::LIBRARY_SHARED || linkTargetType == TargetType::PLIBRARY_SHARED))
        {
            importLibraryNode = readHalfNode(ptr, configCacheBytesRead);
        }
    }
}

#else

PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_)
    : BTarget(outputName_, getLaunchesProcessPloat(linkTargetType_), getBTargetTypePloat(linkTargetType_), false,
              false),
      outputDirectory(myBuildDir_), outputName{getLastNameAfterSlash(outputName_)}, config(config_),
      linkTargetType{linkTargetType_}
{
    if (linkTargetType == TargetType::PLIBRARY_STATIC || linkTargetType == TargetType::PLIBRARY_SHARED)
    {
        if (outputDirectory)
        {
            return;
        }
        printErrorMessage(
            FORMAT("Prebuilt library requires a build directory.\nLibrary: {}\nBuild directory: <empty>", name));
    }
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_,
             string name_, bool buildExplicit, bool makeDirectory)
    : BTarget(name_, getLaunchesProcessPloat(linkTargetType_), getBTargetTypePloat(linkTargetType_), buildExplicit,
              makeDirectory),
      outputDirectory(myBuildDir_), outputName(outputName_), config(config_), linkTargetType(linkTargetType_)

{
    if (linkTargetType == TargetType::PLIBRARY_STATIC || linkTargetType == TargetType::PLIBRARY_SHARED)
    {
        if (outputDirectory)
        {
            return;
        }
        printErrorMessage(
            FORMAT("Prebuilt library requires a build directory.\nLibrary: {}\nBuild directory: <empty>", name));
    }
}

#endif

void PLOAT::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
        return;
    }
    rb.reasonForUpdate = nullptr;
    if (outputFileNode->fileType != file_type::regular ||
        (importLibraryNode && importLibraryNode->fileType != file_type::regular))
    {
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }

    uint64_t artifactTime = outputFileNode->lastWriteTime;
    if (importLibraryNode)
    {
        artifactTime = std::max(artifactTime, importLibraryNode->lastWriteTime);
    }

    // Prebuilt PLOATs have no process footer; consumers use the newest artifact timestamp as their completion time.
    if (!launchesProcess)
    {
        rb.completionTime = artifactTime;
    }
    else if (artifactTime > rb.completionTime)
    {
        // An owned output modified after this process last completed must be regenerated before consumers use it.
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }
    BTarget::setUpdateStatus();
}

void PLOAT::completeRoundOne()
{
    // actualOutputName, outputDirectory, and outputName do not exist in the build-mode PLOAT layout, so this phase
    // split must happen in the preprocessor rather than a non-template discarded statement.
#ifdef BUILD_MODE
    readCacheAtBuildTime();
    outputFileNode->doStatFile = true;
    if (importLibraryNode)
    {
        importLibraryNode->doStatFile = true;
    }
#else
    actualOutputName = getActualNameFromTargetName(linkTargetType, os, outputName);

    // A prebuilt library requires outputDirectory at construction; a generated LOAT defaults to its build directory.
    if (!outputDirectory)
    {
        outputDirectory = static_cast<LOAT *>(this)->myBuildDir;
    }
    string outputPath(outputDirectory->filePath);
    outputPath += slashc;
    outputPath += actualOutputName;
    outputFileNode = Node::getNode<PathType::NORMAL_ABSOLUTE>(std::move(outputPath), true, true);
    if constexpr (os == OS::NT)
    {
        if (config.linkerFeatures.linker.bTFamily == BTFamily::MSVC &&
            (linkTargetType == TargetType::LIBRARY_SHARED || linkTargetType == TargetType::PLIBRARY_SHARED))
        {
            string importLibraryPath(outputDirectory->filePath);
            importLibraryPath += slashc;
            importLibraryPath += outputName;
            importLibraryPath += ".lib";
            importLibraryNode =
                Node::getNode<PathType::NORMAL_ABSOLUTE>(std::move(importLibraryPath), true, true);
        }
    }

    // Outputless producers carry their linked-library interface until a physical output consumes their objects.
    // Materialize that interface here, before flattening PLOAT-to-PLOAT requirements. A shared library absorbs
    // PRIVATE requirements; every other PLOAT kind must continue exporting them.
    for (ObjectFileProducer *root : rootObjectFileProducers)
    {
        for (const auto &[dependency, dependencyInfo] : root->reqPloatDeps)
        {
            if (dependency == this)
            {
                printErrorMessage(FORMAT("A deferred link dependency resolves to its consuming PLOAT.\nTarget: {}",
                                         getPrintName()));
            }
            mergePloatDependency(reqDeps, dependency, dependencyInfo);
            if (linkTargetType != TargetType::LIBRARY_SHARED)
            {
                mergePloatDependency(useReqDeps, dependency, dependencyInfo);
            }
        }
        for (const auto &[dependency, dependencyInfo] : root->useReqPloatDeps)
        {
            if (dependency == this)
            {
                printErrorMessage(FORMAT("A deferred link dependency resolves to its consuming PLOAT.\nTarget: {}",
                                         getPrintName()));
            }
            mergePloatDependency(useReqDeps, dependency, dependencyInfo);
        }
    }

    populateReqAndUseReqDeps();
#endif

    // Root producers have completed round one, so their object availability is final. PLOAT owns every blocking
    // round-zero relation needed to materialize its linker inputs. Every link path contributes objects, while only an
    // acyclic path may become a scheduler edge.
    for (ObjectFileProducer *root : rootObjectFileProducers)
    {
        hasObjectFiles |= root->hasObjectFiles;
        if constexpr (bsMode == BSMode::BUILD)
        {
            if (root->hasObjectFiles)
            {
                realBTargets[0].addDep<BTargetType::UNKNOWN>(&root->realBTargets[0]);
            }
        }

        FOR_REQ_OBJECT_FILE_PRODUCERS(root, producer, dependency)
        {
            if (!dependency.isLinkDependency() || !producer->hasObjectFiles)
            {
                continue;
            }

            hasObjectFiles = true;
            if constexpr (bsMode == BSMode::BUILD)
            {
                if (dependency.isAcyclicDependency())
                {
                    realBTargets[0].addDep<BTargetType::UNKNOWN>(&producer->realBTargets[0]);
                }
            }
        }
    }

#ifdef BUILD_MODE
    for (const uint32_t packedDependency : cachedReqDeps)
    {
        PLOAT *reqDep = static_cast<PLOAT *>(
            bTargetCaches[PloatDepInfo::getCacheIndex(packedDependency)].bTarget);
        const PloatDepInfo dependencyInfo = PloatDepInfo::fromCache(packedDependency);
        if (reqDep->suppliesLinkerInput() && dependencyInfo.isAcyclicDependency())
        {
            if (linkTargetType == TargetType::LIBRARY_STATIC)
            {
                realBTargets[0].addDep<BTargetType::LOAT, RelationType::LOOSE>(&reqDep->realBTargets[0]);
            }
            else
            {
                realBTargets[0].addDep<BTargetType::LOAT>(&reqDep->realBTargets[0]);
            }
        }
    }
#endif
}

void PLOAT::readCacheAtBuildTime()
{
    const string_view configCache = bTargetCaches[cacheIndex].configCache;
    const char *ptr = configCache.data();

    const uint32_t reqVecSize = readUint32(ptr, configCacheBytesRead);
    for (uint32_t i = 0; i < reqVecSize; ++i)
    {
        cachedReqDeps.emplace_back(readUint32(ptr, configCacheBytesRead));
    }
}

void PLOAT::populateReqAndUseReqDeps()
{
    const auto populate = [this](PloatDepInfoMap &dependencies) {
        // PLOAT cycle edges intentionally omit scheduler ordering. Walk the already-declared exported relationships
        // to a fixed point so the cached link closure is independent of round-one completion order.
        STACK_PMR_VECTOR(PLOAT *, pending, 1024)
        pending.reserve(dependencies.size() + 1);
        for (const auto &entry : dependencies)
        {
            if (entry.first != this)
            {
                pending.emplace_back(entry.first);
            }
        }

        for (uint64_t position = 0; position < pending.size(); ++position)
        {
            PLOAT *dependency = pending[position];
            const PloatDepInfo dependencyInfo = dependencies.find(dependency)->second;
            for (const auto &[exportedDependency, exportedInfo] : dependency->useReqDeps)
            {
                // Returning to the root adds no linker input. Its direct exports were seeded above, and skipping the
                // self-entry avoids mutating this map through an aliased exported map.
                if (exportedDependency == this)
                {
                    continue;
                }

                if (mergePloatDependency(dependencies, exportedDependency,
                                         dependencyInfo.intersect(exportedInfo)))
                {
                    // Each PLOAT is inserted once and can only be revisited once if an alternative acyclic path
                    // upgrades its scheduler facet from false to true.
                    pending.emplace_back(exportedDependency);
                }
            }
        }
    };

    populate(reqDeps);
    populate(useReqDeps);
}

string PLOAT::getPrintName() const
{
    return FORMAT("PLOAT {}\n", name);
}

void PLOAT::writeConfigCacheAtConfigTime(string &buffer)
{
    writeNode(buffer, outputFileNode);
    if constexpr (os == OS::NT)
    {
        if (config.linkerFeatures.linker.bTFamily == BTFamily::MSVC &&
            (linkTargetType == TargetType::LIBRARY_SHARED || linkTargetType == TargetType::PLIBRARY_SHARED))
        {
            writeNode(buffer, importLibraryNode);
        }
    }

    STACK_PMR_VECTOR(PLOAT *, sortedReqDeps, 1024)
    sortedReqDeps.reserve(reqDeps.size());
    for (const auto &[dependency, dependencyInfo] : reqDeps)
    {
        sortedReqDeps.emplace_back(dependency);
    }
    std::ranges::sort(sortedReqDeps, std::ranges::less{}, &BTarget::cacheIndex);

    writeUint32(buffer, sortedReqDeps.size());
    for (PLOAT *dependency : sortedReqDeps)
    {
        writeUint32(buffer, packPloatDependency(dependency, reqDeps.find(dependency)->second));
    }
}
