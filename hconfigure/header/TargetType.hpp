#ifndef HMAKE_TARGETTYPE_HPP
#define HMAKE_TARGETTYPE_HPP

#ifdef USE_HEADER_UNITS
#include "nlohmann/json.hpp";
import <string>;
#else
#include "nlohmann/json.hpp"
#include <string>
#endif

using Json = nlohmann::ordered_json;
using std::string;

enum class TargetType
{
    EXECUTABLE,
    LIBRARY_STATIC,
    LIBRARY_SHARED,
    PREPROCESS,
    LIBRARY_OBJECT,
    RUN,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED,
};
void to_json(Json &j, const TargetType &targetType);
void from_json(const Json &j, TargetType &targetType);

#endif // HMAKE_TARGETTYPE_HPP
