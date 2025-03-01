#ifndef HMAKE_TARGETTYPE_HPP
#define HMAKE_TARGETTYPE_HPP

#ifdef USE_HEADER_UNITS
#include "nlohmann/json.hpp";
#else
#include "nlohmann/json.hpp"
#endif

using Json = nlohmann::json;

enum class TargetType : char
{
    EXECUTABLE,
    LIBRARY_STATIC,
    LIBRARY_SHARED,
    LIBRARY_OBJECT,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED,
};

#endif // HMAKE_TARGETTYPE_HPP
