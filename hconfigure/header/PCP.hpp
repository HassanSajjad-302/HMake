
#ifndef HMAKE_HCONFIGURE_HEADER_PCP_HPP
#define HMAKE_HCONFIGURE_HEADER_PCP_HPP
#include "Library.hpp"
#include "Platform.hpp"
#include "map"
//Package Copy Path
class PCP {
  std::map<std::tuple<Platform, LibraryType>, fs::path> packageCopyPaths;

  //bool and enum helpers for using Platform class with some operator overload magic
  bool platformHelper = false;
  bool libraryTypeHelper = false;

  Platform platformCurrent;
  LibraryType libraryTypeCurrent;

public:
  PCP &operator[](Platform platform);
  PCP &operator[](LibraryType libraryType);

  void operator=(const fs::path &copyPathRelativeToPackageVariantInstallDirectory);

  operator fs::path();
};

#endif//HMAKE_HCONFIGURE_HEADER_PCP_HPP
