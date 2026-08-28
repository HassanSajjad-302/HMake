#ifndef HMAKE_CACHE_HPP
#define HMAKE_CACHE_HPP

#include "BuildSystemFunctions.hpp"

#include <algorithm>
#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>

inline constexpr string_view projectCacheFileName = "cache.txt";

/// Parsed, order-preserving representation of the user-editable project cache.
///
/// Lines must begin at column zero; `#` starts a comment and only empty lines are blank. The first three non-comment
/// lines are the source directory, toolchain, and default job count. Remaining value lines are uniquely named
/// `name=value` cache variables. Comments, blank lines, and variable order are retained when the file is rewritten.
struct ProjectCache
{
    string sourceDirectoryPath;
    /// Project toolchain selected by hbuild.
    string toolchainName;
    uint16_t defaultJobs;

    ProjectCache();

    /// Parses CRLF/LF text without changing this object when validation fails.
    [[nodiscard]] bool parse(string_view contents, string &error);
    /// Serializes the retained layout with normalized LF line endings and one final newline.
    [[nodiscard]] bool serialize(string &contents, string &error) const;
    /// Hashes graph-affecting cache values in their semantic order. Comments, blank lines, and default jobs are
    /// deliberately excluded.
    [[nodiscard]] uint64_t contentCache() const;

    /// Generated-runtime wrappers around the shared text parser/serializer.
    void initializeFromCacheFile();
    void writeToCacheFile() const;

    template <typename T> T getOrAddVariable(string_view name, T defaultValue);

  private:
    using VariableValue = std::variant<bool, int, string>;

    enum class LineKind : uint8_t
    {
        BLANK_OR_COMMENT,
        SOURCE_DIRECTORY,
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

    static bool validateVariableName(string_view name, string &error);
    static bool parseVariableValue(string_view text, VariableValue &value, string &error);
    static string encodeVariableValue(const VariableValue &value);
    static string_view variableTypeName(const VariableValue &value);
    void appendVariable(string_view name, const VariableValue &value);
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

    const auto existing = std::ranges::find_if(lines_, [name](const Line &line) {
        if (line.kind != LineKind::VARIABLE)
        {
            return false;
        }
        const uint64_t equals = line.text.find('=');
        return string_view(line.text).substr(0, equals) == name;
    });
    if (existing != lines_.end())
    {
        const uint64_t equals = existing->text.find('=');
        VariableValue storedValue;
        string parseError;
        const bool parsed = parseVariableValue(string_view(existing->text).substr(equals + 1), storedValue, parseError);
        assert(parsed && parseError.empty());
        if (!std::holds_alternative<T>(storedValue))
        {
            constexpr string_view expectedType = [] {
                if constexpr (std::is_same_v<T, bool>)
                {
                    return string_view("bool");
                }
                else if constexpr (std::is_same_v<T, int>)
                {
                    return string_view("int");
                }
                else
                {
                    return string_view("string");
                }
            }();
            printErrorMessage(FORMAT("Cache variable has the wrong type.\nVariable: {}\nExpected: {}\nActual: {}",
                                     name, expectedType, variableTypeName(storedValue)));
        }
        return std::get<T>(storedValue);
    }

    const VariableValue storedDefault = defaultValue;
    appendVariable(name, storedDefault);
    return defaultValue;
}

#endif // HMAKE_CACHE_HPP
