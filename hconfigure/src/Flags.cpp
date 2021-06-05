
#include "Flags.hpp"

Flags flags;
Flags &Flags::operator[](CompilerFamily compilerFamily) {
  if (compileHelper || linkHelper || configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  compileHelper = true;
  compilerCurrent = compilerFamily;
  return *this;
}
Flags &Flags::operator[](LinkerFamily linkerFamily) {
  if (compileHelper || linkHelper || configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  linkHelper = true;
  linkerCurrent = linkerFamily;
  return *this;
}
Flags &Flags::operator[](ConfigType configType) {
  if (!compileHelper && !linkHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class. First use operator[] with COMPILER_FAMILY or LINKER_FAMILY");
  }
  configHelper = true;
  configCurrent = configType;
  return *this;
}

void Flags::operator=(const std::string &flags) {
  if ((!compileHelper && !linkHelper) && !configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  configHelper = false;
  if (compileHelper) {
    auto t = std::make_tuple(compilerCurrent, configCurrent);
    if (auto [pos, ok] = compilerFlags.emplace(t, flags); !ok) {
      std::cout << "Rewriting the flags in compilerFlags for this configuration";
      compilerFlags[t] = flags;
    }
    compileHelper = false;
  } else {
    auto t = std::make_tuple(linkerCurrent, configCurrent);
    if (auto [pos, ok] = linkerFlags.emplace(t, flags); !ok) {
      std::cout << "Rewriting the flags in linkerFlags for this configuration";
      linkerFlags[t] = flags;
    }
    linkHelper = false;
  }
}

Flags::operator std::string() {
  if ((!compileHelper && !linkHelper) && !configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  configHelper = false;
  if (compileHelper) {
    auto t = std::make_tuple(compilerCurrent, configCurrent);
    if (compilerFlags.find(t) == compilerFlags.end() || compilerFlags[t].empty()) {
      std::cout << "No Compiler Flags Defined For This Configuration." << std::endl;
    }
    compileHelper = false;
    return compilerFlags[t];
  } else {
    auto t = std::make_tuple(linkerCurrent, configCurrent);
    if (linkerFlags.find(t) == linkerFlags.end() || linkerFlags[t].empty()) {
      std::cout << "No Linker Flags Defined For This Configuration." << std::endl;
    }
    linkHelper = false;
    return linkerFlags[t];
  }
}