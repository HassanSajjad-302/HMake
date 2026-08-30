#ifndef HMAKE_CACHE_HPP
#define HMAKE_CACHE_HPP

#include "BuildSystemFunctions.hpp"

#include <charconv>
#include <type_traits>
#include <utility>

inline constexpr string_view projectCacheFileName = "cache.txt";

/// Parsed, order-preserving representation of the user-editable project cache.
///
/// Lines must begin at column zero; `#` starts a comment and only empty lines are blank. The first two non-comment
/// lines are the toolchain and default job count. Remaining value lines are uniquely named
/// `name=value` cache variables. String values use outermost double quotes, but their contents remain literal.
/// Comments, blank lines, and variable order are retained when the file is rewritten.
struct ProjectCache
{
    /// Project toolchain selected by hbuild.
    string toolchainName;
    uint16_t defaultJobs;
    /// Set for a default cache, direct field changes, and newly appended variables; cleared after parsing or writing.
    bool needsWrite = true;

    ProjectCache();

    /// Parses CRLF/LF text without changing this object when validation fails.
    [[nodiscard]] bool parse(string_view contents, string &error);
    /// Serializes the retained layout with normalized LF line endings and one final newline.
    [[nodiscard]] bool serialize(std::pmr::string &contents, string &error) const;
    /// Hashes graph-affecting cache values in their semantic order. Comments, blank lines, and default jobs are
    /// deliberately excluded.
    [[nodiscard]] uint64_t contentCache() const;

    template <typename T> T getOrAddVariable(string_view name, T defaultValue);

  private:
    enum class LineKind : uint8_t
    {
        BLANK_OR_COMMENT,
        TOOLCHAIN,
        DEFAULT_JOBS,
        VARIABLE,
    };

    struct Line
    {
        LineKind kind;
        string text;
    };

    vector<Line> lines_;
    gtl::flat_hash_map<string, uint64_t> variableLines_;

    static bool validateVariableName(string_view name, string &error);
};

GLOBAL_VARIABLE(ProjectCache, projectCache)

template <typename T> struct CacheVariable
{
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, string>,
                  "CacheVariable supports bool, int, and std::string values.");

    T value;

    CacheVariable(const string_view name, T defaultValue)
        : value(projectCache.getOrAddVariable<T>(name, std::move(defaultValue)))
    {
    }
};

CacheVariable(string_view, bool) -> CacheVariable<bool>;
CacheVariable(string_view, int) -> CacheVariable<int>;
CacheVariable(string_view, string) -> CacheVariable<string>;
CacheVariable(string_view, const char *) -> CacheVariable<string>;

template <typename T> T ProjectCache::getOrAddVariable(const string_view name, T defaultValue)
{
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, string>,
                  "CacheVariable supports bool, int, and std::string values.");

    string validationError;
    if (!validateVariableName(name, validationError))
    {
        printErrorMessage(validationError);
    }

    const auto existing = variableLines_.find(name);
    if (existing != variableLines_.end())
    {
        const string &line = lines_[existing->second].text;
        const string_view value = string_view(line).substr(name.size() + 1);
        if constexpr (std::is_same_v<T, bool>)
        {
            if (value == "true")
            {
                return true;
            }
            if (value == "false")
            {
                return false;
            }
            printErrorMessage(FORMAT("A bool cache variable must be exactly true or false.\nVariable: {}\nValue: {}",
                                     name, value));
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            int result = 0;
            const auto [end, parseError] = std::from_chars(value.data(), value.data() + value.size(), result, 10);
            if (parseError != std::errc{} || end != value.data() + value.size())
            {
                printErrorMessage(
                    FORMAT("An int cache variable must be a decimal integer.\nVariable: {}\nValue: {}", name, value));
            }
            return result;
        }
        else
        {
            if (value.size() < 2 || value.front() != '"' || value.back() != '"')
            {
                printErrorMessage(
                    FORMAT("A string cache variable must use outermost double quotes.\nVariable: {}\nValue: {}", name,
                           value));
            }
            return string(value.substr(1, value.size() - 2));
        }
    }

    string variableName(name);
    string variableLine;
    if constexpr (std::is_same_v<T, string>)
    {
        variableLine.reserve(variableName.size() + defaultValue.size() + 3);
    }
    else
    {
        variableLine.reserve(variableName.size() + 12);
    }
    variableLine += variableName;
    variableLine.push_back('=');
    if constexpr (std::is_same_v<T, bool>)
    {
        variableLine += defaultValue ? "true" : "false";
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        variableLine += std::to_string(defaultValue);
    }
    else
    {
        if (defaultValue.find_first_of("\r\n") != string::npos || defaultValue.find('\0') != string::npos)
        {
            printErrorMessage(FORMAT("A string cache-variable default must fit on one line and must not contain a null "
                                     "byte.\nVariable: {}",
                                     name));
        }
        variableLine.push_back('"');
        variableLine += defaultValue;
        variableLine.push_back('"');
    }

    if (!lines_.empty() && !(lines_.back().kind == LineKind::BLANK_OR_COMMENT && lines_.back().text.empty()))
    {
        lines_.push_back({LineKind::BLANK_OR_COMMENT, {}});
    }
    lines_.push_back({LineKind::BLANK_OR_COMMENT,
                      FORMAT("# Cache variable '{}'. Values are true, false, decimal, or quoted raw strings.",
                             variableName)});
    const uint64_t lineIndex = static_cast<uint64_t>(lines_.size());
    lines_.push_back({LineKind::VARIABLE, std::move(variableLine)});
    variableLines_.emplace(std::move(variableName), lineIndex);
    needsWrite = true;
    return defaultValue;
}

#endif // HMAKE_CACHE_HPP
