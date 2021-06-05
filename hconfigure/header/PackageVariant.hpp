
#ifndef HMAKE_HCONFIGURE_HEADER_PACKAGEVARIANT_HPP
#define HMAKE_HCONFIGURE_HEADER_PACKAGEVARIANT_HPP

#include "ConfigType.hpp"
#include "Directory.hpp"
#include "Executable.hpp"
#include "Family.hpp"
#include "Flags.hpp"
#include "nlohmann/json.hpp"

using Json = nlohmann::ordered_json;
struct PackageVariant {
  ConfigType packageVariantConfigurationType;
  Compiler packageVariantCompiler;
  Linker packageVariantLinker;
  Flags flags;
  LibraryType libraryType;
  std::vector<Executable> projectExecutables;
  std::vector<Library> projectLibraries;

  decltype(Json::object()) associtedJson;
  PackageVariant();
};

#endif//HMAKE_HCONFIGURE_HEADER_PACKAGEVARIANT_HPP
