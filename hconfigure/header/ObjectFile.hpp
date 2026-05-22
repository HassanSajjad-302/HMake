
#ifndef OBJECTFILE_HPP
#define OBJECTFILE_HPP

#include "BTarget.hpp"
#include "Node.hpp"

class ObjectFile : public BTarget
{
  public:
    explicit ObjectFile(const uint64_t cacheName_, const BTargetType targetType)
        : BTarget("", cacheName_, targetType, true, false)
    {
    }

    ObjectFile(const uint64_t cacheName_, const BTargetType targetType, const bool add0, const bool add1)
        : BTarget("", cacheName_, true, targetType, true, false, add0, add1)
    {
    }
    Node *objectNode = nullptr;
};

#endif // OBJECTFILE_HPP
