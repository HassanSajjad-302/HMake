#include "TargetType.hpp"
#include "JConsts.hpp"
#include "fmt/format.h"

using fmt::print;

void to_json(Json &j, const TargetType &bTargetType)
{
    switch (bTargetType)
    {

    case TargetType::EXECUTABLE:
        j = JConsts::executable;
        break;
    case TargetType::LIBRARY_STATIC:
        j = JConsts::static_;
        break;
    case TargetType::LIBRARY_SHARED:
        j = JConsts::shared;
        break;
    case TargetType::COMPILE:
        j = JConsts::compile;
        break;
    case TargetType::PREPROCESS:
        j = JConsts::preprocess;
        break;
    case TargetType::RUN:
        j = JConsts::run;
        break;
    case TargetType::PLIBRARY_STATIC:
        j = JConsts::plibraryStatic;
        break;
    case TargetType::PLIBRARY_SHARED:
        j = JConsts::plibraryShared;
        break;
    }
}

void from_json(const Json &j, TargetType &bTargetType)
{
    if (j == JConsts::executable)
    {
        bTargetType = TargetType::EXECUTABLE;
    }
    else if (j == JConsts::static_)
    {
        bTargetType = TargetType::LIBRARY_STATIC;
    }
    else if (j == JConsts::shared)
    {
        bTargetType = TargetType::LIBRARY_SHARED;
    }
    else if (j == JConsts::compile)
    {
        bTargetType = TargetType::COMPILE;
    }
    else if (j == JConsts::preprocess)
    {
        bTargetType = TargetType::PREPROCESS;
    }
    else if (j == JConsts::run)
    {
        bTargetType = TargetType::RUN;
    }
    else if (j == JConsts::plibraryStatic)
    {
        bTargetType = TargetType::PLIBRARY_STATIC;
    }
    else
    {
        bTargetType = TargetType::PLIBRARY_SHARED;
    }
}