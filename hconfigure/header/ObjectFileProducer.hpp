
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP
#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "DS.hpp";
#else
#include "BasicTargets.hpp"
#include "DS.hpp"
#endif

struct ObjectFile : public BTarget
{
    virtual string getObjectFileOutputFilePath() = 0;
    virtual string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) = 0;
};

struct ObjectFileProducer : public BTarget
{
    virtual void getObjectFiles(set<ObjectFile *> *objectFiles,
                                class LinkOrArchiveTarget *linkOrArchiveTarget) const = 0;
};

template <typename T> struct ObjectFileProducerWithDS : public ObjectFileProducer, public DS<T>
{
    set<const ObjectFileProducer *> requirementObjectFileTargets;
    set<const ObjectFileProducer *> usageRequirementObjectFileProducers;
};

#endif // HMAKE_OBJECTFILEPRODUCER_HPP

