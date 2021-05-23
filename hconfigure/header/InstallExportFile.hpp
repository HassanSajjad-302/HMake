
#ifndef HMAKE_INSTALLEXPORTFILE_HPP
#define HMAKE_INSTALLEXPORTFILE_HPP

#include "Directory.hpp"
#include <string>

struct InstallExportFile {
  std::string exportName;
  directory destination;
};

#endif//HMAKE_INSTALLEXPORTFILE_HPP
