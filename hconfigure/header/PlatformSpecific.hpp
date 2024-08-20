
#ifndef HMAKE_PLATFORMSPECIFIC_HPP
#define HMAKE_PLATFORMSPECIFIC_HPP

#ifdef USE_HEADER_UNITS
import "fmt/format.h";
import <string>;
import <filesystem>;
import "rapidjson/document.h";
import "rapidjson/encodings.h";
import <fstream>;
import <vector>;
#else
#include "fmt/format.h"
#include "rapidjson/document.h"
#include "rapidjson/encodings.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#endif

using fmt::format, std::string, std::filesystem::path, std::wstring, std::unique_ptr, std::make_unique, std::vector;

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
wstring (path::*toPStr)() const = &path::wstring;

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
    static void Flush();
};

// While decompressing lz4 file, we allocate following + 1 the buffer size.
// So, we have compressed filee * bufferMultiplier times the space.
// Also, while storing we check that the original file size / compresseed file size
// is not equal to or greater than bufferMultiplier. Hence validating our assumption.
void prettyWritePValueToFile(pstring_view fileName, const PValue &value);
unique_ptr<vector<pchar>> readPValueFromFile(pstring_view fileName, PDocument &document);
void writePValueToFile(pstring_view fileName, const PValue &value);
unique_ptr<vector<pchar>> readPValueFromCompressedFile(pstring_view fileName, PDocument &document);
void writePValueToCompressedFile(pstring_view fileName, const PValue &value);
size_t pvalueIndexInSubArray(const PValue &pvalue, const PValue &element);
bool compareStringsFromEnd(pstring_view lhs, pstring_view rhs);
void lowerCasePString(pstring &str);
bool childInParentPathRecursiveNormalized(pstring_view parent, pstring_view child);

struct Indices
{

    constexpr static unsigned targetBuildCache = 1;

    struct CppTargetBuildCache
    {

        constexpr static unsigned sourceFiles = 0;
        constexpr static unsigned moduleFiles = 1;
        constexpr static unsigned headerUnits = 2;

        struct SourceFiles
        {
            constexpr static unsigned fullPath = 0;
            constexpr static unsigned compileCommandWithTool = 1;
            constexpr static unsigned headerFiles = 2;
        };

        struct ModuleFiles : SourceFiles
        {
            constexpr static unsigned fullPath = 0;
            constexpr static unsigned scanningCommandWithTool = 1;
            constexpr static unsigned headerFiles = 2;
            constexpr static unsigned smRules = 3;
            struct SmRules
            {
                constexpr static unsigned exportName = 0;
                constexpr static unsigned isInterface = 1;
                constexpr static unsigned requireArray = 2;

                struct SingleModuleDep
                {
                    constexpr static unsigned logicalName = 0;
                    constexpr static unsigned isHeaderUnit = 1;
                    // Points to different things in-case of header-unit and module. In-case of header-unit, it is the
                    // value of the source-path key, while in-case of module it is assigned before saving. So, in next
                    // build in resolveRequiePaths, we check that whether we are resolving to the same module.
                    constexpr static unsigned fullPath = 2;
                    constexpr static unsigned boolean = 3;
                };
            };

            constexpr static unsigned compileCommandWithTool = 4;
        };
    };

    struct LinkTargetBuildCache
    {
        constexpr static unsigned commandWithoutArgumentsWithTools = 1;
        constexpr static unsigned objectFiles = 2;
        constexpr static unsigned localLinkCommandWithoutTargets = 3;
    };

    struct LinkTargetConfigCache
    {
        constexpr static unsigned outputDirectoryNode = 1;
        constexpr static unsigned outputFileNode = 2;
        constexpr static unsigned librariesDirectoriesArray = 3;
    };

    struct CppTargetConfigCache
    {
        constexpr static unsigned requriementIncludesArray = 1;
        constexpr static unsigned usageRequirementIncludesArray = 2;
        constexpr static unsigned requirementHUDirArray = 3;
        constexpr static unsigned usageRequirementHUDirArray = 4;
        constexpr static unsigned sourceFiles = 5;
        constexpr static unsigned moduleFiles = 6;
    };
};

#endif // HMAKE_PLATFORMSPECIFIC_HPP
