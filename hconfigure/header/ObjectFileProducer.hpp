
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP

#include "BuildSystemFunctions.hpp"
#include "ObjectFile.hpp"

class LOAT;

class ObjectFileProducer : public BTarget
{
  public:
    bool hasObjectFiles = true;

    ObjectFileProducer(string name_, const BTargetType bTargetType, const bool buildExplicit, const bool makeDirectory)
        : BTarget(std::move(name_), false, bTargetType, buildExplicit, makeDirectory)
    {
    }
    virtual void getObjectFiles(vector<const ObjectFile *> *objectFiles) const
    {
    }
};

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
