
#include "Settings.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"

void to_json(Json &json, const Settings &settings_)
{
    json[JConsts::maximumBuildThreads] = settings_.maximumBuildThreads;
}

void from_json(const Json &json, Settings &settings_)
{
    settings_.maximumBuildThreads = json.at(JConsts::maximumBuildThreads).get<unsigned int>();
}