
#ifdef USE_HEADER_UNITS
import "ObjectFileProducer.hpp";
import "LinkOrArchiveTarget.hpp";
#else
#include "ObjectFileProducer.hpp"

#include <utility>
#include "LinkOrArchiveTarget.hpp"
#endif

ObjectFileProducer::ObjectFileProducer()
{
}

ObjectFileProducer::ObjectFileProducer(pstring name_, bool buildExplicit, bool makeDirectory) :BTarget(std::move(name_), buildExplicit, makeDirectory)
{
}

void ObjectFileProducer::getObjectFiles(vector<const ObjectFile *> *, LinkOrArchiveTarget *) const
{
}