

#include "Family.hpp"

void to_json(Json &j, const Compiler &p) {
  if (p.compilerFamily == CompilerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (p.compilerFamily == CompilerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = p.path.string();
}

void to_json(Json &j, const Linker &p) {
  if (p.linkerFamily == LinkerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (p.linkerFamily == LinkerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = p.path.string();
}