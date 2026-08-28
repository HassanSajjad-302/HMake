#include "Cache.hpp"

#include "Node.hpp"
#include "rapidhash/rapidhash.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <thread>

namespace
{
[[noreturn]] void projectCacheError(const path &filePath, const string_view message)
{
    printErrorMessage(FORMAT("Invalid project cache.\nFile: {}\n{}", filePath.string(), message));
}

bool validatePlainValue(const string_view value, const string_view field, string &error)
{
    if (value.empty())
    {
        error = FORMAT("The {} value must not be empty.", field);
        return false;
    }
    if (value.front() == '#')
    {
        error = FORMAT("The {} value must not start with '#', which denotes a comment.", field);
        return false;
    }
    if (value.front() == ' ' || value.front() == '\t')
    {
        error = FORMAT("The {} value must not have leading whitespace.", field);
        return false;
    }
    if (value.find_first_of("\r\n") != string_view::npos || value.find('\0') != string_view::npos)
    {
        error = FORMAT("The {} value contains a character that cannot be represented on one line.", field);
        return false;
    }
    return true;
}
} // namespace

ProjectCache::ProjectCache()
{
    sourceDirectoryPath = "..";
    const unsigned hardwareJobs = std::thread::hardware_concurrency();
    defaultJobs = static_cast<uint16_t>(std::clamp(hardwareJobs == 0 ? 1U : hardwareJobs, 1U, 65535U));

    lines_.push_back({LineKind::BLANK_OR_COMMENT,
                      "# Project source directory. Relative paths are resolved from the build directory."});
    lines_.push_back({LineKind::SOURCE_DIRECTORY, {}});
    lines_.push_back({LineKind::BLANK_OR_COMMENT, {}});
    lines_.push_back({LineKind::BLANK_OR_COMMENT, "# Selected toolchain name."});
    lines_.push_back({LineKind::TOOLCHAIN, {}});
    lines_.push_back({LineKind::BLANK_OR_COMMENT, {}});
    lines_.push_back({LineKind::BLANK_OR_COMMENT, "# Default number of concurrent build jobs."});
    lines_.push_back({LineKind::DEFAULT_JOBS, {}});
}

bool ProjectCache::validateVariableName(const string_view name, string &error)
{
    if (name.empty())
    {
        error = "A cache variable name must not be empty.";
        return false;
    }
    if (name.front() == '#' || name.find_first_of("=\r\n") != string_view::npos ||
        name.find('\0') != string_view::npos)
    {
        error = FORMAT("Invalid cache variable name.\nVariable: {}", name);
        return false;
    }
    if (name.front() == ' ' || name.front() == '\t' || name.back() == ' ' || name.back() == '\t')
    {
        error = FORMAT("Cache variable names must not have leading or trailing whitespace.\nVariable: {}", name);
        return false;
    }
    return true;
}

bool ProjectCache::parseVariableValue(const string_view text, VariableValue &value, string &error)
{
    if (text == "true")
    {
        value = true;
        return true;
    }
    if (text == "false")
    {
        value = false;
        return true;
    }
    if (!text.empty() && text.front() == '"')
    {
        if (text.size() < 2 || text.back() != '"')
        {
            error = "A string cache-variable value must end with an unescaped double quote.";
            return false;
        }

        string decoded;
        decoded.reserve(text.size() - 2);
        for (uint64_t index = 1; index + 1 < text.size(); ++index)
        {
            const unsigned char character = static_cast<unsigned char>(text[index]);
            if (character < 0x20)
            {
                error = "A string cache-variable value contains an unescaped control character.";
                return false;
            }
            if (character != '\\')
            {
                if (character == '"')
                {
                    error = "A string cache-variable value contains an unescaped double quote.";
                    return false;
                }
                decoded.push_back(static_cast<char>(character));
                continue;
            }

            if (++index + 1 >= text.size())
            {
                error = "A string cache-variable value ends with an incomplete escape sequence.";
                return false;
            }
            switch (text[index])
            {
            case '\\':
                decoded.push_back('\\');
                break;
            case '"':
                decoded.push_back('"');
                break;
            case 'n':
                decoded.push_back('\n');
                break;
            case 'r':
                decoded.push_back('\r');
                break;
            case 't':
                decoded.push_back('\t');
                break;
            case 'b':
                decoded.push_back('\b');
                break;
            case 'f':
                decoded.push_back('\f');
                break;
            case 'x':
            {
                if (index + 3 >= text.size())
                {
                    error = "A hexadecimal cache-variable string escape requires two digits.";
                    return false;
                }
                const auto hexDigit = [](const char digit) -> int {
                    if (digit >= '0' && digit <= '9')
                    {
                        return digit - '0';
                    }
                    if (digit >= 'a' && digit <= 'f')
                    {
                        return digit - 'a' + 10;
                    }
                    if (digit >= 'A' && digit <= 'F')
                    {
                        return digit - 'A' + 10;
                    }
                    return -1;
                };
                const int high = hexDigit(text[index + 1]);
                const int low = hexDigit(text[index + 2]);
                if (high < 0 || low < 0)
                {
                    error = "A hexadecimal cache-variable string escape contains a non-hexadecimal digit.";
                    return false;
                }
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                break;
            }
            default:
                error = FORMAT("Unsupported cache-variable string escape.\nEscape: \\{}", text[index]);
                return false;
            }
        }
        value = std::move(decoded);
        return true;
    }

    int parsedInteger = 0;
    const auto [end, parseError] = std::from_chars(text.data(), text.data() + text.size(), parsedInteger, 10);
    if (parseError != std::errc{} || end != text.data() + text.size())
    {
        error = "A cache-variable value must be true, false, a decimal integer, or an escaped quoted string.";
        return false;
    }
    value = parsedInteger;
    return true;
}

string ProjectCache::encodeVariableValue(const VariableValue &value)
{
    if (const bool *boolean = std::get_if<bool>(&value))
    {
        return *boolean ? "true" : "false";
    }
    if (const int *integer = std::get_if<int>(&value))
    {
        return std::to_string(*integer);
    }

    string encoded;
    const string &plain = std::get<string>(value);
    encoded.reserve(plain.size() + 2);
    encoded.push_back('"');
    for (const char character : plain)
    {
        switch (character)
        {
        case '\\':
            encoded += "\\\\";
            break;
        case '"':
            encoded += "\\\"";
            break;
        case '\n':
            encoded += "\\n";
            break;
        case '\r':
            encoded += "\\r";
            break;
        case '\t':
            encoded += "\\t";
            break;
        case '\b':
            encoded += "\\b";
            break;
        case '\f':
            encoded += "\\f";
            break;
        default:
        {
            const unsigned char byte = static_cast<unsigned char>(character);
            if (byte < 0x20 || byte == 0x7f)
            {
                constexpr char hexadecimal[] = "0123456789ABCDEF";
                encoded += "\\x";
                encoded.push_back(hexadecimal[byte >> 4]);
                encoded.push_back(hexadecimal[byte & 0x0f]);
            }
            else
            {
                encoded.push_back(character);
            }
            break;
        }
        }
    }
    encoded.push_back('"');
    return encoded;
}

string_view ProjectCache::variableTypeName(const VariableValue &value)
{
    if (std::holds_alternative<bool>(value))
    {
        return "bool";
    }
    if (std::holds_alternative<int>(value))
    {
        return "int";
    }
    return "string";
}

bool ProjectCache::parse(string_view contents, string &error)
{
    error.clear();
    vector<Line> parsedLines;
    parsedLines.reserve(lines_.size());
    string parsedSourceDirectory;
    string parsedToolchain;
    uint16_t parsedDefaultJobs = 0;
    uint8_t positionalValueCount = 0;

    while (!contents.empty())
    {
        const uint64_t lineSize = std::min(contents.find('\n'), contents.size());
        string_view line = contents.substr(0, lineSize);
        contents.remove_prefix(lineSize + (lineSize != contents.size()));
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        if (line.find('\r') != string_view::npos || line.find('\0') != string_view::npos)
        {
            error = FORMAT("A project-cache line contains an invalid control character.\nLine: {}", parsedLines.size() + 1);
            return false;
        }

        if (line.empty() || line.front() == '#')
        {
            parsedLines.push_back({LineKind::BLANK_OR_COMMENT, string(line)});
            continue;
        }
        if (line.front() == ' ' || line.front() == '\t')
        {
            error = FORMAT("Project-cache lines must not have leading whitespace.\nLine: {}", parsedLines.size() + 1);
            return false;
        }
        if (positionalValueCount == 0)
        {
            parsedSourceDirectory = line;
            parsedLines.push_back({LineKind::SOURCE_DIRECTORY, {}});
            ++positionalValueCount;
            continue;
        }
        if (positionalValueCount == 1)
        {
            parsedToolchain = line;
            parsedLines.push_back({LineKind::TOOLCHAIN, {}});
            ++positionalValueCount;
            continue;
        }
        if (positionalValueCount == 2)
        {
            const auto [end, parseError] =
                std::from_chars(line.data(), line.data() + line.size(), parsedDefaultJobs, 10);
            if (parseError != std::errc{} || end != line.data() + line.size() || parsedDefaultJobs == 0)
            {
                error = FORMAT("The default job count must be a decimal integer in 1..65535.\nValue: {}", line);
                return false;
            }
            parsedLines.push_back({LineKind::DEFAULT_JOBS, {}});
            ++positionalValueCount;
            continue;
        }

        const uint64_t equals = line.find('=');
        if (equals == string_view::npos)
        {
            error = FORMAT("A cache-variable line must use name=value syntax.\nLine: {}", line);
            return false;
        }
        const string_view name = line.substr(0, equals);
        const string_view valueText = line.substr(equals + 1);
        if (!validateVariableName(name, error))
        {
            return false;
        }
        VariableValue value;
        if (!parseVariableValue(valueText, value, error))
        {
            error = FORMAT("{}\nVariable: {}", error, name);
            return false;
        }
        if (std::ranges::any_of(parsedLines, [name](const Line &parsedLine) {
                if (parsedLine.kind != LineKind::VARIABLE)
                {
                    return false;
                }
                const uint64_t parsedEquals = parsedLine.text.find('=');
                return string_view(parsedLine.text).substr(0, parsedEquals) == name;
            }))
        {
            error = FORMAT("A cache variable is defined more than once.\nVariable: {}", name);
            return false;
        }
        parsedLines.push_back({LineKind::VARIABLE, string(line)});
    }

    if (positionalValueCount != 3)
    {
        error = FORMAT("The project cache requires source-directory, toolchain, and default-jobs values.\n"
                       "Values found: {}",
                       positionalValueCount);
        return false;
    }
    sourceDirectoryPath = std::move(parsedSourceDirectory);
    toolchainName = std::move(parsedToolchain);
    defaultJobs = parsedDefaultJobs;
    lines_ = std::move(parsedLines);
    return true;
}

bool ProjectCache::serialize(string &contents, string &error) const
{
    error.clear();
    if (!validatePlainValue(sourceDirectoryPath, "source-directory", error) ||
        !validatePlainValue(toolchainName, "toolchain", error))
    {
        return false;
    }
    if (defaultJobs == 0)
    {
        error = "The default job count must be in 1..65535.";
        return false;
    }

    contents.clear();
    uint64_t estimatedSize = sourceDirectoryPath.size() + toolchainName.size() + 32;
    for (const Line &line : lines_)
    {
        estimatedSize += line.text.size() + 1;
    }
    contents.reserve(estimatedSize);

    for (const Line &line : lines_)
    {
        switch (line.kind)
        {
        case LineKind::BLANK_OR_COMMENT:
            contents += line.text;
            break;
        case LineKind::SOURCE_DIRECTORY:
            contents += sourceDirectoryPath;
            break;
        case LineKind::TOOLCHAIN:
            contents += toolchainName;
            break;
        case LineKind::DEFAULT_JOBS:
            contents += std::to_string(defaultJobs);
            break;
        case LineKind::VARIABLE:
            contents += line.text;
            break;
        }
        contents.push_back('\n');
    }
    return true;
}

uint64_t ProjectCache::contentCache() const
{
    STACK_PMR_STRING(semantic, 4 * 1024)
    semantic.reserve(sourceDirectoryPath.size() + toolchainName.size() + lines_.size() * 8);
    semantic += "source";
    semantic.push_back('\0');
    semantic.append(sourceDirectoryPath);
    semantic.push_back('\0');
    semantic += "toolchain";
    semantic.push_back('\0');
    semantic.append(toolchainName);
    for (const Line &line : lines_)
    {
        if (line.kind == LineKind::VARIABLE)
        {
            semantic.push_back('\0');
            semantic += "variable";
            semantic.push_back('\0');
            semantic += line.text;
        }
    }
    return rapidhash(semantic.data(), semantic.size());
}

void ProjectCache::appendVariable(const string_view name, const VariableValue &value)
{
    if (!lines_.empty() && !(lines_.back().kind == LineKind::BLANK_OR_COMMENT && lines_.back().text.empty()))
    {
        lines_.push_back({LineKind::BLANK_OR_COMMENT, {}});
    }
    lines_.push_back({LineKind::BLANK_OR_COMMENT,
                      FORMAT("# Cache variable '{}'. Values are true, false, decimal, or quoted.", name)});
    const string line = FORMAT("{}={}", name, encodeVariableValue(value));
    lines_.push_back({LineKind::VARIABLE, line});
}

void ProjectCache::initializeFromCacheFile()
{
    const path filePath = path(configureNode->filePath) / projectCacheFileName;
    std::error_code error;
    const bool cacheExists = std::filesystem::exists(filePath, error);
    if (error)
    {
        printErrorMessage(FORMAT("Could not inspect the project cache.\nFile: {}\nSystem error: {}",
                                 filePath.string(), error.message()));
    }
    if (cacheExists)
    {
        const string contents = fileToString(filePath.string());
        string error;
        if (!parse(contents, error))
        {
            projectCacheError(filePath, error);
        }
    }

    path sourcePath(sourceDirectoryPath);
    if (sourcePath.is_relative())
    {
        sourcePath = path(configureNode->filePath) / sourcePath;
    }
    sourcePath = sourcePath.lexically_normal();
    srcNode = Node::getHalfNode(sourcePath.string());
    normalizationBasePath = srcNode->filePath;
}

void ProjectCache::writeToCacheFile() const
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const path filePath = path(configureNode->filePath) / projectCacheFileName;
        string contents;
        string error;
        if (!serialize(contents, error))
        {
            projectCacheError(filePath, error);
        }
        writeCacheFile(filePath.string(), contents);
    }
}
