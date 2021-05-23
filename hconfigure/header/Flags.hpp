

#ifndef HMAKE_FLAGS_HPP
#define HMAKE_FLAGS_HPP

#include "CONFIG_TYPE.hpp"
#include "Family.hpp"
#include <iostream>

//TODO: Improve the console message and documentation.
struct Flags {

  std::map<std::tuple<CompilerFamily, CONFIG_TYPE>, std::string> compilerFlags;
  std::map<std::tuple<LinkerFamily, CONFIG_TYPE>, std::string> linkerFlags;

  //bool and enum helpers for using Flags class with some operator overload magic
  bool compileHelper = false;
  bool linkHelper = false;
  bool configHelper = false;

  CompilerFamily compilerCurrent;
  LinkerFamily linkerCurrent;
  CONFIG_TYPE configCurrent;

public:
  Flags &operator[](CompilerFamily compilerFamily);
  Flags &operator[](LinkerFamily linkerFamily);
  Flags &operator[](CONFIG_TYPE configType);

  void operator=(const std::string& flags);

  operator std::string();
};
#endif//HMAKE_FLAGS_HPP
