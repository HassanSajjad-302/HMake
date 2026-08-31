#include "Node.hpp"
#include "Manager.hpp"
#include "rapidhash/rapidhash.h"

#ifdef _WIN32
#include "Windows.h"
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

using std::filesystem::file_type;

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

uint64_t NodeHash::operator()(const Node &node) const
{
    return rapidhash(node.filePath.c_str(), node.filePath.size());
}

uint64_t NodeHash::operator()(const string_view &str) const
{
    return rapidhash(str.data(), str.size());
}

Node::Node(const string_view filePath_) : filePath(filePath_), myId(idCount++)
{
    if (myId >= 128 * 1024)
    {
        printErrorMessage(FORMAT("Maximum node count exceeded.\nLimit: {}\nPath: {}", 128 * 1024, filePath));
    }
    nodeIndices.emplace_back(this);
}

bool Node::isAbsolute(string_view fileSystemPath)
{
    if (fileSystemPath.empty())
    {
        return false;
    }

    if constexpr (os == OS::NT)
    {
        const auto isSeparator = [](const char character) { return character == '\\' || character == '/'; };

        if (fileSystemPath.size() >= 2 && isSeparator(fileSystemPath[0]) && isSeparator(fileSystemPath[1]))
        {
            return true;
        }

        const char driveLetter = fileSystemPath[0];
        const bool isAsciiLetter = (driveLetter >= 'A' && driveLetter <= 'Z') ||
                                   (driveLetter >= 'a' && driveLetter <= 'z');
        return fileSystemPath.size() >= 3 && isAsciiLetter && fileSystemPath[1] == ':' &&
               isSeparator(fileSystemPath[2]);
    }
    else
    {
        return fileSystemPath.front() == '/';
    }
}

string_view Node::getFileName() const noexcept
{
    if (const uint64_t slashPos = filePath.find_last_of(slashc); slashPos != string::npos)
    {
        return {filePath.data() + slashPos + 1, filePath.size() - slashPos - 1};
    }
    return filePath;
}

string_view Node::getFileStem() const noexcept
{
    const uint64_t slashPos = filePath.find_last_of(slashc);
    const uint64_t nameStart = slashPos == string::npos ? 0 : slashPos + 1;
    const uint64_t dotPos = filePath.find_last_of('.');
    if (dotPos == string::npos || dotPos <= nameStart)
    {
        return {filePath.data() + nameStart, filePath.size() - nameStart};
    }
    return {filePath.data() + nameStart, dotPos - nameStart};
}

string_view Node::getFileExtension() const noexcept
{
    const uint64_t slashPos = filePath.find_last_of(slashc);
    const uint64_t nameStart = slashPos == string::npos ? 0 : slashPos + 1;
    const uint64_t dotPos = filePath.find_last_of('.');
    if (dotPos == string::npos || dotPos <= nameStart)
    {
        return {};
    }
    return {filePath.data() + dotPos, filePath.size() - dotPos};
}

void Node::performSystemCheck()
{
    if (statCompleted)
    {
        return;
    }
    statCompleted = true;
    const uint64_t persistedLastWriteTime = lastWriteTime;
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
    if (stat(filePath.data(), &st) != 0)
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

    // Until this check, lastWriteTime/contentHash hold one persisted snapshot. An unchanged regular-file timestamp
    // makes that content hash current, so Builder::checkNodes() can omit the file from its hashing work.
    if (fileType == file_type::regular && contentHash != missingContentHash &&
        lastWriteTime == persistedLastWriteTime)
    {
        hashCompleted = true;
    }
}

Node *Node::getNode(const string_view filePath_, const bool isFile, const bool mayNotExist)
{
    Node *node = getHalfNode(filePath_);

    node->performSystemCheck();
    if (node->fileType != (isFile ? file_type::regular : file_type::directory) && !mayNotExist)
    {
        printErrorMessage(FORMAT("Filesystem entry has the wrong type.\nPath: {}\nExpected type: {}\nActual status:{}",
                                 node->filePath, isFile ? "regular file" : "directory",
                                 getStatusString(node->filePath)));
    }
    return node;
}

Node *Node::getNode(const std::filesystem::directory_entry &entry)
{
    string filePath = entry.path().string();
    lowerCaseOnWindows(filePath.data(), filePath.size());
    return getNode(filePath, entry.is_regular_file());
}

void Node::performContentHash()
{
    if (fileType != file_type::regular)
    {
        printErrorMessage(FORMAT("Cannot hash a filesystem entry that is not a regular file.\nPath: {}\n"
                                 "File type: {}",
                                 filePath, static_cast<int>(fileType)));
    }

    if (hashCompleted)
    {
        return;
    }

    if (fileSize == 0)
    {
        contentHash = 0;
        hashCompleted = true;
        return;
    }
#ifdef _WIN32
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(FORMAT("Could not open a file for content hashing.\nPath: {}\nOperation: CreateFileA\n"
                                 "System error: {}",
                                 filePath, P2978::getErrorString()));
    }
    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap)
    {
        printErrorMessage(FORMAT("Could not create a file mapping for content hashing.\nPath: {}\n"
                                 "Operation: CreateFileMappingA\nSystem error: {}",
                                 filePath, P2978::getErrorString()));
    }
    const void *view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!view)
    {
        printErrorMessage(FORMAT("Could not map a file for content hashing.\nPath: {}\nOperation: MapViewOfFile\n"
                                 "System error: {}",
                                 filePath, P2978::getErrorString()));
    }
    contentHash = rapidhash(view, fileSize);
    hashCompleted = true;
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);
#else
    const int fd = open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == -1)
    {
        printErrorMessage(FORMAT("Could not open a file for content hashing.\nPath: {}\nOperation: open\n"
                                 "System error: {}",
                                 filePath, P2978::getErrorString()));
    }
    void *mapping = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED)
    {
        const string systemError = P2978::getErrorString();
        close(fd);
        printErrorMessage(FORMAT("Could not map a file for content hashing.\nPath: {}\nOperation: mmap\n"
                                 "File size: {} bytes\nSystem error: {}",
                                 filePath, fileSize, systemError));
    }
    close(fd); // safe to close before hashing; mapping remains valid
    madvise(mapping, fileSize, MADV_SEQUENTIAL);
    contentHash = rapidhash(mapping, fileSize);
    hashCompleted = true;
    if (munmap(mapping, fileSize) == -1)
    {
        printErrorMessage(FORMAT("Could not unmap a file after content hashing.\nPath: {}\nOperation: munmap\n"
                                 "System error: {}",
                                 filePath, P2978::getErrorString()));
    }
#endif
}

Node *Node::getNodeNonNormalized(const string_view filePath_, const bool isFile, const bool mayNotExist)
{
    return getNode(getNormalizedPath(filePath_), isFile, mayNotExist);
}

string_view Node::getDirectoryStringView() const
{
    const uint64_t separator = filePath.find_last_of(slashc);
    if (separator == string::npos)
    {
        return {};
    }
    return string_view(filePath).substr(0, separator);
}

Node *Node::getHalfNode(const string_view filePath_)
{
    const auto it = nodeAllFiles.emplace(filePath_).first;
    return &const_cast<Node &>(*it);
}

Node *Node::getHalfNodeNonNormalized(const string_view filePath_)
{
    if constexpr (os != OS::NT)
    {
        if (isAbsolute(filePath_) && filePath_.back() != '/')
        {
            bool isClearlyNormalized = true;
            for (uint64_t i = 1; i < filePath_.size(); ++i)
            {
                if (filePath_[i] == '/' && filePath_[i - 1] == '/')
                {
                    isClearlyNormalized = false;
                    break;
                }
                if (filePath_[i] == '.' && filePath_[i - 1] == '/' &&
                    (i + 1 == filePath_.size() || filePath_[i + 1] == '/' ||
                     (filePath_[i + 1] == '.' &&
                      (i + 2 == filePath_.size() || filePath_[i + 2] == '/'))))
                {
                    isClearlyNormalized = false;
                    break;
                }
            }
            if (isClearlyNormalized)
            {
                return getHalfNode(filePath_);
            }
        }
    }
    return getHalfNode(getNormalizedPath(filePath_));
}

Node *Node::getHalfNode(const uint32_t index)
{
    assert(index < nodeIndices.size());
    return nodeIndices[index];
}
