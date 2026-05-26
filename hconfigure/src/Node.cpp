#include "Node.hpp"
#include "rapidhash/rapidhash.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <utility>

#ifdef _WIN32
#include "Windows.h"
#else
#include <cerrno>
#include <sys/stat.h>
#endif

using std::filesystem::file_type, std::filesystem::file_time_type, std::lock_guard;

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

Node::Node(const string_view filePath_) : filePath(filePath_)
{
    myId = idCount++;
    nodeIndices[myId] = this;
}

string Node::getFileName() const
{
    if (const size_t slashPos = filePath.find_last_of(slashc); slashPos != string::npos)
    {
        return string(filePath.substr(slashPos + 1));
    }
    return filePath;
}

string Node::getFileStem() const
{
    const size_t slashPos = filePath.find_last_of(slashc);
    const size_t nameStart = slashPos == string::npos ? 0 : slashPos + 1;
    const size_t dotPos = filePath.find_last_of('.');
    if (dotPos == string::npos || dotPos <= nameStart)
    {
        return string(filePath.substr(nameStart));
    }
    return string(filePath.substr(nameStart, dotPos - nameStart));
}

string Node::getExtension() const
{
    const size_t slashPos = filePath.find_last_of(slashc);
    const size_t nameStart = slashPos == string::npos ? 0 : slashPos + 1;
    const size_t dotPos = filePath.find_last_of('.');
    if (dotPos == string::npos || dotPos <= nameStart)
    {
        return {};
    }
    return filePath.substr(dotPos);
}

void Node::performSystemCheck()
{
    // In build-mode, this function can be called only once.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (statCompleted)
        {
            return;
        }
        statCompleted = true;
    }
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
        // Non-not-found error: mark as unknown and leave timestamp unset.
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
        fileType = file_type::character; // Windows does not directly map to POSIX block devices.
    }
    else
    {
        fileType = file_type::regular;
        fileSize = (static_cast<uint64_t>(attrs.nFileSizeHigh) << 32) | attrs.nFileSizeLow;
    }

    // Always set lastWriteTime for every resolved file type.
    // Convert Windows FILETIME to std::filesystem::file_time_type.
    ULARGE_INTEGER ull;
    ull.LowPart = attrs.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = attrs.ftLastWriteTime.dwHighDateTime;

    // Convert to std::chrono time point.
    // Windows FILETIME uses 100ns intervals since Jan 1, 1601.
    const auto duration = std::chrono::duration<uint64_t, std::ratio<1, 10000000>>(ull.QuadPart);
    constexpr auto windows_epoch = std::chrono::duration<uint64_t, std::ratio<1, 10000000>>(116444736000000000ULL);
    const auto unix_time = duration - windows_epoch;

    // Cast to nanoseconds to match the POSIX path's unit.
    lastWriteTime = std::chrono::duration_cast<std::chrono::nanoseconds>(unix_time).count();
#else
    struct stat st{};
    if (stat(filePath.c_str(), &st) != 0)
    {
        if (errno == ENOENT || errno == ENOTDIR)
        {
            fileType = file_type::not_found;
        }
        else
        {
            fileType = file_type::unknown;
        }
        lastWriteTime = {};
        return;
    }

    if (S_ISREG(st.st_mode))
    {
        fileType = file_type::regular;
        fileSize = static_cast<uint64_t>(st.st_size);
        // ... lastWriteTime as before
#if defined(__APPLE__)
        lastWriteTime = static_cast<int64_t>(st.st_mtimespec.tv_sec) * 1'000'000'000LL +
                        static_cast<int64_t>(st.st_mtimespec.tv_nsec);
#else
        lastWriteTime = st.st_mtim.tv_sec * 1'000'000'000LL + st.st_mtim.tv_nsec;
#endif
    }
    else if (S_ISDIR(st.st_mode))
    {
        fileType = file_type::directory;
        lastWriteTime = {};
    }
    else
    {
        fileType = file_type::unknown;
        lastWriteTime = {};
    }
#endif
}

Node *Node::getNode(const string_view filePath_, const bool isFile, const bool mayNotExist)
{
#ifdef BUILD_MODE
    printErrorMessage(FORMAT("For filePath {}\n Node::getNode is called at build-time.\n", filePath_));
#endif

    const auto &[it, ok] = nodeAllFiles.emplace(filePath_);
    Node *node = &const_cast<Node &>(*it);

    node->performSystemCheck();
    if (node->fileType != (isFile ? file_type::regular : file_type::directory) && !mayNotExist)
    {
        printErrorMessage(FORMAT("{} is not a {} file. File Type is {}\n", node->filePath, isFile ? "regular" : "dir",
                                 getStatusString(node->filePath)));
    }
    return node;
}

void Node::performContentHash()
{
    if (fileSize == 0)
    {
        contentHash = 0;
        return;
    }

    if (hashCompleted)
    {
        return;
    }
    hashCompleted = true;

#ifdef _WIN32
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        contentHash = 0;
        return;
    }
    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap)
    {
        contentHash = 0;
        CloseHandle(hFile);
        return;
    }
    const void *view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!view)
    {
        contentHash = 0;
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }
    contentHash = rapidhash(view, fileSize);
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);
#else
    const int fd = open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == -1)
    {
        contentHash = 0;
        return;
    }
    void *mapping = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); // safe to close before hashing; mapping remains valid
    if (mapping == MAP_FAILED)
    {
        contentHash = 0;
        return;
    }
    madvise(mapping, fileSize, MADV_SEQUENTIAL);
    contentHash = rapidhash(mapping, fileSize);
    munmap(mapping, fileSize);
#endif
}

Node *Node::getNodeNonNormalized(const string &filePath_, const bool isFile, const bool mayNotExist)
{
    return getNode(getNormalizedPath(filePath_), isFile, mayNotExist);
}

Node *Node::getHalfNode(const string_view filePath_)
{
    const auto &[it, ok] = nodeAllFiles.emplace(filePath_);
    return &const_cast<Node &>(*it);
}

Node *Node::getHalfNodeNonNormalized(const string_view filePath_)
{
    return getHalfNode(getNormalizedPath(filePath_));
}

Node *Node::getHalfNode(const uint32_t index)
{
    return nodeIndices[index];
}