
#ifndef HMAKE_HCONFIGURE_HEADER_PACKAGE_HPP
#define HMAKE_HCONFIGURE_HEADER_PACKAGE_HPP

#include ""
#include "Project.hpp"
#include "set"

struct Package {
  std::set<decltype(Json::object())> packageVariances;
};

#include "CONFIG_TYPE.hpp"
#include "EnumIterator.hpp"
#include "Library.hpp"
void user() {
  Package aPackage;
  using LibraryTypeIterator = Iterator<LibraryType, LibraryType::STATIC, LibraryType::SHARED>;
  Json j;
  for (auto i : LibraryTypeIterator()) {
    j["CONFIGURATION"] = i;
    aPackage.packageVariances.emplace(j);
  }
}
#endif//HMAKE_HCONFIGURE_HEADER_PACKAGE_HPP
