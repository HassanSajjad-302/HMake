#include "Features.hpp"
#include "Cache.hpp"
#include "JConsts.hpp"
#include "fmt/format.h"
#include "nlohmann/json.hpp"

using Json = nlohmann::ordered_json;
using fmt::print;

Define::Define(string name_, string value_) : name{std::move(name_)}, value{std::move(value_)}
{
}

void to_json(Json &j, const Define &cd)
{
    j[JConsts::name] = cd.name;
    j[JConsts::value] = cd.value;
}

void from_json(const Json &j, Define &cd)
{
    cd.name = j.at(JConsts::name).get<string>();
    cd.value = j.at(JConsts::value).get<string>();
}

void to_json(Json &json, const OS &os)
{
    if (os == OS::NT)
    {
        json = JConsts::windows;
    }
    else if (os == OS::LINUX)
    {
        json = JConsts::linuxUnix;
    }
}

void from_json(const Json &json, OS &os)
{
    if (json == JConsts::windows)
    {
        os = OS::NT;
    }
    else if (json == JConsts::linuxUnix)
    {
        os = OS::LINUX;
    }
}

string getActualNameFromTargetName(TargetType bTargetType, const OS &os, const string &targetName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return targetName + (os == OS::NT ? ".exe" : "");
    }
    else if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string actualName;
        actualName = os == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += os == OS::NT ? ".lib" : ".a";
        return actualName;
    }
    else
    {
    }
    print(stderr, "Other Targets Are Not Supported Yet.\n");
    exit(EXIT_FAILURE);
}

string getTargetNameFromActualName(TargetType bTargetType, const OS &os, const string &actualName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return os == OS::NT ? actualName + ".exe" : actualName;
    }
    else if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string libName = actualName;
        // Removes lib from libName.a
        libName = os == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .a from libName.a or .lib from Name.lib
        unsigned short eraseCount = os == OS::NT ? 4 : 2;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    print(stderr, "Other Targets Are Not Supported Yet.\n");
    exit(EXIT_FAILURE);
}

ToolSet::ToolSet(TS ts_) : ts{ts_}
{
    // Initialize Name And Version
}

void to_json(Json &json, const ConfigType &configType)
{
    if (configType == ConfigType::DEBUG)
    {
        json = JConsts::debug;
    }
    else
    {
        json = JConsts::release;
    }
}

void from_json(const Json &json, ConfigType &configType)
{
    if (json == JConsts::debug)
    {
        configType = ConfigType::DEBUG;
    }
    else if (json == JConsts::release)
    {
        configType = ConfigType::RELEASE;
    }
}

void Features::initializeFromCacheFunc()
{
    configurationType = cache.projectConfigurationType;
    compiler = cache.compilerArray[cache.selectedCompilerArrayIndex];
    linker = cache.linkerArray[cache.selectedLinkerArrayIndex];
    archiver = cache.archiverArray[cache.selectedArchiverArrayIndex];
    libraryType = cache.libraryType;
    environment = cache.environment;
}