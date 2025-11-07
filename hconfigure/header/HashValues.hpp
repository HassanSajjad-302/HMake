#ifndef HASHVALUES_HPP
#define HASHVALUES_HPP

#include "cstdint"
#include "string"

using std::string_view;

class CppTarget;
struct DSCFeatures;
template <typename T> struct DSC;
class Configuration;
struct RoundZeroUpdateBTarget;
struct Define;

uint64_t hash_value(const CppTarget &p);
uint64_t hash_value(const DSC<CppTarget> &p);
uint64_t hash_value(const Configuration &p);
uint64_t hash_value(const Define &p);
uint64_t hash_value(const string_view &p);

bool operator==(const CppTarget &lhs, const CppTarget &rhs);
bool operator==(const DSC<CppTarget> &lhs, const DSC<CppTarget> &rhs);
bool operator==(const Configuration &lhs, const Configuration &rhs);
bool operator==(const Define &lhs, const Define &rhs);

#endif // HASHVALUES_HPP
