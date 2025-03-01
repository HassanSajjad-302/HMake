#ifndef HASHVALUES_HPP
#define HASHVALUES_HPP

#ifdef USE_HEADER_UNITS
import "cstdint";
import "PlatformSpecific.hpp";
#else
#include "PlatformSpecific.hpp"
#include "cstdint"
#endif

class CppSourceTarget;
struct DSCFeatures;
template <typename T> struct DSC;
class Configuration;
struct RoundZeroUpdateBTarget;
struct Define;

uint64_t hash_value(const CppSourceTarget &p);
uint64_t hash_value(const DSC<CppSourceTarget> &p);
uint64_t hash_value(const Configuration &p);
uint64_t hash_value(const RoundZeroUpdateBTarget &p);
uint64_t hash_value(const Define &p);
uint64_t hash_value(const string_view &p);

bool operator==(const CppSourceTarget &lhs, const CppSourceTarget &rhs);
bool operator==(const DSC<CppSourceTarget> &lhs, const DSC<CppSourceTarget> &rhs);
bool operator==(const Configuration &lhs, const Configuration &rhs);
bool operator==(const RoundZeroUpdateBTarget &lhs, const RoundZeroUpdateBTarget &rhs);
bool operator==(const Define &lhs, const Define &rhs);

#endif // HASHVALUES_HPP
