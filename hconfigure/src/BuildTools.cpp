#include "BuildTools.hpp"
#include <utility>

Version::Version(const unsigned int majorVersion_, const unsigned int minorVersion_, const unsigned int patchVersion_)
    : majorVersion{majorVersion_}, minorVersion{minorVersion_}, patchVersion{patchVersion_}
{
}

BuildTool::BuildTool(const BTFamily btFamily_, const BTSubFamily btSubFamily_, const Version btVersion_, string btPath_)
    : bTFamily(btFamily_), btSubFamily(btSubFamily_), bTVersion(btVersion_), bTPath(std::move(btPath_))
{
}

Compiler::Compiler(const BTFamily btFamily_, const BTSubFamily btSubFamily_, const Version btVersion_, string btPath_)
    : BuildTool(btFamily_, btSubFamily_, btVersion_, std::move(btPath_))
{
}

Linker::Linker(const BTFamily btFamily_, const BTSubFamily btSubFamily_, const Version btVersion_, string btPath_)
    : BuildTool(btFamily_, btSubFamily_, btVersion_, std::move(btPath_))
{
}

Archiver::Archiver(const BTFamily btFamily_, const BTSubFamily btSubFamily_, const Version btVersion_, string btPath_)
    : BuildTool(btFamily_, btSubFamily_, btVersion_, std::move(btPath_))
{
}