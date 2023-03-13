
#ifdef USE_HEADER_UNITS
#include "BuildTools.hpp"
#include "JConsts.hpp"
#include <string>
#include <utility>
#else
#include "BuildTools.hpp"
#include "JConsts.hpp"
#include <string>
#include <utility>
#endif
using std::to_string, std::stringstream;

Version::Version(unsigned int majorVersion_, unsigned int minorVersion_, unsigned int patchVersion_)
    : majorVersion{majorVersion_}, minorVersion{minorVersion_}, patchVersion{patchVersion_}
{
}

void to_json(Json &j, const Version &p)
{
    j = to_string(p.majorVersion) + "." + to_string(p.minorVersion) + "." + to_string(p.patchVersion);
}

void from_json(const Json &j, Version &v)
{
    string jString = j;
    char delim = '.';
    stringstream ss(jString);
    string item;
    int count = 0;
    while (getline(ss, item, delim))
    {
        if (count == 0)
        {
            v.majorVersion = stoi(item);
        }
        else if (count == 1)
        {
            v.minorVersion = stoi(item);
        }
        else
        {
            v.patchVersion = stoi(item);
        }
        ++count;
    }
}

void to_json(Json &json, const BTFamily &bTFamily)
{
    if (bTFamily == BTFamily::GCC)
    {
        json = JConsts::gcc;
    }
    else if (bTFamily == BTFamily::MSVC)
    {
        json = JConsts::msvc;
    }
    else
    {
        json = JConsts::clang;
    }
}

void from_json(const Json &json, BTFamily &bTFamily)
{
    if (json == JConsts::gcc)
    {
        bTFamily = BTFamily::GCC;
    }
    else if (json == JConsts::msvc)
    {
        bTFamily = BTFamily::MSVC;
    }
    else if (json == JConsts::clang)
    {
        bTFamily = BTFamily::CLANG;
    }
}

BuildTool::BuildTool(BTFamily btFamily_, Version btVersion_, path btPath_)
    : bTFamily(btFamily_), bTVersion(btVersion_), bTPath(std::move(btPath_))
{
}

void to_json(Json &json, const BuildTool &buildTool)
{
    json[JConsts::family] = buildTool.bTFamily;
    json[JConsts::path] = buildTool.bTPath.generic_string();
    json[JConsts::version] = buildTool.bTVersion;
}

void from_json(const Json &json, BuildTool &buildTool)
{
    buildTool.bTPath = json.at(JConsts::path).get<string>();
    buildTool.bTFamily = json.at(JConsts::family).get<BTFamily>();
    buildTool.bTVersion = json.at(JConsts::version).get<Version>();
}

Compiler::Compiler(BTFamily btFamily_, Version btVersion_, path btPath_)
    : BuildTool(btFamily_, btVersion_, std::move(btPath_))
{
}

Linker::Linker(BTFamily btFamily_, Version btVersion_, path btPath_)
    : BuildTool(btFamily_, btVersion_, std::move(btPath_))
{
}

Archiver::Archiver(BTFamily btFamily_, Version btVersion_, path btPath_)
    : BuildTool(btFamily_, btVersion_, std::move(btPath_))
{
}