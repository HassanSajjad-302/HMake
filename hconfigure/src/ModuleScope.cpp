#include "ModuleScope.hpp"

ModuleScope::ModuleScope(string name_, CTarget &container, const bool hasFile_)
    : CTarget{std::move(name_), container, hasFile_}
{
}

ModuleScope::ModuleScope(string name_) : CTarget(std::move(name_))
{
}