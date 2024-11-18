
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
    FILE *fp = nullptr;
    RHPOStream(pstring_view fileName);
    ~RHPOStream();
    typedef pchar Ch;
    void Put(Ch c) const;
    void Flush();
};

// While decompressing lz4 file, we allocate following + 1 the buffer size.
// So, we have compressed filee * bufferMultiplier times the space.
// Also, while storing we check that the original file size / compresseed file size
// is not equal to or greater than bufferMultiplier. Hence validating our assumption.
void prettyWritePValueToFile(pstring_view fileName, const PValue &value);
unique_ptr<vector<pchar>> readPValueFromFile(pstring_view fileName, PDocument &document);
void writePValueToFile(pstring fileName, const PValue &value);
unique_ptr<vector<pchar>> readPValueFromCompressedFile(pstring_view fileName, PDocument &document);
void writePValueToCompressedFile(pstring fileName, const PValue &value);
uint64_t pvalueIndexInSubArray(const PValue &pvalue, const PValue &element);
// This will consider the currentCacheIndex in its search
uint64_t pvalueIndexInSubArrayConsidered(const PValue &pvalue, const PValue &element);
bool compareStringsFromEnd(pstring_view lhs, pstring_view rhs);
uint64_t nodeIndexInPValueArray(const PValue &pvalue, const class Node &node);
bool isNodeInPValue(const PValue &value, const Node &node);
void lowerCasePStringOnWindows(pchar *ptr, uint64_t size);
bool childInParentPathRecursiveNormalized(pstring_view parent, pstring_view child);

// TODO
// Optimize this
inline vector<class CppSourceTarget *> cppSourceTargets{10000};
namespace Indices
{

namespace ConfigCache
{
constexpr static unsigned name = 0;
namespace CppConfig
{
constexpr static unsigned reqInclsArray = 0;
constexpr static unsigned useReqInclsArray = 1;
constexpr static unsigned reqHUDirsArray = 2;
constexpr static unsigned useReqHUDirsArray = 3;
constexpr static unsigned sourceFiles = 4;
constexpr static unsigned moduleFiles = 5;
} // namespace CppConfig

namespace LinkConfig
{

constexpr static unsigned requirementLibraryDirectoriesArray = 0;
constexpr static unsigned usageRequirementLibraryDirectoriesArray = 1;
constexpr static unsigned outputDirectoryNode = 2;
constexpr static unsigned outputFileNode = 3;

} // namespace LinkConfig

} // namespace ConfigCache

namespace BuildCache
{
namespace CppBuild
{
constexpr static unsigned sourceFiles = 0;
constexpr static unsigned moduleFiles = 1;
constexpr static unsigned headerUnits = 2;

namespace SourceFiles
{
constexpr static unsigned fullPath = 0;
constexpr static unsigned compileCommandWithTool = 1;
constexpr static unsigned headerFiles = 2;
// Used only in GenerateModuleData::YES mode
constexpr static unsigned moduleData = 3;
namespace ModuleData
{
constexpr static unsigned exportName = 0;
constexpr static unsigned isInterface = 1;
constexpr static unsigned headerUnitArray = 2;
constexpr static unsigned moduleArray = 3;

namespace SingleHeaderUnitDep
{
constexpr static unsigned fullPath = 0;
constexpr static unsigned logicalName = 1;
// Maybe store this info in logicalName and extract it from there
constexpr static unsigned angle = 2;
constexpr static unsigned targetIndex = 3;
constexpr static unsigned myIndex = 4;
} // namespace SingleHeaderUnitDep

namespace SingleModuleDep
{
// This is the value of the source-path key and is assigned before saving. So, in next build in
// resolveRequirePaths, we check that whether we are resolving to the same module.
constexpr static unsigned fullPath = 0;
constexpr static unsigned logicalName = 1;
} // namespace SingleModuleDep
} // namespace ModuleData
} // namespace SourceFiles

namespace ModuleFiles
{
using namespace SourceFiles;
constexpr static unsigned fullPath = 0;
constexpr static unsigned scanningCommandWithTool = 1;
constexpr static unsigned headerFiles = 2;
constexpr static unsigned smRules = 3;
namespace SmRules = ModuleData;

constexpr static unsigned compileCommandWithTool = 4;
} // namespace ModuleFiles
} // namespace CppBuild

namespace LinkBuild
{
constexpr static unsigned commandWithoutArgumentsWithTools = 1;
constexpr static unsigned objectFiles = 2;
constexpr static unsigned localLinkCommandWithoutTargets = 3;
} // namespace LinkBuild

} // namespace BuildCache
} // namespace Indices

#endif // HMAKE_PLATFORMSPECIFIC_HPP
