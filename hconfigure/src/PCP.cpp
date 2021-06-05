

#include "PCP.hpp"
#include "Platform.hpp"

PCP &PCP::operator[](Platform platform) {
  if (platformHelper || libraryTypeHelper) {
    throw std::logic_error("Wrong Usage Of PCP class");
  }
  platformHelper = true;
  platformCurrent = platform;
  return *this;
}

PCP &PCP::operator[](LibraryType libraryType) {
  if (!platformHelper) {
    throw std::logic_error("Wrong Usage of PCP class. First use operator(Platform) function");
  }
  libraryTypeHelper = true;
  libraryTypeCurrent = libraryType;
  return *this;
}

void PCP::operator=(const fs::path &copyPathRelativeToPackageVariantInstallDirectory) {
  if (!platformHelper || !libraryTypeHelper) {
    throw std::logic_error("Wrong Usage Of PCP Class.");
  }
  platformHelper = true;
  libraryTypeHelper = true;
  auto t = std::make_tuple(platformCurrent, libraryTypeCurrent);
  if (auto [pos, ok] = packageCopyPaths.emplace(t, copyPathRelativeToPackageVariantInstallDirectory); !ok) {
    std::cout << "ReWriting Install Location for this configuration" << std::endl;
    packageCopyPaths[t] = copyPathRelativeToPackageVariantInstallDirectory;
  }
}

PCP::operator fs::path() {
  if (!platformHelper || !libraryTypeHelper) {
    throw std::logic_error("Wrong Usage Of PCP Class.");
  }
  platformHelper = libraryTypeHelper = false;
  auto t = std::make_tuple(platformCurrent, libraryTypeCurrent);
  return packageCopyPaths[t];
}
