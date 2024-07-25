
#ifdef USE_HEADER_UNITS
import <atomic>;
import "Node.hpp";
import <mutex>;
#else

#include "Node.hpp"
#include <mutex>
#endif

using std::filesystem::directory_entry, std::filesystem::file_type, std::filesystem::file_time_type, std::lock_guard,
    std::mutex;

pstring getStatusPString(const path &p)
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

bool NodeEqual::operator()(const Node &lhs, const pstring_view &rhs) const
{
    return lhs.filePath == rhs;
}

bool NodeEqual::operator()(const pstring_view &lhs, const Node &rhs) const
{
    return lhs == rhs.filePath;
}
#include "komihash.h"
std::size_t NodeHash::operator()(const Node &node) const
{
    return komihash(node.filePath.c_str(), node.filePath.size(), 0);
}

std::size_t NodeHash::operator()(const pstring_view &str) const
{
    return komihash(str.data(), str.size(), 0);
}

bool CompareNode::operator()(const Node &lhs, const Node &rhs) const
{
    return lhs.filePath < rhs.filePath;
}

bool CompareNode::operator()(const pstring_view &lhs, const Node &rhs) const
{
    return lhs < rhs.filePath;
}

bool CompareNode::operator()(const Node &lhs, const pstring_view &rhs) const
{
    return lhs.filePath < rhs;
}

Node::Node(pstring filePath_) : filePath(filePath_)
{
}

pstring Node::getFileName() const
{
    return pstring(filePath.begin() + filePath.find_last_of(slashc) + 1, filePath.end());
}

file_time_type Node::getLastUpdateTime() const
{
    /*if (!isUpdated.load())
    {
        const_cast<file_time_type &>(lastUpdateTime) = last_write_time(path(filePath));
        const_cast<atomic<bool> &>(isUpdated).store(true);
    }*/
    return lastUpdateTime;
}

path Node::getFinalNodePathFromPath(path filePath)
{
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = filePath.lexically_normal();

    if constexpr (os == OS::NT)
    {
        // Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        // actual paths. In Windows paths could be case-insensitive. Just another wrinkle hahaha.
        for (auto it = const_cast<path::value_type *>(filePath.c_str()); *it != '\0'; ++it)
        {
            *it = std::tolower(*it);
        }
    }
    return filePath;
}

class SpinLock
{
    std::atomic_flag locked = ATOMIC_FLAG_INIT;

  public:
    void lock()
    {
        while (locked.test_and_set(std::memory_order_acquire))
        {
        }
    }
    void unlock()
    {
        locked.clear(std::memory_order_release);
    }
};

static mutex nodeInsertMutex;

// static SpinLock nodeInsertMutex;
Node *Node::getNodeFromNormalizedString(pstring p, const bool isFile, const bool mayNotExist)
{
    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once. One more reason for this to be atomic is that a user might delete a file after it has been
    // checked for existence and calling last_edit_time will throw causing it be a build-system error instead of
    // compilation-error.
    // Also, calling lastEditTime for a Node for which doesNotExists == true should throw

    Node *node;
    {
        lock_guard lk{nodeInsertMutex};
        if (const auto it = allFiles.find(pstring_view(p)); it != allFiles.end())
        {
            node = const_cast<Node *>(it.operator->());
        }
        else
        {
            node = const_cast<Node *>(allFiles.emplace(std::move(p)).first.operator->());
        }
    }

    if (node->systemCheckCompleted.load())
    {
        return node;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!node->systemCheckCalled.exchange(true))
    {
        node->performSystemCheck(isFile, mayNotExist);
        node->systemCheckCompleted.store(true);
        return node;
    }

    // systemCheck is being called for this node by another thread
    while (!node->systemCheckCompleted.load())
        ;

    return node;
}

Node *Node::getNodeFromNormalizedString(const pstring_view p, const bool isFile, const bool mayNotExist)
{
    Node *node;
    {
        const auto str = new string(p);
        const pstring_view view(*str);
        lock_guard lk{nodeInsertMutex};
        if (const auto it = allFiles.find(view); it != allFiles.end())
        {
            node = const_cast<Node *>(it.operator->());
        }
        else
        {
            node = const_cast<Node *>(allFiles.emplace(pstring(view)).first.operator->());
        }
    }

    if (node->systemCheckCompleted.load())
    {
        return node;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!node->systemCheckCalled.exchange(true))
    {
        node->performSystemCheck(isFile, mayNotExist);
        node->systemCheckCompleted.store(true);
        return node;
    }

    // systemCheck is being called for this node by another thread
    while (!node->systemCheckCompleted.load())
        ;

    return node;
}

Node *Node::getNodeFromNonNormalizedString(const pstring &p, const bool isFile, const bool mayNotExist)
{
    const path filePath = getFinalNodePathFromPath(p);
    return getNodeFromNormalizedString((filePath.*toPStr)(), isFile, mayNotExist);
}

Node *Node::getNodeFromNormalizedPath(const path &p, const bool isFile, const bool mayNotExist)
{
    return getNodeFromNormalizedString((p.*toPStr)(), isFile, mayNotExist);
}

Node *Node::getNodeFromNonNormalizedPath(const path &p, const bool isFile, const bool mayNotExist)
{
    const path filePath = getFinalNodePathFromPath(p);
    return getNodeFromNormalizedString((filePath.*toPStr)(), isFile, mayNotExist);
}

void Node::performSystemCheck(const bool isFile, const bool mayNotExist)
{
    if (const directory_entry entry = directory_entry(filePath);
        entry.status().type() == (isFile ? file_type::regular : file_type::directory))
    {
        lastUpdateTime = entry.last_write_time();
    }
    else
    {
        if (!mayNotExist || entry.status().type() != file_type::not_found)
        {
            printErrorMessage(fmt::format("{} is not a {} file. File Type is {}\n", filePath,
                                          isFile ? "regular" : "directory", getStatusPString(filePath)));
            throw std::exception();
        }
        doesNotExist = true;
    }
}

bool operator<(const Node &lhs, const Node &rhs)
{
    return lhs.filePath < rhs.filePath;
}

void to_json(Json &j, const Node *node)
{
    j = node->filePath;
}
