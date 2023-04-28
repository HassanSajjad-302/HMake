// Copied from https://github.com/microsoft/STL/blob/0a2d59ed7bfe099199237090998b758c9b0b7403/stl/modules/std.ixx
// Besides iostream, other headers removed for faster example execution

// In a module-file, the optional `module;` must appear first; see [cpp.pre].
module;

// This named module expects to be built with classic headers, not header units.
#define _BUILD_STD_MODULE

export module std;

#pragma warning(push)
#pragma warning(disable : 5244) // '#include <meow>' in the purview of module 'std' appears erroneous.
#include <iostream>
#pragma warning(pop)