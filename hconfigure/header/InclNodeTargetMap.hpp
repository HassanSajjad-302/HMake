
#ifndef INCLNODETARGETMAP_HPP
#define INCLNODETARGETMAP_HPP

#ifdef USE_HEADER_UNITS
import "SpecialNodes.hpp";
#else
#include "SpecialNodes.hpp"
#endif


struct InclNodeTargetMap
{
    InclNode inclNode;
    class CppSourceTarget *cppSourceTarget;
    InclNodeTargetMap(InclNode inclNode_, CppSourceTarget *cppSourceTarget_);
};

#endif //INCLNODETARGETMAP_HPP
