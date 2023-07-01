
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP
#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "DS.hpp";
#else
#include "BasicTargets.hpp"
#include "DS.hpp"
#endif

class ObjectFile : public BTarget
{
  public:
    virtual pstring getObjectFileOutputFilePath() const = 0;
    virtual pstring getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const = 0;
};

class ObjectFileProducer : public BTarget
{
  public:
    virtual void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                                class LinkOrArchiveTarget *linkOrArchiveTarget) const;
    void addDependencyOnObjectFileProducers(class PrebuiltBasic *prebuiltBasic);
};

// This is used in conjunction with DSC to specify dependencies.
template <typename T> struct ObjectFileProducerWithDS : public ObjectFileProducer, public DS<T>
{
};

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
