
#ifndef HMAKE_CONFIGTYPE_HPP
#define HMAKE_CONFIGTYPE_HPP
#ifdef USE_HEADER_UNITS
#include "nlohmann/json.hpp";
#else
#include "nlohmann/json.hpp"
#endif

using Json = nlohmann::json;

enum class ConfigType : char
{
    DEBUG,
    RELEASE,
    PROFILE,
};
void to_json(Json &json, const ConfigType &configType);
void from_json(const Json &json, ConfigType &configType);

#endif // HMAKE_CONFIGTYPE_HPP

