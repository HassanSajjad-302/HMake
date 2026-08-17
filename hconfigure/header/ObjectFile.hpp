/// \file
/// Defines `ObjectFile`, the base class for compile outputs (.o / .obj) referenced by link targets.

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
    /// Linker inputs emitted by this compile action. Ordinary C/C++ actions contain one object; a single action may
    /// also emit several objects (for example, one ISPC object per selected instruction-set target).
    vector<Node *> objectNodes;
};

#endif // OBJECTFILE_HPP
