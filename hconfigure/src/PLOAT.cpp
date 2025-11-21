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

#ifdef BUILD_MODE
PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(name), config(config_), linkTargetType{linkTargetType_}
{
    initializePLOAT();
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_,
             string name_, bool buildExplicit, bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name), config(config_), linkTargetType(linkTargetType_)

{
    initializePLOAT();
}

void PLOAT::initializePLOAT()
{
    const char *ptr = fileTargetCaches[cacheIndex].configCache.data();
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
    : BTarget(outputName_, false, false), TargetCache(name), config(config_),
      outputName{getLastNameAfterSlash(outputName_)}, linkTargetType{linkTargetType_}, outputDirectory(myBuildDir_)
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
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name), config(config_), outputName(outputName_),
      linkTargetType(linkTargetType_), outputDirectory(myBuildDir_)

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

void PLOAT::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (round == 1)
    {
        if constexpr (bsMode == BSMode::BUILD)
        {
            readCacheAtBuildTime();
            outputFileNode->toBeChecked = true;
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
            writeTargetConfigCacheAtConfigureTime();
        }

        for (const uint32_t index : reqDepsVecIndices)
        {
            PLOAT *reqDep = static_cast<PLOAT *>(fileTargetCaches[index].targetCache);
            if constexpr (bsMode == BSMode::BUILD)
            {
                if (evaluate(TargetType::LIBRARY_STATIC))
                {
                    if (reqDep->hasObjectFiles)
                    {
                        addDepMT<0, BTargetDepType::LOOSE>(*reqDep);
                    }
                }
                else
                {
                    if (reqDep->hasObjectFiles)
                    {
                        addDepMT<0>(*reqDep);
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
                addDepMT<0>(*objectFileProducer);
            }
        }
    }
}

void PLOAT::writeTargetConfigCacheAtConfigureTime()
{
    writeNode(configCacheBuffer, outputFileNode);

    writeUint32(configCacheBuffer, useReqLibraryDirs.size());
    for (const LibDirNode &libDirNode : useReqLibraryDirs)
    {
        writeNode(configCacheBuffer, libDirNode.node);
    }

    writeUint32(configCacheBuffer, reqDeps.size());
    for (const PLOAT *ploat : reqDeps)
    {
        writeUint32(configCacheBuffer, ploat->cacheIndex);
    }

    writeUint32(configCacheBuffer, reqLibraryDirs.size());
    for (const LibDirNode &libDirNode : reqLibraryDirs)
    {
        writeNode(configCacheBuffer, libDirNode.node);
    }

    fileTargetCaches[cacheIndex].configCache = string_view(configCacheBuffer.data(), configCacheBuffer.size());
}

void PLOAT::readCacheAtBuildTime()
{
    const string_view configCache = fileTargetCaches[cacheIndex].configCache;
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

    for (auto localReqDeps = reqDeps; PLOAT * t : localReqDeps)
    {
        for (PLOAT *t_ : t->useReqDeps)
        {
            reqDeps.emplace(t_);
        }
    }

    for (auto localUseReqDeps = useReqDeps; PLOAT * t : localUseReqDeps)
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

bool operator<(const PLOAT &lhs, const PLOAT &rhs)
{
    return lhs.id < rhs.id;
}

void to_json(Json &json, const PLOAT &PLOAT)
{
    json = PLOAT.getPrintName();
}
