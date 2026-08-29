#include "ParseHeaderDeps.hpp"

#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Node.hpp"

#include <rapidjson/document.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <utility>

namespace
{
struct HeaderDepsParser
{
    std::string &output;
    const std::string &dependencyFile;
    std::string_view workingDirectory;
    const Node *compiledSource;
    bool excludeHeadersInConfigureNode;
    std::filesystem::path workingDirectoryPath;
    gtl::flat_hash_set<Node *> dependencies;

    Node *dependencyNode(const std::string_view dependency)
    {
        if (dependency.empty())
        {
            return nullptr;
        }

        Node *node;
        if (Node::isAbsolute(dependency))
        {
            node = Node::getHalfNodeNonNormalized(dependency);
        }
        else
        {
            if (workingDirectoryPath.empty())
            {
                workingDirectoryPath = std::filesystem::path{workingDirectory};
            }
            // Both callers supply an absolute working directory, so dependency normalization stays purely lexical.
            std::filesystem::path dependencyPath = workingDirectoryPath;
            dependencyPath /= dependency;
            std::string normalized = dependencyPath.lexically_normal().string();
            lowerCaseOnWindows(normalized.data(), normalized.size());
            node = Node::getHalfNode(normalized);
        }

        if (node == compiledSource ||
            (excludeHeadersInConfigureNode && isPathInDirectory(node->filePath, configureNode->filePath)))
        {
            return nullptr;
        }
        return node;
    }

    void parseShowIncludes(const bool isClang, const bool collectHeaders)
    {
        constexpr std::string_view includeFileNote = "Note: including file:";
        const uint64_t outputSize = output.size();
        uint64_t readOffset = 0;
        if (collectHeaders && !isClang)
        {
            const uint64_t firstLineEnd = output.find('\n');
            if (firstLineEnd == std::string::npos)
            {
                return;
            }
            readOffset = firstLineEnd + 1;
        }

        char *const data = output.data();
        uint64_t writeOffset = 0;
        while (readOffset < outputSize)
        {
            const char *const newline =
                static_cast<const char *>(std::memchr(data + readOffset, '\n', outputSize - readOffset));
            const uint64_t nextOffset =
                newline == nullptr ? outputSize : static_cast<uint64_t>(newline - data) + 1;
            const std::string_view line(data + readOffset, nextOffset - readOffset);
            const uint64_t notePosition = line.find(includeFileNote);

            if (notePosition == std::string_view::npos)
            {
                const uint64_t lineSize = nextOffset - readOffset;
                if (writeOffset != readOffset)
                {
                    std::memmove(data + writeOffset, data + readOffset, lineSize);
                }
                writeOffset += lineSize;
            }
            else if (collectHeaders)
            {
                uint64_t headerStart = notePosition + includeFileNote.size();
                while (headerStart < line.size() && (line[headerStart] == ' ' || line[headerStart] == '\t'))
                {
                    ++headerStart;
                }
                uint64_t headerEnd = line.size();
                while (headerEnd > headerStart && (line[headerEnd - 1] == '\n' || line[headerEnd - 1] == '\r' ||
                                                   line[headerEnd - 1] == ' ' || line[headerEnd - 1] == '\t'))
                {
                    --headerEnd;
                }
                if (headerStart != headerEnd)
                {
                    const std::string_view headerView(data + readOffset + headerStart, headerEnd - headerStart);
                    if (Node *header = dependencyNode(headerView))
                    {
                        dependencies.emplace(header);
                    }
                }
            }
            readOffset = nextOffset;
        }
        output.resize(writeOffset);
    }

    void parseMakeDependencies()
    {
        constexpr auto isWhitespace = [](const char character) {
            return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
                   character == '\f' || character == '\v';
        };
        STACK_PMR_STRING(contents, 256 * 1024)
        fileToString(dependencyFile, contents);
        uint64_t writeOffset = 0;
        const uint64_t contentStart = contents.starts_with("\xef\xbb\xbf") ? 3 : 0;
        for (uint64_t index = contentStart; index < contents.size(); ++index)
        {
            if (contents[index] == '\\' && index + 1 < contents.size() &&
                (contents[index + 1] == '\n' || contents[index + 1] == '\r'))
            {
                ++index;
                if (contents[index] == '\r' && index + 1 < contents.size() && contents[index + 1] == '\n')
                {
                    ++index;
                }
                contents[writeOffset++] = ' ';
                continue;
            }
            contents[writeOffset++] = contents[index];
        }
        contents.resize(writeOffset);

        uint64_t colon = std::string::npos;
        bool escaped = false;
        for (uint64_t index = 0; index < contents.size(); ++index)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (contents[index] == '\\')
            {
                escaped = true;
                continue;
            }
            if (contents[index] == ':' &&
                (index + 1 == contents.size() || isWhitespace(contents[index + 1])))
            {
                colon = index;
                break;
            }
        }
        if (colon == std::string::npos)
        {
            printErrorMessage(FORMAT("Malformed Make dependency file.\nFile: {}", dependencyFile));
        }

        STACK_PMR_STRING(token, 1024)
        bool comment = false;
        const auto commit = [&]() {
            if (!token.empty())
            {
                if (Node *dependency = dependencyNode(token))
                {
                    dependencies.emplace(dependency);
                }
                token.clear();
            }
        };
        for (uint64_t index = colon + 1; index <= contents.size(); ++index)
        {
            const char character = index == contents.size() ? ' ' : contents[index];
            if (comment)
            {
                if (character == '\n' || character == '\r')
                {
                    comment = false;
                }
                continue;
            }
            if (character == '#')
            {
                commit();
                comment = true;
                continue;
            }
            if (isWhitespace(character))
            {
                commit();
                continue;
            }
            if (character == '\\' && index + 1 < contents.size())
            {
                const char next = contents[index + 1];
                if (isWhitespace(next) || next == '#' || next == ':' || next == '\\')
                {
                    token.push_back(next);
                    ++index;
                    continue;
                }
            }
            token.push_back(character);
        }
    }

    void collectSourceDependencyPaths(const rapidjson::Value &value, const std::string_view memberName)
    {
        if (value.IsObject())
        {
            for (auto iterator = value.MemberBegin(); iterator != value.MemberEnd(); ++iterator)
            {
                collectSourceDependencyPaths(iterator->value,
                                             {iterator->name.GetString(), iterator->name.GetStringLength()});
            }
            return;
        }
        if (value.IsArray())
        {
            for (const rapidjson::Value &element : value.GetArray())
            {
                if (element.IsString() && memberName == "Includes")
                {
                    if (Node *dependency =
                            dependencyNode({element.GetString(), element.GetStringLength()}))
                    {
                        dependencies.emplace(dependency);
                    }
                }
                else
                {
                    collectSourceDependencyPaths(element, memberName);
                }
            }
            return;
        }
        if (value.IsString() && (memberName == "Source" || memberName == "Header" || memberName == "Path"))
        {
            if (Node *dependency = dependencyNode({value.GetString(), value.GetStringLength()}))
            {
                dependencies.emplace(dependency);
            }
        }
    }

    void parseSourceDependencies()
    {
        STACK_PMR_STRING(json, 256 * 1024)
        fileToString(dependencyFile, json);
        char *const documentStart = json.data() + (json.starts_with("\xef\xbb\xbf") ? 3 : 0);
        rapidjson::Document document;
        document.ParseInsitu(documentStart);
        if (document.HasParseError() || !document.IsObject())
        {
            printErrorMessage(FORMAT("Malformed MSVC source-dependencies file.\nFile: {}\nByte offset: {}",
                                     dependencyFile, document.GetErrorOffset()));
        }
        collectSourceDependencyPaths(document, {});
    }
};
} // namespace

gtl::flat_hash_set<Node *> parseHeaderDeps(std::string &output, const Compiler &compiler, const int exitStatus,
                                           const std::string &dependencyFile,
                                           const std::string_view workingDirectory, const Node *compiledSource,
                                           const bool excludeHeadersInConfigureNode)
{
    HeaderDepsParser parser{output, dependencyFile, workingDirectory, compiledSource,
                            excludeHeadersInConfigureNode};
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        if (dependencyFile.empty())
        {
            parser.parseShowIncludes(compiler.btSubFamily == BTSubFamily::CLANG, exitStatus == EXIT_SUCCESS);
        }
        else if (exitStatus == EXIT_SUCCESS)
        {
            parser.parseSourceDependencies();
        }
    }
    else if (exitStatus == EXIT_SUCCESS)
    {
        parser.parseMakeDependencies();
    }
    return std::move(parser.dependencies);
}
