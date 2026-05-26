#include "PLOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CppMod.hpp"
#include "JConsts.hpp"
#include "LOAT.hpp"
#include "ObjectFileProducer.hpp"
#include <utility>

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
    const uint32_t size = readUint32(ptr, configCacheBytesRead);
    useReqLibraryDirs.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
    {
        Node *node = readHalfNode(ptr, configCacheBytesRead);
        useReqLibraryDirs.emplace_back(node);
    }
}

#else

PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_)
    : BTarget(outputName_, getLaunchesProcessPloat(linkTargetType_), getBTargetTypePloat(linkTargetType_), false,
              false),
      config(config_), outputName{getLastNameAfterSlash(outputName_)}, linkTargetType{linkTargetType_},
      outputDirectory(myBuildDir_)
{
    if (linkTargetType == TargetType::PLIBRARY_STATIC || linkTargetType == TargetType::PLIBRARY_SHARED)
    {
        if (outputDirectory)
        {
            return;
        }
        printErrorMessage(FORMAT("Empty build-dir provided for Prebuilt Library {}\n", name));
    }
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_,
             string name_, bool buildExplicit, bool makeDirectory)
    : BTarget(name_, getLaunchesProcessPloat(linkTargetType_), getBTargetTypePloat(linkTargetType_), buildExplicit,
              makeDirectory),
      config(config_), outputName(outputName_), linkTargetType(linkTargetType_), outputDirectory(myBuildDir_)

{
    if (linkTargetType == TargetType::PLIBRARY_STATIC || linkTargetType == TargetType::PLIBRARY_SHARED)
    {
        if (outputDirectory)
        {
            return;
        }
        printErrorMessage(FORMAT("Empty build-dir provided for Prebuilt Library {}\n", name));
    }
}

#endif

void PLOAT::completeRoundOne()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        readCacheAtBuildTime();
        outputFileNode->doStatFile = true;
    }
    else
    {

#ifndef BUILD_MODE
        actualOutputName = getActualNameFromTargetName(linkTargetType, os, outputName);

        // In case of prebuilt library not having a valid outputDirectory at constructor time is an error. While in
        // case of LOAT, if no other outputDirectory is assigned, then we use the LOAT::myBuildDir as
        // outputDirectory.
        if (!outputDirectory)
        {
            outputDirectory = static_cast<LOAT *>(this)->myBuildDir;
        }
        outputFileNode = Node::getNode(outputDirectory->filePath + slashc + actualOutputName, true, true);

#endif

        populateReqAndUseReqDeps();
    }

    for (const uint32_t index : reqDepsVecIndices)
    {
        PLOAT *reqDep = static_cast<PLOAT *>(bTargetCaches[index].bTarget);
        if constexpr (bsMode == BSMode::BUILD)
        {
            if (evaluate(TargetType::LIBRARY_STATIC))
            {
                if (reqDep->hasObjectFiles)
                {
                    realBTargets[0].addDep<BTargetType::LOAT, RelationType::LOOSE>(&reqDep->realBTargets[0]);
                }
            }
            else
            {
                if (reqDep->hasObjectFiles)
                {
                    realBTargets[0].addDep<BTargetType::LOAT>(&reqDep->realBTargets[0]);
                }
            }
        }

        for (const LibDirNode &libDirNode : reqDep->useReqLibraryDirs)
        {
            reqLibraryDirs.emplace_back(libDirNode.node);
        }
    }

    if (!hasObjectFiles)
    {
        return;
    }

    for (ObjectFileProducer *objectFileProducer : objectFileProducers)
    {
        if (objectFileProducer->hasObjectFiles)
        {
            realBTargets[0].addDep<BTargetType::UNKNOWN>(&objectFileProducer->realBTargets[0]);
        }
    }
}

void PLOAT::readCacheAtBuildTime()
{
    const string_view configCache = bTargetCaches[cacheIndex].configCache;
    const char *ptr = configCache.data();

    const uint32_t reqVecSize = readUint32(ptr, configCacheBytesRead);
    for (uint32_t i = 0; i < reqVecSize; ++i)
    {
        reqDepsVecIndices.emplace_back(readUint32(ptr, configCacheBytesRead));
    }
    uint32_t size = readUint32(ptr, configCacheBytesRead);
    reqLibraryDirs.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
    {
        Node *node = readHalfNode(ptr, configCacheBytesRead);
        reqLibraryDirs.emplace_back(node);
    }
}

void PLOAT::populateReqAndUseReqDeps()
{

    // Set is copied because new elements are to be inserted in it.

    for (auto localReqDeps = reqDeps; PLOAT *t : localReqDeps)
    {
        for (PLOAT *t_ : t->useReqDeps)
        {
            reqDeps.emplace(t_);
        }
    }

    for (auto localUseReqDeps = useReqDeps; PLOAT *t : localUseReqDeps)
    {
        for (PLOAT *t_ : t->useReqDeps)
        {
            useReqDeps.emplace(t_);
        }
    }
}

string PLOAT::getPrintName() const
{
    return FORMAT("PLOAT {}\n", name);
}

void PLOAT::writeConfigCacheAtConfigTime(string &buffer)
{
    writeNode(buffer, outputFileNode);

    writeUint32(buffer, useReqLibraryDirs.size());
    for (const LibDirNode &libDirNode : useReqLibraryDirs)
    {
        writeNode(buffer, libDirNode.node);
    }

    writeUint32(buffer, reqDeps.size());
    for (const PLOAT *ploat : reqDeps)
    {
        writeUint32(buffer, ploat->cacheIndex);
    }

    writeUint32(buffer, reqLibraryDirs.size());
    for (const LibDirNode &libDirNode : reqLibraryDirs)
    {
        writeNode(buffer, libDirNode.node);
    }
}

bool operator<(const PLOAT &lhs, const PLOAT &rhs)
{
    return lhs.id < rhs.id;
}

void to_json(Json &json, const PLOAT &PLOAT)
{
    json = PLOAT.getPrintName();
}
