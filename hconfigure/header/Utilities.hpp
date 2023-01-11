#ifndef HMAKE_UTILITIES_HPP
#define HMAKE_UTILITIES_HPP
#include <string>
#include <vector>

using std::string, std::vector;
string addQuotes(const string &pathString);
string addEscapedQuotes(const string &pathString);
string file_to_string(const string &file_name);
vector<string> split(string str, const string &token);

#endif // HMAKE_UTILITIES_HPP
