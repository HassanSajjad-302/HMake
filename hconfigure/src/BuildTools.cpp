
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "JConsts.hpp";
import <utility>;
#else
#include "BuildTools.hpp"
#include "JConsts.hpp"
#include <utility>
#endif

Version::Version(const unsigned int majorVersion_, const unsigned int minorVersion_, const unsigned int patchVersion_)
    : majorVersion{majorVersion_}, minorVersion{minorVersion_}, patchVersion{patchVersion_}
{
}

void to_json(Json &j, const Version &p)
{
    j = to_pstring(p.majorVersion) + "." + to_pstring(p.minorVersion) + "." + to_pstring(p.patchVersion);
}

void from_json(const Json &j, Version &v)
{
    const pstring jString = j;
    const char delim = '.';
    pstringstream ss(jString);
    pstring item;
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

BuildTool::BuildTool(const BTFamily btFamily_, const Version btVersion_, path btPath_)
    : bTFamily(btFamily_), bTVersion(btVersion_), bTPath(std::move(btPath_))
{
}

void to_json(Json &json, const BuildTool &buildTool)
{
    json[JConsts::family] = buildTool.bTFamily;
    json[JConsts::path] = (buildTool.bTPath.*toPStr)();
    json[JConsts::version] = buildTool.bTVersion;
}

void from_json(const Json &json, BuildTool &buildTool)
{
    buildTool.bTPath = json.at(JConsts::path).get<pstring>();
    buildTool.bTFamily = json.at(JConsts::family).get<BTFamily>();
    buildTool.bTVersion = json.at(JConsts::version).get<Version>();
}

Compiler::Compiler(const BTFamily btFamily_, const Version btVersion_, path btPath_)
    : BuildTool(btFamily_, btVersion_, std::move(btPath_))
{
}

Linker::Linker(const BTFamily btFamily_, const Version btVersion_, path btPath_)
    : BuildTool(btFamily_, btVersion_, std::move(btPath_))
{
}

Archiver::Archiver(const BTFamily btFamily_, const Version btVersion_, path btPath_)
    : BuildTool(btFamily_, btVersion_, std::move(btPath_))
{
}

ScannerTool::ScannerTool(const BTFamily btFamily_, const Version btVersion_, path btPath_)
    : BuildTool(btFamily_, btVersion_, std::move(btPath_))
{
}
