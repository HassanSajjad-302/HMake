#ifdef USE_HEADER_UNITS
import "InclNodeTargetMap.hpp";
#else
#include "InclNodeTargetMap.hpp"
#endif


InclNodeTargetMap::InclNodeTargetMap(InclNode inclNode_, CppSourceTarget *cppSourceTarget_)
    : inclNode(inclNode_), cppSourceTarget(cppSourceTarget_)
{
}

InclNodePointerTargetMap::InclNodePointerTargetMap(const InclNode *inclNode_, CppSourceTarget *cppSourceTarget_)
    : inclNode(inclNode_), cppSourceTarget(cppSourceTarget_)
{
}
