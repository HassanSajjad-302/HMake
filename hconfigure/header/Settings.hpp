#ifndef HMAKE_SETTINGS_HPP
#define HMAKE_SETTINGS_HPP

#include "nlohmann/json.hpp"
#include <string>
#include <thread>

using std::string, std::string_view;

using Json = nlohmann::json;

struct Settings
{
    unsigned int maximumBuildThreads = std::thread::hardware_concurrency();
};

void to_json(Json &json, const Settings &settings_);
void from_json(const Json &json, Settings &settings_);
inline Settings settings;
#endif // HMAKE_SETTINGS_HPP
