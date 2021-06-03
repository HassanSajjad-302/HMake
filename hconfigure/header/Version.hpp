
#ifndef HMAKE_HCONFIGURE_HEADER_VERSION_HPP
#define HMAKE_HCONFIGURE_HEADER_VERSION_HPP

#include "nlohmann/json.hpp"
using Json = nlohmann::ordered_json;
struct Version {
  int majorVersion{};
  int minorVersion{};
  int patchVersion{};
};

void to_json(Json &j, const Version &p);

#endif//HMAKE_HCONFIGURE_HEADER_VERSION_HPP
