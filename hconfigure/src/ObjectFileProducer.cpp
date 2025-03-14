
#ifdef USE_HEADER_UNITS
import "ObjectFileProducer.hpp";
import "LOAT.hpp";
#else
#include "ObjectFileProducer.hpp"

#include <utility>
#include "LOAT.hpp"
#endif

ObjectFileProducer::ObjectFileProducer()
{
}

ObjectFileProducer::ObjectFileProducer(string name_, bool buildExplicit, bool makeDirectory) :BTarget(std::move(name_), buildExplicit, makeDirectory)
{
}

void ObjectFileProducer::getObjectFiles(vector<const ObjectFile *> *, LOAT *) const
{
}