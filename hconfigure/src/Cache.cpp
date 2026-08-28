#include "Cache.hpp"

#include "rapidhash/rapidhash.h"

#include <algorithm>
#include <charconv>
#include <thread>

ProjectCache::ProjectCache()
{
    sourceDirectoryPath = "..";
    const unsigned hardwareJobs = std::thread::hardware_concurrency();
    defaultJobs = static_cast<uint16_t>(std::clamp(hardwareJobs == 0 ? 1U : hardwareJobs, 1U, 65535U));

    lines_.reserve(8);
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
    const char firstCharacter = name.front();
    if (!((firstCharacter >= 'A' && firstCharacter <= 'Z') ||
          (firstCharacter >= 'a' && firstCharacter <= 'z') || firstCharacter == '_'))
    {
        error = FORMAT("A cache variable name must match [A-Za-z_][A-Za-z0-9_]*.\nVariable: {}", name);
        return false;
    }
    for (uint64_t index = 1; index < name.size(); ++index)
    {
        const char character = name[index];
        const bool letter = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';
        if (!letter && !digit && character != '_')
        {
            error = FORMAT("A cache variable name must match [A-Za-z_][A-Za-z0-9_]*.\nVariable: {}", name);
            return false;
        }
    }
    return true;
}

bool ProjectCache::parse(string_view contents, string &error)
{
    error.clear();
    vector<Line> parsedLines;
    parsedLines.reserve(lines_.size());
    gtl::flat_hash_map<string, uint64_t> parsedVariableLines;
    parsedVariableLines.reserve(variableLines_.size());
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
        if (!validateVariableName(name, error))
        {
            return false;
        }
        if (!parsedVariableLines.try_emplace(string(name), static_cast<uint64_t>(parsedLines.size())).second)
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
    variableLines_ = std::move(parsedVariableLines);
    return true;
}

bool ProjectCache::serialize(string &contents, string &error) const
{
    error.clear();
    const auto validatePlainValue = [&error](const string_view value, const string_view field) {
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
    };
    if (!validatePlainValue(sourceDirectoryPath, "source-directory") || !validatePlainValue(toolchainName, "toolchain"))
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
    uint64_t semanticSize = sourceDirectoryPath.size() + toolchainName.size() + 18;
    for (const Line &line : lines_)
    {
        if (line.kind == LineKind::VARIABLE)
        {
            semanticSize += line.text.size() + 10;
        }
    }

    STACK_PMR_STRING(semantic, 4 * 1024)
    semantic.reserve(semanticSize);
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
