
#ifndef HMAKE_PLATFORMSPECIFIC_HPP
#define HMAKE_PLATFORMSPECIFIC_HPP

#ifdef USE_HEADER_UNITS
import "fmt/format.h";
import <string>;
import <filesystem>;
#else
#include "fmt/format.h"
#include <filesystem>
#include <string>
#endif

using fmt::format, std::string, std::filesystem::path, std::wstring;

#ifndef _WIN32

#ifdef USE_HEADER_UNITS
import "fmt/xchar.h";
#else
#include "fmt/xchar.h"
#endif

#define FORMAT(formatStr, ...) fmt::format(L##formatStr, __VA_ARGS__)

using pstring = std::wstring;
using pchar = wchar_t;
inline wstring (path::*toPStr)() const = &path::wstring;

template <typename T> wstring to_pstring(T t)
{
    return std::to_wstring(t);
}
using opstringstream = std::basic_ostringstream<wchar_t>;
using pstringstream = std::basic_stringstream<char>;

#else

#define FORMAT(formatStr, ...) fmt::format(formatStr, __VA_ARGS__)

using pstring = std::string;
using pchar = char;
inline string (path::*toPStr)() const = &path::string;
template <typename T> string to_pstring(T t)
{
    return std::to_string(t);
}
using opstringstream = std::ostringstream;
using pstringstream = std::stringstream;

#endif

#endif // HMAKE_PLATFORMSPECIFIC_HPP
