
#include "../header/ObjectFile.hpp"

ObjectFile::ObjectFile() : BTarget("", true, false)
{
}

ObjectFile::ObjectFile(const bool add0, const bool add1, const bool add2) : BTarget("", true, false, add0, add1, add2)
{
}