
#ifdef USE_HEADER_UNITS
import <atomic>;
import "rapidhash/rapidhash.h";
import "Node.hpp";
import <mutex>;
#else

#include "Node.hpp"
#include "rapidhash/rapidhash.h"
#include <mutex>
#include <utility>
#endif

using std::filesystem::directory_entry, std::filesystem::file_type, std::filesystem::file_time_type, std::lock_guard,
    std::mutex, std::atomic_ref;

string getStatusPString(const path &p)
{
    switch (status(p).type())
    {
    case file_type::none:
        return " has `not-evaluated-yet` type";
    case file_type::not_found:
        return " does not exist";
    case file_type::regular:
        return " is a regular file";
    case file_type::directory:
        return " is a directory";
    case file_type::symlink:
        return " is a symlink";
    case file_type::block:
        return " is a block device";
    case file_type::character:
        return " is a character device";
    case file_type::fifo:
        return " is a named IPC pipe";
    case file_type::socket:
        return " is a named IPC socket";
    case file_type::unknown:
        return " has `unknown` type";
    default:
        return " has `implementation-defined` type";
    }
}

bool NodeEqual::operator()(const Node &lhs, const Node &rhs) const
{
    return lhs.filePath == rhs.filePath;
}

bool NodeEqual::operator()(const Node &lhs, const string_view &rhs) const
{
    return lhs.filePath == rhs;
}

bool NodeEqual::operator()(const string_view &lhs, const Node &rhs) const
{
    return lhs == rhs.filePath;
}

std::size_t NodeHash::operator()(const Node &node) const
{
    return rapidhash(node.filePath.c_str(), node.filePath.size());
}

std::size_t NodeHash::operator()(const string_view &str) const
{
    return rapidhash(str.data(), str.size());
}

Node::Node(Node *&node, string filePath_) : filePath(std::move(filePath_))
{
    node = this;
    myId = idCount.fetch_add(1);
    nodeIndices[myId] = this;
    ++idCountCompleted;
}

// This function is called single-threaded. While the above is called multithreaded in lambdas passed to nodeAllFiles
// emplace functions.
Node::Node(string filePath_) : filePath(std::move(filePath_))
{
    myId = reinterpret_cast<uint32_t &>(idCount)++;
    nodeIndices[myId] = this;
    ++reinterpret_cast<uint32_t &>(idCountCompleted);
}

string Node::getFileName() const
{
    return {filePath.begin() + filePath.find_last_of(slashc) + 1, filePath.end()};
}

Value Node::getValue() const
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    return Value(myId);
#else
    return Value(svtogsr(filePath));
#endif
}

// TODO
// See if we can use new functions with absolute paths. So, only lexically_normal is called.
path Node::getFinalNodePathFromPath(path filePath)
{
    if (filePath.is_relative())
    {
        filePath = path(srcNode->filePath) / filePath;
    }
    filePath = filePath.lexically_normal();

    if constexpr (os == OS::NT)
    {
        // TODO
        //  This is illegal
        //  TODO
        //  Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        //  actual paths. In Windows paths could be case-insensitive. Just another wrinkle hahaha.
        for (auto it = const_cast<path::value_type *>(filePath.c_str()); *it != '\0'; ++it)
        {
            *it = std::tolower(*it);
        }
    }
    return filePath;
}

void Node::ensureSystemCheckCalled(const bool isFile, const bool mayNotExist)
{
    if (atomic_ref(systemCheckCompleted).load())
    {
        return;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!atomic_ref(systemCheckCalled).exchange(true))
    {
        performSystemCheck(isFile, mayNotExist);
        atomic_ref(systemCheckCompleted).store(true);
        return;
    }

    // systemCheck is being called for this node by another thread
    while (!atomic_ref(systemCheckCompleted).load())
    {
        // std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

bool Node::trySystemCheck(const bool isFile, const bool mayNotExist)
{
    // If systemCheck was not called previously or isn't being called, call it.
    if (!atomic_ref(systemCheckCalled).exchange(true))
    {
        performSystemCheck(isFile, mayNotExist);
        atomic_ref(systemCheckCompleted).store(true);
        return true;
    }

    if (atomic_ref(systemCheckCompleted).load())
    {
        return true;
    }

    return false;
}

Node *Node::getNodeFromNormalizedString(string p, const bool isFile, const bool mayNotExist)
{
    Node *node = nullptr;

    using Map = decltype(nodeAllFiles);

    if (nodeAllFiles.lazy_emplace_l(
            string_view(p), [&](const Map::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const Map::constructor &constructor) { constructor(node, p); }))
    {
    }

    node->ensureSystemCheckCalled(isFile, mayNotExist);
    return node;
}

Node *Node::getNodeFromNormalizedString(const string_view p, const bool isFile, const bool mayNotExist)
{
    Node *node = nullptr;

    using Map = decltype(nodeAllFiles);

    if (nodeAllFiles.lazy_emplace_l(
            p, [&](const Map::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const Map::constructor &constructor) { constructor(node, string(p)); }))
    {
    }

    node->ensureSystemCheckCalled(isFile, mayNotExist);
    return node;
}

Node *Node::getNodeFromNormalizedStringNoSystemCheckCalled(string_view p)
{
    Node *node = nullptr;

    using Map = decltype(nodeAllFiles);

    if (nodeAllFiles.lazy_emplace_l(
            p, [&](const Map::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const Map::constructor &constructor) { constructor(node, string(p)); }))
    {
    }

    return node;
}

Node *Node::getNodeFromNonNormalizedString(const string &p, const bool isFile, const bool mayNotExist)
{
    const path filePath = getFinalNodePathFromPath(p);
    return getNodeFromNormalizedString(filePath.string(), isFile, mayNotExist);
}

Node *Node::getNodeFromNormalizedPath(const path &p, const bool isFile, const bool mayNotExist)
{
    return getNodeFromNormalizedString(p.string(), isFile, mayNotExist);
}

Node *Node::getNodeFromNonNormalizedPath(const path &p, const bool isFile, const bool mayNotExist)
{
    const path filePath = getFinalNodePathFromPath(p);
    return getNodeFromNormalizedString(filePath.string(), isFile, mayNotExist);
}

Node *Node::addHalfNodeFromNormalizedStringSingleThreaded(string normalizedFilePath)
{
    return const_cast<Node *>(nodeAllFiles.emplace(std::move(normalizedFilePath)).first.operator->());
}

Node *Node::getHalfNodeFromNormalizedString(const string_view p)
{
    Node *node = nullptr;

    using Map = decltype(nodeAllFiles);

    if (nodeAllFiles.lazy_emplace_l(
            p, [&](const Map::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const Map::constructor &constructor) { constructor(node, string(p)); }))
    {
    }

    return node;
}

Node *Node::getNodeFromValue(const Value &value, bool isFile, bool mayNotExist)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    Node *node = nodeIndices[value.GetUint64()];
    node->ensureSystemCheckCalled(isFile, mayNotExist);
#else
    Node *node =
        getNodeFromNormalizedString(string_view(value.GetString(), value.GetStringLength()), isFile, mayNotExist);
#endif
    return node;
}

Node *Node::getHalfNodeFromValue(const Value &value)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    Node *node = nodeIndices[value.GetUint64()];
#else
    Node *node =
        getNodeFromNormalizedStringNoSystemCheckCalled(string_view(value.GetString(), value.GetStringLength()));
#endif
    return node;
}

Node *Node::tryGetNodeFromValue(bool &systemCheckSucceeded, const Value &value, bool isFile, bool mayNotExist)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    Node *node = nodeIndices[value.GetUint64()];
    systemCheckSucceeded = node->trySystemCheck(isFile, mayNotExist);
#else
    Node *node =
        Node::getNodeFromNormalizedString(string_view(value.GetString(), value.GetStringLength()), isFile, mayNotExist);
    systemCheckSucceeded = true;
#endif
    return node;
}

Node *Node::getLastNodeAdded()
{
    return nodeIndices[idCountCompleted - 1];
}

rapidjson::Type Node::getType()
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    return rapidjson::kNumberType;
#else
    return rapidjson::kStringType;
#endif
}

void Node::performSystemCheck(const bool isFile, const bool mayNotExist)
{
    // TODO
    // nodeAllFiles is initialized with 10000. This may not be enough. Either have a counter which is incremented here
    // which warns on 90% usage and errors out at 100% Or warn the user at the end if 80% is used. Or do both
    if (systemCheckCompleted)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }

    {
        if (const directory_entry entry = directory_entry(filePath);
            entry.status().type() == (isFile ? file_type::regular : file_type::directory))
        {
            lastWriteTime = entry.last_write_time();
        }
        else
        {
            if (!mayNotExist || entry.status().type() != file_type::not_found)
            {
                printErrorMessage(FORMAT("{} is not a {} file. File Type is {}\n", filePath, isFile ? "regular" : "dir",
                                         getStatusPString(filePath)));
            }
            doesNotExist = true;
        }
    }

    /////////////////////////////////
    /*WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFileEx((isFile ? filePath : filePath + "\\*").c_str(), FindExInfoBasic, &findFileData,
                                   FindExSearchNameMatch, nullptr, 0);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        if (mayNotExist)
        {
            doesNotExist = true;
        }
        else
        {
            printErrorMessage(FORMAT("FindFirstFileEx failed {}\n", GetLastError()));
            printErrorMessage(FORMAT("{} is not a {} file. File Type is {}\n", filePath,
                                          isFile ? "regular" : "dir", getStatusPString(filePath)));
        }
    }

    if (isFile)
    {
        auto [dwLowDateTime, dwHighDateTime] = findFileData.ftLastWriteTime;
        const uint64_t a = static_cast<__int64>(dwHighDateTime) << 32 | dwLowDateTime;
        lastWriteTime = file_time_type(file_time_type::duration(a));
    }
    else
    {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            auto [dwLowDateTime, dwHighDateTime] = findFileData.ftLastWriteTime;
            const uint64_t a = static_cast<__int64>(dwHighDateTime) << 32 | dwLowDateTime;
            lastWriteTime = file_time_type(file_time_type::duration(a));
        }
        else
        {
            printErrorMessage(FORMAT("FindFirstFileEx failed {}\n", GetLastError()));
            printErrorMessage(
                FORMAT("{} is not a dir file. File Type is {}\n", filePath, getStatusPString(filePath)));
        }
    }

    FindClose(hFind)*/
    ;
    /////////////////////////////////////
}

void Node::clearNodes()
{
    nodeAllFiles.clear();
    idCount = 0;
    idCountCompleted = 0;
    for (Node *&node : nodeIndices)
    {
        node = nullptr;
    }
}