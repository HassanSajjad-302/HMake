

#ifndef HMAKE_FAMILY_HPP
#define HMAKE_FAMILY_HPP

#include "filesystem"
#include "nlohmann/json.hpp"

enum class COMPILER_FAMILY{
    ANY, GCC, MSVC, CLANG
};

enum class LINKER_FAMILY{
    ANY, GCC, MSVC, CLANG
};

namespace fs = std::filesystem;
struct compiler{
    COMPILER_FAMILY family;
    fs::path path;
};

struct linker{
    LINKER_FAMILY family;
    fs::path path;
};

typedef nlohmann::ordered_json json;
void to_json(json &j, const compiler&p);
void to_json(json &j, const linker&p);

#endif //HMAKE_FAMILY_HPP
