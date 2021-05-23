

#ifndef HMAKE_FAMILY_HPP
#define HMAKE_FAMILY_HPP

#include "filesystem"
#include "nlohmann/json.hpp"

enum class CompilerFamily {
  ANY,
  GCC,
  MSVC,
  CLANG
};

enum class LinkerFamily {
  ANY,
  GCC,
  MSVC,
  CLANG
};

namespace fs = std::filesystem;
struct Compiler {
  CompilerFamily compilerFamily;
  fs::path path;
};

struct Linker {
  LinkerFamily linkerFamily;
  fs::path path;
};

typedef nlohmann::ordered_json Json;
void to_json(Json &j, const Compiler &p);
void to_json(Json &j, const Linker &p);

#endif//HMAKE_FAMILY_HPP
