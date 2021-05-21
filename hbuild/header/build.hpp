
#ifndef HMAKE_BUILD_HPP
#define HMAKE_BUILD_HPP

#include "filesystem"

namespace fs = std::filesystem;
namespace build{
    void build(fs::path exeHmakeFilePath);
}

#endif //HMAKE_BUILD_HPP
