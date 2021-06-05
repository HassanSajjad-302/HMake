

#include "Package.hpp"
Package::Package(const fs::path &packageConfigureDirectoryPathRelativeToConfigureDirectory)
    : packageConfigureDirectory(Project::CONFIGURE_DIRECTORY.path / packageConfigureDirectoryPathRelativeToConfigureDirectory / "") {
}
