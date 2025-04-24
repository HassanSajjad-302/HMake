
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

using fmt::format, std::string, std::filesystem::path, std::wstring, std::unique_ptr, std::make_unique, std::vector,
    rapidjson::Document, rapidjson::Value, std::string_view, rapidjson::GenericStringRef;

// There is nothing platform specific in this file. It is just another BuildSystemFunctions.hpp file. Some functions go
// there, some go here.

#define FORMAT(formatStr, ...) fmt::format(formatStr, __VA_ARGS__)
#define CALL_ONLY_AT_CONFIGURE_TIME

// value to string_view
inline string_view vtosv(const Value &v)
{
    return {v.GetString(), v.GetStringLength()};
}
// string_view to GenericstringRef
inline GenericStringRef<char> svtogsr(string_view str)
{
    return {str.data(), static_cast<rapidjson::SizeType>(str.size())};
}
void prettyWriteValueToFile(string_view fileName, const Value &value);
// While decompressing lz4 file, we allocate following + 1 the buffer size.
// So, we have compressed filee * bufferMultiplier times the space.
// Also, while storing we check that the original file size / compresseed file size
// is not equal to or greater than bufferMultiplier. Hence validating our assumption.
unique_ptr<vector<char>> readValueFromFile(string_view fileName, Document &document);
void writeValueToFile(string fileName, const Value &value);
unique_ptr<vector<char>> readValueFromCompressedFile(string_view fileName, Document &document);
void writeValueToCompressedFile(string fileName, const Value &value);
uint64_t valueIndexInSubArray(const Value &value, const Value &element);
// This will consider the currentCacheIndex in its search
uint64_t valueIndexInSubArrayConsidered(const Value &value, const Value &element);
bool compareStringsFromEnd(string_view lhs, string_view rhs);
uint64_t nodeIndexInValueArray(const Value &value, const class Node &node);
bool isNodeInValue(const Value &value, const Node &node);
void lowerCasePStringOnWindows(char *ptr, uint64_t size);
bool childInParentPathNormalized(string_view parent, string_view child);

namespace Indices
{

namespace ConfigCache
{
constexpr static unsigned name = 0;
namespace CppConfig
{
constexpr static unsigned name = 0;
constexpr static unsigned reqInclsArray = 1;
constexpr static unsigned useReqInclsArray = 2;
constexpr static unsigned reqHUDirsArray = 3;
constexpr static unsigned useReqHUDirsArray = 4;
constexpr static unsigned sourceFiles = 5;
constexpr static unsigned moduleFiles = 6;
constexpr static unsigned headerUnits = 7;
constexpr static unsigned buildCacheFilesDirPath = 8;
} // namespace CppConfig

namespace LinkConfig
{
constexpr static unsigned name = 0;
constexpr static unsigned reqLibraryDirsArray = 1;
constexpr static unsigned useReqLibraryDirsArray = 2;
constexpr static unsigned outputFileNode = 3;
constexpr static unsigned buildCacheFilesDirPath = 4;

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
} // namespace SourceFiles

namespace ModuleFiles
{
using namespace SourceFiles;
constexpr static unsigned fullPath = 0;
constexpr static unsigned scanningCommandWithTool = 1;
constexpr static unsigned headerFiles = 2;
constexpr static unsigned smRules = 3;

namespace SmRules
{
constexpr static unsigned exportName = 0;
constexpr static unsigned isInterface = 1;
constexpr static unsigned headerUnitArray = 2;
constexpr static unsigned moduleArray = 3;

namespace SingleHeaderUnitDep
{
constexpr static unsigned fullPath = 0;
// Maybe store this info in logicalName and extract it from there
constexpr static unsigned angle = 1;
constexpr static unsigned targetIndex = 2;
constexpr static unsigned myIndex = 3;
} // namespace SingleHeaderUnitDep

namespace SingleModuleDep
{
// This is the value of the source-path key and is assigned before saving. So, in next build in
// resolveRequirePaths, we check that whether we are resolving to the same module.
constexpr static unsigned fullPath = 0;
constexpr static unsigned logicalName = 1;
} // namespace SingleModuleDep
} // namespace SmRules
constexpr static unsigned compileCommandWithTool = 4;
} // namespace ModuleFiles
} // namespace CppBuild

namespace LinkBuild
{
constexpr static unsigned commandWithoutArgumentsWithTools = 0;
constexpr static unsigned objectFiles = 1;
} // namespace LinkBuild

} // namespace BuildCache
} // namespace Indices

#endif // HMAKE_PLATFORMSPECIFIC_HPP
