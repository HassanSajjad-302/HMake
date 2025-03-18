#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "PrebuiltLinkOrOrArchiveTarget.hpp";
import "SMFile.hpp";
#else
#include "PLOAT.hpp"

#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "SMFile.hpp"
#include <utility>
#endif

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
    : BTarget(outputName_, false, false), TargetCache(outputName_), config(config_), linkTargetType{linkTargetType_}
{
    outputFileNode =
        Node::getHalfNodeFromValue(getConfigCache()[Indices::ConfigCache::LinkConfig::outputFileNode]);
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_, string name_, bool buildExplicit, bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name_), config(config_), linkTargetType(linkTargetType_)

{
    outputFileNode =
        Node::getHalfNodeFromValue(getConfigCache()[Indices::ConfigCache::LinkConfig::outputFileNode]);
}

#else

PLOAT::PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(outputName_), config(config_),
      outputName{getLastNameAfterSlash(outputName_)}, linkTargetType{linkTargetType_},
      outputDirectory(std::move(dir))
{
}

PLOAT::PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_, string name_, bool buildExplicit, bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name_), config(config_), outputName(outputName_),
      linkTargetType(linkTargetType_), outputDirectory(std::move(dir))

{
}

#endif

void PLOAT::updateBTarget(Builder &builder, unsigned short round)
{
    if (round == 1)
    {
        for (ObjectFileProducer *objectFileProducer : objectFileProducers)
        {
            addDependency<0>(*objectFileProducer);
        }
    }
    else if (round == 2)
    {

        if constexpr (bsMode == BSMode::BUILD)
        {
            readConfigCacheAtBuildTime();
        }
        else
        {

#ifndef BUILD_MODE
            actualOutputName = getActualNameFromTargetName(linkTargetType, os, outputName);
            Node *outputDirectoryNode = Node::getNodeFromNonNormalizedString(outputDirectory, false, true);
            if (outputDirectoryNode->doesNotExist)
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
    }
}

void PLOAT::writeTargetConfigCacheAtConfigureTime()
{
    namespace LinkConfig = Indices::ConfigCache::LinkConfig;

    buildOrConfigCacheCopy.PushBack(kArrayType, cacheAlloc);
    Value &libDirsConfigCache = buildOrConfigCacheCopy[LinkConfig::reqLibraryDirsArray];
    libDirsConfigCache.Reserve(reqLibraryDirs.size(), cacheAlloc);

    for (const LibDirNode &libDirNode : reqLibraryDirs)
    {
        libDirsConfigCache.PushBack(libDirNode.node->getValue(), cacheAlloc);
    }

    buildOrConfigCacheCopy.PushBack(kArrayType, cacheAlloc);
    Value &useLibDirsConfigCache = buildOrConfigCacheCopy[LinkConfig::useReqLibraryDirsArray];
    useLibDirsConfigCache.Reserve(useReqLibraryDirs.size(), cacheAlloc);

    for (const LibDirNode &libDirNode : useReqLibraryDirs)
    {
        useLibDirsConfigCache.PushBack(libDirNode.node->getValue(), cacheAlloc);
    }

    buildOrConfigCacheCopy.PushBack(outputFileNode->getValue(), cacheAlloc);
    copyBackConfigCacheMutexLocked();
}

void PLOAT::readConfigCacheAtBuildTime()
{
    namespace LinkConfig = Indices::ConfigCache::LinkConfig;

    Value &reqLibDirsConfigCache = getConfigCache()[LinkConfig::reqLibraryDirsArray];
    reqLibraryDirs.reserve(reqLibDirsConfigCache.Size());
    for (const Value &value : reqLibDirsConfigCache.GetArray())
    {
        reqLibraryDirs.emplace_back(Node::getNodeFromValue(value, false), true);
    }

    Value &useReqLibDirsConfigCache = getConfigCache()[LinkConfig::useReqLibraryDirsArray];
    useReqLibraryDirs.reserve(useReqLibDirsConfigCache.Size());
    for (const Value &value : useReqLibDirsConfigCache.GetArray())
    {
        useReqLibraryDirs.emplace_back(Node::getNodeFromValue(value, false), true);
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
            addLooseDependency<0>(*PLOAT);
        }
    }
    else
    {
        for (auto &[PLOAT, prebuiltDep] : reqDeps)
        {
            addDependency<0>(*PLOAT);
        }
    }
}

bool operator<(const PLOAT &lhs, const PLOAT &rhs)
{
    return lhs.id < rhs.id;
}

void to_json(Json &json, const PLOAT &PLOAT)
{
    json = PLOAT.getTarjanNodeName();
}
