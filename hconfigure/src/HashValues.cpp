
#ifdef USE_HEADER_UNITS
import "Configuration.hpp";
import "CppSourceTarget.hpp";
import "HashValues.hpp";
import "DSC.hpp";
import "rapidhash.h";
#else
#include "HashValues.hpp"
#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "rapidhash/rapidhash.h"
#endif

uint64_t hash_value(const CppSourceTarget &p)
{
    return p.id;
}

uint64_t hash_value(const DSC<CppSourceTarget> &p)
{
    uint64_t arr[2];
    arr[0] = p.ploat->id;
    arr[1] = p.objectFileProducer->id;
    return rapidhash(&arr, sizeof(arr));
}

uint64_t hash_value(const Configuration &p)
{
    return p.id;
}

uint64_t hash_value(const Define &p)
{
    const uint64_t hash[2] = {rapidhash(p.name.c_str(), p.name.size()), rapidhash(p.value.c_str(), p.value.size())};
    return rapidhash(&hash, sizeof(hash));
}

uint64_t hash_value(const string_view &p)
{
    return rapidhash(p.data(), p.size());
}

bool operator==(const CppSourceTarget &lhs, const CppSourceTarget &rhs)
{
    return lhs.id == rhs.id;
}

bool operator==(const DSC<CppSourceTarget> &lhs, const DSC<CppSourceTarget> &rhs)
{
    return lhs.ploat->id == rhs.ploat->id && rhs.objectFileProducer->id == rhs.objectFileProducer->id;
}

bool operator==(const Configuration &lhs, const Configuration &rhs)
{
    return lhs.id == rhs.id;
}

bool operator==(const Define &lhs, const Define &rhs)
{
    return lhs.name == rhs.name && lhs.value == rhs.value;
}