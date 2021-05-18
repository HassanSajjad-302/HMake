
#ifndef HMAKE_DEPENDENCYTYPE_HPP
#define HMAKE_DEPENDENCYTYPE_HPP

#include "nlohmann/json.hpp"
typedef nlohmann::json json;
enum class dependencyType{
    PUBLIC, PRIVATE, INTERFACE
};

void to_json(json &j, const dependencyType &p);
#endif //HMAKE_DEPENDENCYTYPE_HPP
