#ifndef HMAKE_TARGETTYPE_HPP
#define HMAKE_TARGETTYPE_HPP
#include "nlohmann/json.hpp"
#include "string"

using Json = nlohmann::ordered_json;
using std::string;

enum class TargetType
{
    NOT_ASSIGNED,
    EXECUTABLE,
    LIBRARY_STATIC,
    LIBRARY_SHARED,
    VARIANT,
    COMPILE,
    PREPROCESS,
    RUN,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED,
};
void to_json(Json &j, const TargetType &targetType);
void from_json(const Json &j, TargetType &targetType);

#endif // HMAKE_TARGETTYPE_HPP
