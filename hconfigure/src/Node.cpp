#include "Node.hpp"
#include "TargetCache.hpp"
#include "rapidhash/rapidhash.h"
#include <mutex>
#include <utility>

#ifdef _WIN32
#include "Windows.h"
#endif

using std::filesystem::directory_entry, std::filesystem::file_type, std::filesystem::file_time_type, std::lock_guard,
    std::mutex, std::atomic_ref;

string getStatusString(const path &p)
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
    myId = atomic_ref(idCount).fetch_add(1, std::memory_order_relaxed);
    nodeIndices[myId] = this;
    atomic_ref(idCountCompleted).fetch_add(1, std::memory_order_release);
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

string Node::getFileStem() const
{
    return {filePath.begin() + filePath.find_last_of(slashc) + 1, filePath.begin() + filePath.find_last_of('.')};
}

// TODO
// See if we can use new functions with absolute paths. So, only lexically_normal is called.
static string getNormalizedPath(path filePath)
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
    return filePath.string();
}

void Node::performSystemCheck()
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &attrs))
    {
        DWORD win_err = GetLastError();
        if (win_err == ERROR_FILE_NOT_FOUND || win_err == ERROR_PATH_NOT_FOUND)
        {
            fileType = file_type::not_found;
            lastWriteTime = {}; // Default initialize
            return;
        }
        // Handle other errors - you might want to throw or set an error flag
        fileType = file_type::unknown;
        lastWriteTime = {};
        return;
    }

    // Set file type based on Windows attributes
    if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        fileType = file_type::directory;
    }
    else if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
    {
        fileType = file_type::character; // or block, depending on your needs
    }
    else
    {
        fileType = file_type::regular;
    }

    // Always set lastWriteTime for all file types (not just regular files)
    // Convert Windows FILETIME to std::filesystem::file_time_type
    ULARGE_INTEGER ull;
    ull.LowPart = attrs.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = attrs.ftLastWriteTime.dwHighDateTime;

    // Convert to std::chrono time point
    // Windows FILETIME is 100-nanosecond intervals since January 1, 1601
    const auto duration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>(ull.QuadPart);
    const auto windows_epoch = std::chrono::duration<int64_t, std::ratio<1, 10000000>>(116444736000000000LL);
    const auto unix_time = duration - windows_epoch;

    lastWriteTime = std::filesystem::file_time_type(unix_time);
#else
    const auto entry = directory_entry(filePath);
    fileType = entry.status().type();
    if (fileType == file_type::regular)
    {
        lastWriteTime = entry.last_write_time();
    }
#endif
}

void Node::ensureSystemCheckCalled(const bool isFile, const bool mayNotExist)
{
    if (isOneThreadRunning)
    {
        performSystemCheck();
        if (fileType != (isFile ? file_type::regular : file_type::directory) && !mayNotExist)
        {
            printErrorMessage(FORMAT("{} is not a {} file. File Type is {}\n", filePath, isFile ? "regular" : "dir",
                                     getStatusString(filePath)));
        }
        systemCheckCalled = true;
        systemCheckCompleted = true;
        return;
    }

    if (systemCheckCompleted || atomic_ref(systemCheckCompleted).load())
    {
        return;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!atomic_ref(systemCheckCalled).exchange(true))
    {
        performSystemCheck();
        if (fileType != (isFile ? file_type::regular : file_type::directory) && !mayNotExist)
        {
            printErrorMessage(FORMAT("{} is not a {} file. File Type is {}\n", filePath, isFile ? "regular" : "dir",
                                     getStatusString(filePath)));
        }
        atomic_ref(systemCheckCompleted).store(true);
        return;
    }

    // systemCheck is being called for this node by another thread
    while (!atomic_ref(systemCheckCompleted).load())
    {
        // std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
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

Node *Node::getNodeFromNonNormalizedString(const string &p, const bool isFile, const bool mayNotExist)
{
    return getNodeFromNormalizedString(getNormalizedPath(p), isFile, mayNotExist);
}

Node *Node::getNodeFromNonNormalizedPath(const path &p, const bool isFile, const bool mayNotExist)
{
    return getNodeFromNormalizedString(getNormalizedPath(p), isFile, mayNotExist);
}

Node *Node::addHalfNodeFromNormalizedStringSingleThreaded(string normalizedFilePath)
{
    return const_cast<Node *>(nodeAllFiles.emplace(std::move(normalizedFilePath)).first.operator->());
}

Node *Node::getHalfNode(const string_view p)
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

Node *Node::getHalfNode(const uint32_t index)
{
    return nodeIndices[index];
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