
#ifndef HMAKE_CONFIGTYPE_HPP
#define HMAKE_CONFIGTYPE_HPP
#include "nlohmann/json.hpp"

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
