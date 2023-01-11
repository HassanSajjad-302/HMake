#include "BuildTools.hpp"
#include "JConsts.hpp"
#include <string>

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

bool operator<(const Version &lhs, const Version &rhs)
{
    if (lhs.majorVersion < rhs.majorVersion)
    {
        return true;
    }
    else if (lhs.majorVersion == rhs.majorVersion)
    {
        if (lhs.minorVersion < rhs.minorVersion)
        {
            return true;
        }
        else if (lhs.minorVersion == rhs.minorVersion)
        {
            if (lhs.patchVersion < rhs.patchVersion)
            {
                return true;
            }
        }
    }
    return false;
}

bool operator>(const Version &lhs, const Version &rhs)
{
    return rhs < lhs;
}

bool operator<=(const Version &lhs, const Version &rhs)
{
    return !(lhs > rhs);
}

bool operator>=(const Version &lhs, const Version &rhs)
{
    return !(lhs < rhs);
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
