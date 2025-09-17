#ifdef USE_HEADER_UNITS
import "PLOAT.hpp";
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "ObjectFileProducer.hpp";
import "SMFile.hpp";
#else
#include "PLOAT.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "ObjectFileProducer.hpp"
#include "SMFile.hpp"
#include <utility>
#endif
#include "JConsts.hpp"

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
    return outputDirectory;
#endif
}

#ifdef BUILD_MODE
PLOAT::PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(name), config(config_), linkTargetType{linkTargetType_}
{
    outputFileNode = readHalfNode(fileTargetCaches[cacheIndex].configCache.data(), configCacheBytesRead);
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_, string name_,
             bool buildExplicit, bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name), config(config_), linkTargetType(linkTargetType_)

{
    outputFileNode = readHalfNode(fileTargetCaches[cacheIndex].configCache.data(), configCacheBytesRead);
}

#else

PLOAT::PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(name), config(config_),
      outputName{getLastNameAfterSlash(outputName_)}, linkTargetType{linkTargetType_}, outputDirectory(std::move(dir))
{
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_, string name_,
             bool buildExplicit, bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name), config(config_), outputName(outputName_),
      linkTargetType(linkTargetType_), outputDirectory(std::move(dir))

{
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
            Node *outputDirectoryNode = Node::getNodeFromNonNormalizedString(outputDirectory, false, true);
            if (outputDirectoryNode->fileType == file_type::not_found)
            {
                // TODO
                // Throw Exception. Also Replace outputDirectory with outputNode initialized in the constructor.
                // User won't have the ability to setOutputName or setOutputDirectory. Should be achieved in the
                // constructor.

                // throw std::exception(FORMAT("Output dir {} for LinkTarget {} does not exists.",
                // outputDirectoryNode->filePath, name));
            }
            outputDirectory = outputDirectoryNode->filePath;
            outputFileNode = Node::getNodeFromNormalizedString(outputDirectory + slashc + actualOutputName, true, true);

#endif

            writeTargetConfigCacheAtConfigureTime();
        }

        populateReqAndUseReqDeps();
        addReqDepsToBTargetDependencies();
        for (auto &[PLOAT, prebuiltDep] : reqDeps)
        {
            for (const LibDirNode &libDirNode : PLOAT->useReqLibraryDirs)
            {
                reqLibraryDirs.emplace_back(libDirNode.node, libDirNode.isStandard);
            }
        }

        for (auto &[ploat, prebuiltDep] : reqDeps)
        {
            for (const LibDirNode &libDirNode : ploat->useReqLibraryDirs)
            {
                reqLibraryDirs.emplace_back(libDirNode.node, libDirNode.isStandard);
            }
        }

        for (ObjectFileProducer *objectFileProducer : objectFileProducers)
        {
            addDepHalfNowHalfLater(*objectFileProducer);
        }
    }
}

void PLOAT::writeTargetConfigCacheAtConfigureTime()
{
    writeNode(configCacheBuffer, outputFileNode);

    writeUint32(configCacheBuffer, reqLibraryDirs.size());
    for (const LibDirNode &libDirNode : reqLibraryDirs)
    {
        writeNode(configCacheBuffer, libDirNode.node);
    }

    writeUint32(configCacheBuffer, useReqLibraryDirs.size());
    for (const LibDirNode &libDirNode : useReqLibraryDirs)
    {
        writeNode(configCacheBuffer, libDirNode.node);
    }

    fileTargetCaches[cacheIndex].configCache = string_view(configCacheBuffer.data(), configCacheBuffer.size());
}

void PLOAT::readCacheAtBuildTime()
{
    const string_view configCache = fileTargetCaches[cacheIndex].configCache;
    uint32_t size = readUint32(configCache.data(), configCacheBytesRead);
    reqLibraryDirs.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
    {
        Node *node = readHalfNode(configCache.data(), configCacheBytesRead);
        reqLibraryDirs.emplace_back(node, true);
    }

    size = readUint32(configCache.data(), configCacheBytesRead);
    useReqLibraryDirs.reserve(size);
    for (uint32_t i = 0; i < size; ++i)
    {
        Node *node = readHalfNode(configCache.data(), configCacheBytesRead);
        useReqLibraryDirs.emplace_back(node, true);
    }
}

void PLOAT::populateReqAndUseReqDeps()
{
    // Set is copied because new elements are to be inserted in it.
    node_hash_map<PLOAT *, PrebuiltDep> localReqDeps = reqDeps;

    for (auto &[PLOAT, prebuiltDep] : localReqDeps)
    {
        for (auto &[PLOAT_, prebuilt] : PLOAT->useReqDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.reqPreLF = prebuilt.useReqPreLF;
            prebuiltDep_.reqPostLF = prebuilt.useReqPostLF;
            prebuiltDep_.reqRpathLink = prebuilt.useReqRpathLink;
            prebuiltDep_.reqRpath = prebuilt.useReqRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            reqDeps.emplace(PLOAT_, std::move(prebuiltDep_));
        }
    }

    for (auto localUseReqs = useReqDeps; auto &[PLOAT, prebuiltDep] : localUseReqs)
    {
        for (auto &[PLOAT_, prebuilt] : PLOAT->useReqDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.useReqPreLF = prebuilt.useReqPreLF;
            prebuiltDep_.useReqPostLF = prebuilt.useReqPostLF;
            prebuiltDep_.useReqRpathLink = prebuilt.useReqRpathLink;
            prebuiltDep_.useReqRpath = prebuilt.useReqRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            useReqDeps.emplace(PLOAT_, std::move(prebuiltDep_));
        }
    }
}

void PLOAT::addReqDepsToBTargetDependencies()
{
    if (evaluate(TargetType::LIBRARY_STATIC))
    {
        for (auto &[PLOAT, prebuiltDep] : reqDeps)
        {
            addDepLooseHalfNowHalfLater(*PLOAT);
        }
    }
    else
    {
        for (auto &[PLOAT, prebuiltDep] : reqDeps)
        {
            addDepHalfNowHalfLater(*PLOAT);
        }
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
