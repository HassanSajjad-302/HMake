
#include "../header/ObjectFile.hpp"

ObjectFile::ObjectFile() : BTarget("", true, false)
{
}

ObjectFile::ObjectFile(const bool add0, const bool add1) : BTarget("", true, false, add0, add1)
{
}