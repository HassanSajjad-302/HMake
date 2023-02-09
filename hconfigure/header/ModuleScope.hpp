
#ifndef HMAKE_MODULESCOPE_HPP
#define HMAKE_MODULESCOPE_HPP

#include "BasicTargets.hpp"
struct ModuleScope : CTarget
{
    ModuleScope(string name_, CTarget &container, bool hasFile_ = true);
    explicit ModuleScope(string name_);
};

#endif // HMAKE_MODULESCOPE_HPP
