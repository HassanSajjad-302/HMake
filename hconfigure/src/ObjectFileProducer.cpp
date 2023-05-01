
#ifdef USE_HEADER_UNITS
import "ObjectFileProducer.hpp";
import "LinkOrArchiveTarget.hpp";
#else
#include "ObjectFileProducer.hpp"
#include "LinkOrArchiveTarget.hpp"
#endif

void ObjectFileProducer::getObjectFiles(vector<const ObjectFile *> *, class LinkOrArchiveTarget *) const
{
}

void ObjectFileProducer::addDependencyOnObjectFileProducers(LinkOrArchiveTarget *linkOrArchiveTarget)
{
    linkOrArchiveTarget->getRealBTarget(0).addDependency(*this);
    for (const ObjectFileProducer *objectFileTarget : requirementObjectFileTargets)
    {
        linkOrArchiveTarget->getRealBTarget(0).addDependency(const_cast<ObjectFileProducer &>(*objectFileTarget));
    }
}