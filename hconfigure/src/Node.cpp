
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

bool CompareNode::operator()(const Node &lhs, const Node &rhs) const
{
    return lhs.filePath < rhs.filePath;
}

bool CompareNode::operator()(const pstring &lhs, const Node &rhs) const
{
    return lhs < rhs.filePath;
}

bool CompareNode::operator()(const Node &lhs, const pstring &rhs) const
{
    return lhs.filePath < rhs;
}

Node::Node(const pstring &filePath_) : filePath(filePath_)
{
}

std::mutex fileTimeUpdateMutex;
std::filesystem::file_time_type Node::getLastUpdateTime() const
{
    lock_guard<mutex> lk(fileTimeUpdateMutex);
    if (!isUpdated)
    {
        const_cast<std::filesystem::file_time_type &>(lastUpdateTime) = last_write_time(path(filePath));
        const_cast<bool &>(isUpdated) = true;
    }
    return lastUpdateTime;
}

/*path Node::getFinalNodePathFromString(const pstring &str)
{
    path filePath{str};
    if (filePath.is_relative())
    {
        filePath = path(srcDir) / filePath;
    }
    filePath = (filePath.lexically_normal().*toPStr)();

    if constexpr (os == OS::NT)
    {
        // Needed because MSVC cl.exe returns header-unit paths is smrules file that are all lowercase instead of the
        // actual paths. In Windows paths could be case-insensitive. Just another wrinkle hahaha.
        pstring lowerCase (= filePath*toPStr)());
        for (char &c : lowerCase)
        {
            // Warning: assuming paths to be ASCII
            c = tolower(c);
        }
        filePath = lowerCase;
    }
    return filePath;
}

static mutex nodeInsertMutex;
Node *Node::getNodeFromNonNormalizedPath(const pstring &str, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromPath(str);

    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.

    {
        lock_guard<mutex> lk(nodeInsertMutex);
        if (auto it = allFiles.find((filePath*toPStr)())); it != allFiles.end())
        {
            return const_cast<Node *>(it.operator->());
        }
    }

    lock_guard<mutex> lk(nodeInsertMutex);
    return const_cast<Node *>(allFiles.emplace(filePath, isFile, mayNotExist).first.operator->());
}*/

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
        auto it = const_cast<path::value_type *>(filePath.c_str());
        for (; *it != '\0'; ++it)
        {
            *it = std::tolower(*it);
        }
    }
    return filePath;
}

static mutex nodeInsertMutex;
Node *Node::getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromPath(p);

    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.

    Node *node = nullptr;
    {
        pstring str = (filePath.*toPStr)();
        lock_guard<mutex> lk{nodeInsertMutex};
        if (auto it = allFiles.find(str); it != allFiles.end())
        {
            node = const_cast<Node *>(it.operator->());
        }
        else
        {
            node = const_cast<Node *>(allFiles.emplace(std::move(str)).first.operator->());
        }
    }

    if (node->systemCheckCompleted.load(std::memory_order_relaxed))
    {
        return node;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!node->systemCheckCalled.exchange(true))
    {
        node->performSystemCheck(isFile, mayNotExist);
        node->systemCheckCompleted.store(true, std::memory_order_relaxed);
    }

    // systemCheck is being called for this node by another thread
    while (!node->systemCheckCompleted.load(std::memory_order_relaxed))
        ;

    return node;
}

Node *Node::getNodeFromNormalizedPath(const path &p, bool isFile, bool mayNotExist)
{
    Node *node = nullptr;
    {
        pstring str = (p.*toPStr)();
        lock_guard<mutex> lk{nodeInsertMutex};
        if (auto it = allFiles.find(str); it != allFiles.end())
        {
            node = const_cast<Node *>(it.operator->());
        }
        else
        {
            node = const_cast<Node *>(allFiles.emplace(std::move(str)).first.operator->());
        }
    }

    if (node->systemCheckCompleted.load(std::memory_order_relaxed))
    {
        return node;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!node->systemCheckCalled.exchange(true))
    {
        node->performSystemCheck(isFile, mayNotExist);
        node->systemCheckCompleted.store(true, std::memory_order_relaxed);
    }

    // systemCheck is being called for this node by another thread
    while (!node->systemCheckCompleted.load(std::memory_order_relaxed))
        ;

    return node;
}

void Node::performSystemCheck(bool isFile, bool mayNotExist)
{
    std::filesystem::file_type nodeType = directory_entry(filePath).status().type();
    if (nodeType == (isFile ? file_type::regular : file_type::directory))
    {
    }
    else
    {
        if (!mayNotExist || nodeType != file_type::not_found)
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
