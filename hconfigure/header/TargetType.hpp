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
    PREPROCESS,
    LIBRARY_OBJECT,
    RUN,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED,
    PREBUILT_BASIC,
};
void to_json(Json &j, const TargetType &targetType);
void from_json(const Json &j, TargetType &targetType);

#endif // HMAKE_TARGETTYPE_HPP
