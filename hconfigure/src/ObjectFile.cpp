
#include "../header/ObjectFile.hpp"

ObjectFile::ObjectFile() = default;

ObjectFile::ObjectFile(const bool add0, const bool add1, const bool add2) : BTarget(add0, add1, add2)
{
}