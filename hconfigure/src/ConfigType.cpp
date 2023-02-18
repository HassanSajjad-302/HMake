#include "ConfigType.hpp"
#include "JConsts.hpp"

void to_json(Json &json, const ConfigType &configType)
{
    if (configType == ConfigType::DEBUG)
    {
        json = JConsts::debug;
    }
    else
    {
        json = JConsts::release;
    }
}

void from_json(const Json &json, ConfigType &configType)
{
    if (json == JConsts::debug)
    {
        configType = ConfigType::DEBUG;
    }
    else if (json == JConsts::release)
    {
        configType = ConfigType::RELEASE;
    }
}
