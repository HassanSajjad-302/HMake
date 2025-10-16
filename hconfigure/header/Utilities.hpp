#ifndef HMAKE_UTILITIES_HPP
#define HMAKE_UTILITIES_HPP

#ifdef USE_HEADER_UNITS
import "PlatformSpecific.hpp";
import <vector>;
#else
#include "PlatformSpecific.hpp"
#include <vector>
#endif

using std::vector;
string addQuotes(const string_view pstr);
string addEscapedQuotes(const string &pstr);
string fileToString(const string &file_name);
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
