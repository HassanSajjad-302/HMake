
#ifdef USE_HEADER_UNITS
import <atomic>;
import "rapidhash.h";
import "Node.hpp";
import <mutex>;
#else

#include "Node.hpp"
#include "rapidhash.h"
#include <mutex>
#include <utility>
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

std::size_t NodeHash::operator()(const Node &node) const
{
    return rapidhash(node.filePath.c_str(), node.filePath.size());
}

std::size_t NodeHash::operator()(const pstring_view &str) const
{
    return rapidhash(str.data(), str.size());
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

Node::Node(pstring filePath_) : filePath(std::move(filePath_))
{
}

pstring Node::getFileName() const
{
    return pstring(filePath.begin() + filePath.find_last_of(slashc) + 1, filePath.end());
}

PValue Node::getPValue() const
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    return PValue(myId);
#else
    return PValue(ptoref(filePath));
#endif
}

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

void Node::ensureSystemCheckCalled(bool isFile, bool mayNotExist)
{
    if (reinterpret_cast<atomic<bool> &>(systemCheckCompleted).load())
    {
        return;
    }

    // If systemCheck was not called previously or isn't being called, call it.
    if (!reinterpret_cast<atomic<bool> &>(systemCheckCalled).exchange(true))
    {
        performSystemCheck(isFile, mayNotExist);
        reinterpret_cast<atomic<bool> &>(systemCheckCompleted).store(true);
    }

    // systemCheck is being called for this node by another thread
    while (!reinterpret_cast<atomic<bool> &>(systemCheckCompleted).load())
    {
        // std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

bool Node::trySystemCheck(const bool isFile, const bool mayNotExist)
{
    // If systemCheck was not called previously or isn't being called, call it.
    if (!reinterpret_cast<atomic<bool> &>(systemCheckCalled).exchange(true))
    {
        performSystemCheck(isFile, mayNotExist);
        reinterpret_cast<atomic<bool> &>(systemCheckCompleted).store(true);
        return true;
    }

    if (reinterpret_cast<atomic<bool> &>(systemCheckCompleted).load())
    {
        return true;
    }

    return false;
}

// static SpinLock nodeInsertMutex;
Node *Node::getNodeFromNormalizedString(pstring p, const bool isFile, const bool mayNotExist)
{
    Node *node = nullptr;

    using Map = decltype(nodeAllFiles);

    if (nodeAllFiles.lazy_emplace_l(
            pstring_view(p), [&](const Map::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const Map::constructor &constructor) { constructor(p); }))
    {
        nodeAllFiles.if_contains(pstring_view(p), [&](const Node &node_) { node = const_cast<Node *>(&node_); });
    }

    node->ensureSystemCheckCalled(isFile, mayNotExist);
    return node;
}

Node *Node::getNodeFromNormalizedString(const pstring_view p, const bool isFile, const bool mayNotExist)
{
    Node *node = nullptr;

    using Map = decltype(nodeAllFiles);

    if (nodeAllFiles.lazy_emplace_l(
            p, [&](const Map::value_type &node_) { node = const_cast<Node *>(&node_); },
            [&](const Map::constructor &constructor) { constructor(pstring(p)); }))
    {
        nodeAllFiles.if_contains(p, [&](const Node &node_) { node = const_cast<Node *>(&node_); });
    }

    node->ensureSystemCheckCalled(isFile, mayNotExist);
    return node;
}

Node *Node::getNodeFromNonNormalizedString(const pstring &p, const bool isFile, const bool mayNotExist)
{
    if (bsMode == BSMode::BUILD)
    {
        // printMessage("Called\n");
    }
    const path filePath = getFinalNodePathFromPath(p);
    return getNodeFromNormalizedString((filePath.*toPStr)(), isFile, mayNotExist);
}

Node *Node::getNodeFromNormalizedPath(const path &p, const bool isFile, const bool mayNotExist)
{
    return getNodeFromNormalizedString((p.*toPStr)(), isFile, mayNotExist);
}

Node *Node::getNodeFromNonNormalizedPath(const path &p, const bool isFile, const bool mayNotExist)
{
    if (bsMode == BSMode::BUILD)
    {
        // printMessage("Called\n");
    }
    const path filePath = getFinalNodePathFromPath(p);
    return getNodeFromNormalizedString((filePath.*toPStr)(), isFile, mayNotExist);
}

Node *Node::getHalfNodeFromNormalizedString(pstring normalizedFilePath)
{
    if (const auto &[it, ok] = nodeAllFiles.emplace(std::move(normalizedFilePath)); ok)
    {
        // Why do atomic when it is executed single threaded
        const_cast<Node &>(*it).myId = reinterpret_cast<uint32_t &>(idCount)++;
        nodeIndices[it->myId] = const_cast<Node *>(it.operator->());
        const_cast<Node &>(*it).loadedFromNodesCache = true;
        return const_cast<Node *>(it.operator->());
    }
    else
    {
        return const_cast<Node *>(it.operator->());
    }
}

void Node::emplaceNodeInPValue(const Node *node, PValue &pValue)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    pValue.PushBack(PValue(node->myId), ralloc);
#else
    pValue.PushBack(ptoref(node->filePath), ralloc);
#endif
}

void Node::emplaceNodeInPValue(const Node *node, PValue &pValue, decltype(ralloc) alloc)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    pValue.PushBack(PValue(node->myId), alloc);
#else
    pValue.PushBack(ptoref(node->filePath), alloc);
#endif
}

Node *Node::getNodeFromPValue(const PValue &pValue, bool isFile, bool mayNotExist)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    Node *node = nodeIndices[pValue.GetUint64()];
    node->ensureSystemCheckCalled(isFile, mayNotExist);
#else
    Node *node = Node::getNodeFromNormalizedString(pstring_view(pValue.GetString(), pValue.GetStringLength()), isFile,
                                                   mayNotExist);
#endif
    return node;
}

Node *Node::tryGetNodeFromPValue(bool &systemCheckSucceeded, const PValue &pValue, bool isFile, bool mayNotExist)
{
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
    Node *node = nodeIndices[pValue.GetUint64()];
    systemCheckSucceeded = node->trySystemCheck(isFile, mayNotExist);
#else
    Node *node = Node::getNodeFromNormalizedString(pstring_view(pValue.GetString(), pValue.GetStringLength()), isFile,
                                                   mayNotExist);
    systemCheckSucceeded = true;
#endif
    return node;
}

#include "Windows.h"

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
                printErrorMessage(fmt::format("{} is not a {} file. File Type is {}\n", filePath,
                                              isFile ? "regular" : "directory", getStatusPString(filePath)));
                throw std::exception();
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
            printErrorMessage(fmt::format("FindFirstFileEx failed {}\n", GetLastError()));
            printErrorMessage(fmt::format("{} is not a {} file. File Type is {}\n", filePath,
                                          isFile ? "regular" : "directory", getStatusPString(filePath)));
            throw std::exception();
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
            printErrorMessage(fmt::format("FindFirstFileEx failed {}\n", GetLastError()));
            printErrorMessage(
                fmt::format("{} is not a directory file. File Type is {}\n", filePath, getStatusPString(filePath)));
            throw std::exception();
        }
    }

    FindClose(hFind)*/
    ;
    /////////////////////////////////////

    if (!loadedFromNodesCache)
    {
        myId = idCount.fetch_add(1);
        nodeIndices[myId] = this;
        nodesCache.PushBack(PValue(ptoref(filePath)), ralloc);
    }
}

void Node::clearNodes()
{
    nodeAllFiles.clear();
    idCount = 0;
    for (Node *&node : nodeIndices)
    {
        node = nullptr;
    }
}