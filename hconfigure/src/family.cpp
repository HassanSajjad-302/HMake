

#include "family.hpp"

void to_json(json &j, const compiler &p) {
    if(p.family == COMPILER_FAMILY::GCC){
        j["FAMILY"] = "GCC";
    }else if(p.family == COMPILER_FAMILY::MSVC){
        j["FAMILY"] = "MSVC";
    }else{
        j["FAMILY"] = "CLANG";
    }
    j["PATH"] = p.path.string();
}

void to_json(json &j, const linker &p) {
    if(p.family == LINKER_FAMILY::GCC){
        j["FAMILY"] = "GCC";
    }else if(p.family == LINKER_FAMILY::MSVC){
        j["FAMILY"] = "MSVC";
    }else{
        j["FAMILY"] = "CLANG";
    }
    j["PATH"] = p.path.string();
}