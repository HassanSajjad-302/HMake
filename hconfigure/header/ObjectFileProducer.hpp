
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
    virtual void getObjectFiles(set<ObjectFile *> *objectFiles, class LinkOrArchiveTarget *linkOrArchiveTarget) const;
};

// This is used in conjunction with DSC to specify dependencies.
template <typename T> struct ObjectFileProducerWithDS : public ObjectFileProducer, public DS<T>
{
    // Following are needed to propagate dependencies above if linkOrArchiveTarget of both DSC(dependency and dependent)
    // is nullptr i.e. if there are 3 targets A,B,C. C being executable and have dependency on B(a library) having a
    // dependency on A(a library). Then object-files of A would be propagated to C through linkOrArchiveTarget of B. But
    // in-case both B and C are object-file-targets, then object-files of C will propagate through object-files of B
    set<const ObjectFileProducer *> requirementObjectFileTargets;
    set<const ObjectFileProducer *> usageRequirementObjectFileProducers;
};

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
