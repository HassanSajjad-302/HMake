#ifndef HMAKE_ENVIRONMENT_HPP
#define HMAKE_ENVIRONMENT_HPP
#include "nlohmann/json.hpp"
#include <set>
#include <string>

using std::string, std::set;
using Json = nlohmann::ordered_json;
struct Environment
{
    set<string> includeDirectories;
    set<string> libraryDirectories;
    string compilerFlags;
    string linkerFlags;
    static Environment initializeEnvironmentFromVSBatchCommand(const string &command);
    static Environment initializeEnvironmentOnLinux();
};
void to_json(Json &j, const Environment &environment);
void from_json(const Json &j, Environment &environment); // Used in hbuild

#endif // HMAKE_ENVIRONMENT_HPP
