

#ifndef HMAKE_CUSTOMFUNCTIONS_HPP
#define HMAKE_CUSTOMFUNCTIONS_HPP

#include "nlohmann/json.hpp"
typedef nlohmann::ordered_json Json;

void jsonAssignSpecialist(const std::string &jstr, Json &j, auto &container) {
  if (container.empty()) {
    return;
  }
  if (container.size() == 1) {
    j[jstr] = *container.begin();
    return;
  }
  j[jstr] = container;
}
#endif//HMAKE_CUSTOMFUNCTIONS_HPP
