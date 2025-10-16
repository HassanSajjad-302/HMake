
#ifndef HMAKE_PLATFORMSPECIFIC_HPP
#define HMAKE_PLATFORMSPECIFIC_HPP
#include "rapidjson/document.h"

#ifdef USE_HEADER_UNITS
import "fmt/format.h";
import "phmap.h";
import <filesystem>;
import <span>;
import <string>;
import <vector>;
#else
#include "fmt/format.h"
#include "phmap.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>
#endif

using fmt::format, std::string, std::filesystem::path, std::wstring, std::unique_ptr, std::make_unique, std::vector,
    std::span, std::string_view;

// There is nothing platform-specific in this file. It is just another BuildSystemFunctions.hpp file. Some functions go
// there, some go here.

#define FORMAT(formatStr, ...) fmt::format(formatStr, __VA_ARGS__)

vector<char> readBufferFromFile(const string &fileName);
vector<char> readBufferFromCompressedFile(const string &fileName);

void readConfigCache();
void readBuildCache();

void writeConfigBuffer(vector<char> &buffer);
void writeBuildBuffer(vector<char> &buffer);

// While decompressing lz4 file, we allocate following + 1 the buffer size.
// So, we have compressed filee * bufferMultiplier times the space.
// Also, while storing we check that the original file size / compresseed file size
// is not equal to or greater than bufferMultiplier. Hence validating our assumption.
void writeBufferToCompressedFile(const string &fileName, const vector<char> &fileBuffer);
bool compareStringsFromEnd(string_view lhs, string_view rhs);
void lowerCaseOnWindows(char *ptr, uint64_t size);
bool childInParentPathNormalized(string_view parent, string_view child);
void readValueFromFile(string_view fileName, rapidjson::Document &document, rapidjson::ParseFlag flag);

#endif // HMAKE_PLATFORMSPECIFIC_HPP
