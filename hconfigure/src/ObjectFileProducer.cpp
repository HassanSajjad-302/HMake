
#ifdef USE_HEADER_UNITS
import "ObjectFileProducer.hpp";
import "LinkOrArchiveTarget.hpp";
#else
#include "ObjectFileProducer.hpp"
#include "LinkOrArchiveTarget.hpp"
#endif

ObjectFileProducer::ObjectFileProducer()
{
    realBTargets.emplace_back(this, 0);
}

void ObjectFileProducer::getObjectFiles(vector<const ObjectFile *> *, class LinkOrArchiveTarget *) const
{
}

void ObjectFileProducer::addDependencyOnObjectFileProducers(PrebuiltBasic *prebuiltBasic)
{
    prebuiltBasic->realBTargets[0].addDependency(*this);
}