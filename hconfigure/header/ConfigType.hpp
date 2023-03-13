
#ifndef HMAKE_CONFIGTYPE_HPP
#define HMAKE_CONFIGTYPE_HPP
#ifdef USE_HEADER_UNITS
#include "nlohmann/json.hpp"
#else
#include "nlohmann/json.hpp"
#endif

using Json = nlohmann::ordered_json;
enum class Dependency
{
    PUBLIC,
    PRIVATE,
    INTERFACE
};

enum class ConfigType
{
    DEBUG,
    RELEASE,
    PROFILE,
};
void to_json(Json &json, const ConfigType &configType);
void from_json(const Json &json, ConfigType &configType);

#endif // HMAKE_CONFIGTYPE_HPP
