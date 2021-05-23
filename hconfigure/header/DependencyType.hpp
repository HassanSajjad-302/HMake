
#ifndef HMAKE_DEPENDENCYTYPE_HPP
#define HMAKE_DEPENDENCYTYPE_HPP

#include "nlohmann/json.hpp"
typedef nlohmann::ordered_json Json;
enum class DependencyType {
  PUBLIC,
  PRIVATE
};

void to_json(Json &j, const DependencyType &p);
#endif//HMAKE_DEPENDENCYTYPE_HPP
