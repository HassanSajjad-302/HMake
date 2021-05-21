

#ifndef HMAKE_FLAGS_HPP
#define HMAKE_FLAGS_HPP

#include <iostream>
#include "family.hpp"
#include "CONFIG_TYPE.hpp"

//TODO: Improve the console message and documentation.
struct Flags{

    std::map<std::tuple<COMPILER_FAMILY, CONFIG_TYPE>, std::string > compilerFlags;
    std::map<std::tuple<LINKER_FAMILY, CONFIG_TYPE>, std::string > linkerFlags;

    //bool and enum helpers for using Flags class with some operator overload magic
    bool compileHelper = false;
    bool linkHelper = false;
    bool configHelper = false;

    COMPILER_FAMILY compilerCurrent;
    LINKER_FAMILY linkerCurrent;
    CONFIG_TYPE configCurrent;
public:
    Flags& operator[](COMPILER_FAMILY compilerFamily);
    Flags& operator[](LINKER_FAMILY linkerFamily);
    Flags& operator[](CONFIG_TYPE configType);

    void operator=(std::string flags);

    operator std::string();
};
#endif //HMAKE_FLAGS_HPP
