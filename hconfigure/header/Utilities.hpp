#ifndef HMAKE_UTILITIES_HPP
#define HMAKE_UTILITIES_HPP

#ifdef USE_HEADER_UNITS
import <string>;
import <vector>;
#else
#include <string>
#include <vector>
#endif

using std::string, std::vector;
string addQuotes(const string &pathString);
string addEscapedQuotes(const string &pathString);
string file_to_string(const string &file_name);
vector<string> split(string str, const string &token);

template <typename T> void emplaceInVector(vector<T> &v, T &&t)
{
    for (const T &t_ : v)
    {
        if (t_ == t)
        {
            return;
        }
    }
    auto a = v.emplace_back(std::forward<T>(t));
}

#endif // HMAKE_UTILITIES_HPP
