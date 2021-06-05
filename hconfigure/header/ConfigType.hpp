
#ifndef HMAKE_CONFIG_TYPE_HPP
#define HMAKE_CONFIG_TYPE_HPP
#include "EnumIterator.hpp"
enum class ConfigType {
  DEBUG,
  RELEASE
};
using ConfigTypeIterator = Iterator<ConfigType, ConfigType::DEBUG, ConfigType::RELEASE>;
#endif//HMAKE_CONFIG_TYPE_HPP
