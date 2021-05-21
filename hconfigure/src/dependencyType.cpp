

#include <dependencyType.hpp>

void to_json(json &j, const dependencyType &p)
{
    if(p == dependencyType::PUBLIC){
        j = "PUBLIC";
    }else if(p == dependencyType::PRIVATE){
        j = "PRIVATE";
    }else{
        j = "INTERFACE";
    }
}


