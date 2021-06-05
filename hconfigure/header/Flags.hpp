

#ifndef HMAKE_FLAGS_HPP
#define HMAKE_FLAGS_HPP

#include "ConfigType.hpp"
#include "Family.hpp"
#include <iostream>

//TODO: Improve the console message and documentation.
struct Flags {

  std::map<std::tuple<CompilerFamily, ConfigType>, std::string> compilerFlags;
  std::map<std::tuple<LinkerFamily, ConfigType>, std::string> linkerFlags;

  //bool and enum helpers for using Flags class with some operator overload magic
  bool compileHelper = false;
  bool linkHelper = false;
  bool configHelper = false;

  CompilerFamily compilerCurrent;
  LinkerFamily linkerCurrent;
  ConfigType configCurrent;

public:
  Flags &operator[](CompilerFamily compilerFamily);
  Flags &operator[](LinkerFamily linkerFamily);
  Flags &operator[](ConfigType configType);

  void operator=(const std::string& flags);

  operator std::string();
};
#endif//HMAKE_FLAGS_HPP
