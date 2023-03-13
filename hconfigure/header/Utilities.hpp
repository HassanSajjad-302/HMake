#ifndef HMAKE_UTILITIES_HPP
#define HMAKE_UTILITIES_HPP

#ifdef USE_HEADER_UNITS
#include <string>
#include <vector>
#else
#include <string>
#include <vector>
#endif

using std::string, std::vector;
string addQuotes(const string &pathString);
string addEscapedQuotes(const string &pathString);
string file_to_string(const string &file_name);
vector<string> split(string str, const string &token);

#endif // HMAKE_UTILITIES_HPP
