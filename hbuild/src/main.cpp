
#include "BBuild.hpp"

enum class BuildMode {
  TARGET,
  PROJECT_VARIANT,
  PACKAGE_VARIANT,
  PACKAGE,
  NILL
};
int main(int argc, char **argv) {
  if (argc == 2) {
    std::string filePath = argv[1];
    fs::path buildFilePath = filePath;
    if (buildFilePath.is_relative()) {
      buildFilePath = (fs::current_path() / buildFilePath).lexically_normal();
    } else {
      buildFilePath = buildFilePath.lexically_normal();
    }
    if (!fs::is_regular_file(buildFilePath)) {
      throw std::logic_error(buildFilePath.string() + " is not regular file");
    }

  } else {
    fs::path file;
    BuildMode mode = BuildMode::NILL;
    for (auto &i : fs::directory_iterator(fs::current_path())) {
      if (i.is_regular_file()) {
        std::string fileName = i.path().filename();
        if (fileName.ends_with(".executable.hmake") || fileName.ends_with(".static.hmake")
            || fileName.ends_with(".shared.hmake")) {
          file = i;
          mode = BuildMode::TARGET;
        }
        if (fileName == "projectVariant.hmake") {
          file = i;
          mode = BuildMode::PROJECT_VARIANT;
          break;
        }
        if (fileName == "packageVariant.hmake") {
          file = i;
          mode = BuildMode::PACKAGE_VARIANT;
        }
        if (fileName == "package.hmake") {
          file = i;
          mode = BuildMode::PACKAGE;
        }
      }
    }
    if (mode == BuildMode::NILL) {
      throw std::runtime_error("Could not find or determine the target or projectVariant or packageVariant "
                               "or package file which is to be built.");
    }
    if (mode == BuildMode::TARGET) {
      BTarget builder(file);
      builder.build();
    } else if (mode == BuildMode::PROJECT_VARIANT) {
      BProjectVariant projectVariant(file);
      projectVariant.build();
    } else if (mode == BuildMode::PACKAGE_VARIANT) {
      bool copyVariant = BPackageVariant::shouldVariantBeCopied();
      BPackageVariant variant(file);
      if (copyVariant) {
        variant.build();
      } else {
      }
    } else {
      BPackage package(file);
    }
  }
}
