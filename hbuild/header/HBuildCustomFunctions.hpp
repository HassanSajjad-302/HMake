

#ifndef HMAKE_HBUILD_HEADER_CUSTOMFUNCTIONS_H
#define HMAKE_HBUILD_HEADER_CUSTOMFUNCTIONS_HPP
#include "filesystem"
#include "nlohmann/json.hpp"
#include "string"

using Json = nlohmann::ordered_json;
namespace build {

namespace fs = std::filesystem;

void parseProjectFileAdStartBuildingTarget(const fs::path &targetFilePath);

void doJsonSpecialParseAndConvertStringArrayToPathArray(const std::string &jstr, Json &j, std::vector<fs::path> &container);

template<size_t N>
constexpr size_t length(char const (&)[N]) {
  return N - 1;
}

}// namespace build
#endif//HMAKE_HBUILD_HEADER_CUSTOMFUNCTIONS_H
