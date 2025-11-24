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
}

// This function is called single-threaded. While the above is called multithreaded in lambdas passed to nodeAllFiles
// emplace functions.
Node::Node(string filePath_) : filePath(std::move(filePath_))
{
    myId = reinterpret_cast<uint32_t &>(idCount)++;
    nodeIndices[myId] = this;
}

string Node::getFileName() const
{
    return {filePath.begin() + filePath.find_last_of(slashc) + 1, filePath.end()};
}

string Node::getFileStem() const
{
    return {filePath.begin() + filePath.find_last_of(slashc) + 1, filePath.begin() + filePath.find_last_of('.')};
}

void Node::performSystemCheck()
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &attrs))
    {
        if (const DWORD win_err = GetLastError(); win_err == ERROR_FILE_NOT_FOUND || win_err == ERROR_PATH_NOT_FOUND)
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
    constexpr auto windows_epoch = std::chrono::duration<int64_t, std::ratio<1, 10000000>>(116444736000000000LL);
    const auto unix_time = duration - windows_epoch;

    lastWriteTime = file_time_type(unix_time);
#else
    const auto entry = directory_entry(filePath);
    fileType = entry.status().type();
    if (fileType == file_type::regular)
    {
        lastWriteTime = entry.last_write_time();
    }
#endif
}

void Node::ensureSystemCheckCalled()
{
    if (atomic_ref(systemCheckCompleted).load(std::memory_order_acquire))
    {
        return;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!atomic_ref(systemCheckCalled).exchange(true))
    {
        performSystemCheck();
        atomic_ref(systemCheckCompleted).store(true, std::memory_order_release);
        return;
    }

    // systemCheck is being called for this node by another thread
    while (!atomic_ref(systemCheckCompleted).load(std::memory_order_acquire))
    {
    }
}

Node *Node::getNode(const string_view filePath_, const bool isFile, const bool mayNotExist)
{
    Node *node = nullptr;

    if (nodeAllFiles.lazy_emplace_l(
            filePath_, [&](const NodeHashSet::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const NodeHashSet::constructor &constructor) { constructor(node, string(filePath_)); }))
    {
    }

    node->ensureSystemCheckCalled();
    if (node->fileType != (isFile ? file_type::regular : file_type::directory) && !mayNotExist)
    {
        printErrorMessage(FORMAT("{} is not a {} file. File Type is {}\n", node->filePath, isFile ? "regular" : "dir",
                                 getStatusString(node->filePath)));
    }
    return node;
}

Node *Node::getNodeNonNormalized(const string &filePath_, const bool isFile, const bool mayNotExist)
{
    return getNode(getNormalizedPath(filePath_), isFile, mayNotExist);
}

Node *Node::getHalfNodeST(string normalizedFilePath)
{
    return const_cast<Node *>(nodeAllFiles.emplace(std::move(normalizedFilePath)).first.operator->());
}

Node *Node::getHalfNode(const string_view p)
{
    Node *node = nullptr;

    if (nodeAllFiles.lazy_emplace_l(
            p, [&](const NodeHashSet::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const NodeHashSet::constructor &constructor) { constructor(node, string(p)); }))
    {
    }

    return node;
}

Node *Node::getHalfNode(const uint32_t index)
{
    return nodeIndices[index];
}