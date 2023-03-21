
#ifdef USE_HEADER_UNITS
import "ObjectFileProducer.hpp";
#else
#include "ObjectFileProducer.hpp"
#endif

void ObjectFileProducer::getObjectFiles(set<ObjectFile *> *objectFiles,
                                        class LinkOrArchiveTarget *linkOrArchiveTarget) const
{
}