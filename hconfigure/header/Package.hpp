
#ifndef HMAKE_HCONFIGURE_HEADER_PACKAGE_HPP
#define HMAKE_HCONFIGURE_HEADER_PACKAGE_HPP

#include "PackageVariant.hpp"
#include "Project.hpp"
#include "set"

struct Package {
  Directory packageConfigureDirectory;
  Directory packageInstallDirectory;
  std::vector<PackageVariant> packageVariants;

  explicit Package(const fs::path &packageConfigureDirectoryPathRelativeToConfigureDirectory);
};
#endif//HMAKE_HCONFIGURE_HEADER_PACKAGE_HPP
