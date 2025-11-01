
#ifndef OBJECTFILE_HPP
#define OBJECTFILE_HPP

#include "BTarget.hpp"
#include "Node.hpp"

class ObjectFile : public BTarget
{
  public:
    ObjectFile() : BTarget("", true, false)
    {
    }

    ObjectFile(const bool add0, const bool add1) : BTarget("", true, false, add0, add1)
    {
    }
    Node *objectNode = nullptr;
};

#endif // OBJECTFILE_HPP
