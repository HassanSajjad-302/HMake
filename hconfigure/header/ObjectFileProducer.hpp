
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP

#include "BasicTargets.hpp"
#include "DS.hpp"
struct ObjectFile : public BTarget
{
    virtual string getObjectFileOutputFilePath() = 0;
    virtual string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) = 0;
};

struct ObjectFileProducer
{
    virtual void getObjectFiles(vector<ObjectFile *> *objectFiles,
                                class LinkOrArchiveTarget *linkOrArchiveTarget) const = 0;
};

template <typename T> struct ObjectFileProducerWithDS : public ObjectFileProducer, public DS<T>
{
    set<const ObjectFileProducer *> requirementObjectFileTargets;
    set<const ObjectFileProducer *> usageRequirementObjectFileTargets;
};

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
