
#ifndef HMAKE_PLATFORMSPECIFIC_HPP
#define HMAKE_PLATFORMSPECIFIC_HPP

#ifdef USE_HEADER_UNITS
import "fmt/format.h";
import <string>;
import <filesystem>;
import "rapidjson/document.h";
import "rapidjson/encodings.h";
import <fstream>;
#else
#include "fmt/format.h"
#include "rapidjson/document.h"
#include "rapidjson/encodings.h"
#include <filesystem>
#include <fstream>
#include <string>
#endif

using fmt::format, std::string, std::filesystem::path, std::wstring, std::unique_ptr, std::make_unique;

using rapidjson::UTF8, rapidjson::UTF16, rapidjson::GenericDocument, rapidjson::GenericValue,
    rapidjson::GenericStringRef, rapidjson::kArrayType, rapidjson::kStringType;

// Change this _WIN32 to switch to UTF-16 on Windows.
#ifdef _ASDFKSAJDHFIWSJADNF

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

using PDocument = GenericDocument<UTF16<>>;
using PValue = GenericValue<UTF16<>>;
using PStringRef = GenericStringRef<wchar_t>;
using pstring_view = std::basic_string_view<wchar_t>;

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

using PDocument = GenericDocument<UTF8<>>;
using PValue = GenericValue<UTF8<>>;
using PStringRef = GenericStringRef<char>;
using pstring_view = std::basic_string_view<char>;

#endif

// PString to PStringRef
// #define ptoref(str) PStringRef((str).c_str(), (str).size())

PStringRef ptoref(pstring_view c);

// PString EXPAND
#define P_EXPAND(str) str.c_str(), (str).size()

// Rapid Helper PlatformSpecific OStream
struct RHPOStream
{
    unique_ptr<std::basic_ofstream<pchar>> of = nullptr;
    RHPOStream(pstring_view fileName);
    typedef pchar Ch;
    void Put(Ch c) const;
    void Flush();
};

void prettyWritePValueToFile(pstring_view fileName, PValue &value);
void writePValueToFile(pstring_view fileName, PValue &value);
unique_ptr<pchar[]> readPValueFromFile(pstring_view fileName, PDocument &document);
size_t pvalueIndexInArray(const PValue &pvalue, const PValue &element);
size_t pvalueIndexInSubArray(const PValue &pvalue, const PValue &element);

struct Indices
{

    inline constexpr static unsigned targetBuildCache = 1;

    struct TargetBuildCache
    {

        inline constexpr static unsigned sourceFiles = 0;
        inline constexpr static unsigned moduleFiles = 1;
        inline constexpr static unsigned headerUnits = 2;

        struct SourceFiles
        {
            inline constexpr static unsigned fullPath = 0;
            inline constexpr static unsigned compileCommandWithTool = 1;
            inline constexpr static unsigned headerFiles = 2;
        };

        struct ModuleFiles : public SourceFiles
        {
            inline constexpr static unsigned fullPath = 0;
            inline constexpr static unsigned scanningCommandWithTool = 1;
            inline constexpr static unsigned headerFiles = 2;
            inline constexpr static unsigned smRules = 3;
            struct SmRules
            {
                inline constexpr static unsigned exportName = 0;
                inline constexpr static unsigned isInterface = 1;
                inline constexpr static unsigned requireArray = 2;

                struct SingleModuleDep
                {
                    inline constexpr static unsigned logicalName = 0;
                    inline constexpr static unsigned fullPath = 1;
                    inline constexpr static unsigned boolean = 2;
                };
            };

            inline constexpr static unsigned compileCommandWithTool = 4;
        };
    };
};

#endif // HMAKE_PLATFORMSPECIFIC_HPP
