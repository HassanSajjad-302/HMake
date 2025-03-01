
#ifndef HMAKE_CONFIGTYPE_HPP
#define HMAKE_CONFIGTYPE_HPP
#ifdef USE_HEADER_UNITS
#include "nlohmann/json.hpp";
#else
#include "nlohmann/json.hpp"
#endif

using Json = nlohmann::json;

#endif // HMAKE_CONFIGTYPE_HPP

