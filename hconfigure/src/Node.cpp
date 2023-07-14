
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
    return pstring(filePath.begin() + filePath.find_last_of(slash), filePath.end());
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

Node *Node::getNodeFromNormalizedString(pstring p, bool isFile, bool mayNotExist)
{
    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.

    Node *node = nullptr;
    {
        lock_guard<mutex> lk{nodeInsertMutex};
        if (auto it = allFiles.find(pstring_view(p)); it != allFiles.end())
        {
            node = const_cast<Node *>(it.operator->());
        }
        else
        {
            node = const_cast<Node *>(allFiles.emplace(std::move(p)).first.operator->());
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

Node *Node::getNodeFromNormalizedString(pstring_view p, bool isFile, bool mayNotExist)
{
    // TODO
    // getLastEditTime() also makes a system-call. Is it faster if this data is also fetched with following
    // Check for std::filesystem::file_type of std::filesystem::path in Node constructor is a system-call and hence
    // performed only once.

    Node *node = nullptr;
    {
        auto str = new string(p);
        pstring_view view(*str);
        lock_guard<mutex> lk{nodeInsertMutex};
        if (auto it = allFiles.find(view); it != allFiles.end())
        {
            node = const_cast<Node *>(it.operator->());
        }
        else
        {
            node = const_cast<Node *>(allFiles.emplace(pstring(view)).first.operator->());
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

Node *Node::getNodeFromNonNormalizedString(const pstring &p, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromPath(p);
    return Node::getNodeFromNormalizedString((filePath.*toPStr)(), isFile, mayNotExist);
}

Node *Node::getNodeFromNormalizedPath(const path &p, bool isFile, bool mayNotExist)
{
    return Node::getNodeFromNormalizedString((p.*toPStr)(), isFile, mayNotExist);
}

Node *Node::getNodeFromNonNormalizedPath(const path &p, bool isFile, bool mayNotExist)
{
    path filePath = getFinalNodePathFromPath(p);
    return Node::getNodeFromNormalizedString((filePath.*toPStr)(), isFile, mayNotExist);
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
