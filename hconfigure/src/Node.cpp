#include "Node.hpp"
#include "Manager.hpp"
#include "rapidhash/rapidhash.h"
#include <cstring>

#ifdef _WIN32
#include "Windows.h"
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

using std::filesystem::file_type;

static bool canSkipNormalization(const string_view filePath, const PathType pathType, bool &absolute)
{
    if (!filePath.empty())
    {
        const char lastCharacter = filePath.back();
        bool endsInSeparator = lastCharacter == slashc;
        if constexpr (os == OS::NT)
        {
            endsInSeparator = endsInSeparator || lastCharacter == '/';
        }
        if (endsInSeparator)
        {
            printErrorMessage(
                FORMAT("A path passed to Node must not end in a directory separator.\nPath: {}", filePath));
        }
    }

    absolute = pathType == PathType::ABSOLUTE || Node::isAbsolute(filePath);
    if (!absolute)
    {
        return false;
    }
    if (pathType == PathType::NORMAL)
    {
        return true;
    }
    if constexpr (os == OS::NT)
    {
        // Even a lexically clean Windows path may still need separator conversion and lower-casing.
        return false;
    }

    uint64_t offset = 1;
    while (offset < filePath.size())
    {
        const uint64_t componentStart = offset;
        while (offset < filePath.size() && filePath[offset] != '/')
        {
            ++offset;
        }
        const uint64_t componentSize = offset - componentStart;
        if (componentSize == 0 ||
            (filePath[componentStart] == '.' &&
             (componentSize == 1 || (componentSize == 2 && filePath[componentStart + 1] == '.'))))
        {
            return false;
        }
        if (offset < filePath.size())
        {
            ++offset;
        }
    }
    return true;
}

template <typename String> static void normalizeNodePath(String &filePath, const bool prependBase)
{
    if (prependBase)
    {
        assert(!normalizationBasePath.empty());
        const uint64_t pathSize = filePath.size();
        const bool addSeparator = normalizationBasePath.back() != slashc;
        const uint64_t prefixSize = normalizationBasePath.size() + addSeparator;
        filePath.resize(prefixSize + pathSize);
        memmove(filePath.data() + prefixSize, filePath.data(), pathSize);
        memcpy(filePath.data(), normalizationBasePath.data(), normalizationBasePath.size());
        if (addSeparator)
        {
            filePath[normalizationBasePath.size()] = slashc;
        }
    }

    if constexpr (os == OS::NT)
    {
        path normalized{string(filePath)};
        normalized = normalized.lexically_normal();
        normalized.make_preferred();
        string normalizedString = normalized.string();
        lowerCaseOnWindows(normalizedString.data(), normalizedString.size());
        filePath.assign(normalizedString.data(), normalizedString.size());
    }
    else
    {
        assert(!filePath.empty() && filePath.front() == '/');
        const uint64_t inputSize = filePath.size();
        uint64_t readOffset = 1;
        uint64_t writeOffset = 1;

        while (readOffset < inputSize)
        {
            while (readOffset < inputSize && filePath[readOffset] == '/')
            {
                ++readOffset;
            }
            const uint64_t componentStart = readOffset;
            while (readOffset < inputSize && filePath[readOffset] != '/')
            {
                ++readOffset;
            }
            const uint64_t componentSize = readOffset - componentStart;
            if (componentSize == 0 || (componentSize == 1 && filePath[componentStart] == '.'))
            {
                continue;
            }
            if (componentSize == 2 && filePath[componentStart] == '.' && filePath[componentStart + 1] == '.')
            {
                if (writeOffset > 1)
                {
                    while (writeOffset > 1 && filePath[writeOffset - 1] != '/')
                    {
                        --writeOffset;
                    }
                    if (writeOffset > 1)
                    {
                        --writeOffset;
                    }
                }
                continue;
            }
            if (writeOffset > 1)
            {
                filePath[writeOffset++] = '/';
            }
            memmove(filePath.data() + writeOffset, filePath.data() + componentStart, componentSize);
            writeOffset += componentSize;
        }
        filePath.resize(writeOffset);
    }
}

Node::Node(const string_view filePath_) : filePath(filePath_), myId(idCount++)
{
    assert(filePath_.data()[filePath_.size()] == '\0');
    if (myId >= 128 * 1024)
    {
        printErrorMessage(FORMAT("Maximum node count exceeded.\nLimit: {}\nPath: {}", 128 * 1024, filePath_));
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
    if (!GetFileAttributesExA(filePath.data(), GetFileExInfoStandard, &attrs))
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

Node *Node::finishNode(Node *const node, const bool isFile, const bool mayNotExist)
{
    node->performSystemCheck();
    if (node->fileType != (isFile ? file_type::regular : file_type::directory) && !mayNotExist)
    {
        string_view status;
        switch (node->fileType)
        {
        case file_type::none:
            status = " has `not-evaluated-yet` type";
            break;
        case file_type::not_found:
            status = " does not exist";
            break;
        case file_type::regular:
            status = " is a regular file";
            break;
        case file_type::directory:
            status = " is a directory";
            break;
        case file_type::symlink:
            status = " is a symlink";
            break;
        case file_type::block:
            status = " is a block device";
            break;
        case file_type::character:
            status = " is a character device";
            break;
        case file_type::fifo:
            status = " is a named IPC pipe";
            break;
        case file_type::socket:
            status = " is a named IPC socket";
            break;
        case file_type::unknown:
            status = " has `unknown` type";
            break;
        default:
            status = " has `implementation-defined` type";
            break;
        }
        printErrorMessage(FORMAT("Filesystem entry has the wrong type.\nPath: {}\nExpected type: {}\nActual status:{}",
                                 node->filePath, isFile ? "regular file" : "directory", status));
    }
    return node;
}

Node *Node::getNode(const std::filesystem::directory_entry &entry)
{
    string filePath = entry.path().string();
    lowerCaseOnWindows(filePath.data(), filePath.size());
    return getNode<PathType::NORMAL_ABSOLUTE>(std::move(filePath), entry.is_regular_file());
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
    HANDLE hFile = CreateFileA(filePath.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
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
    const int fd = open(filePath.data(), O_RDONLY | O_CLOEXEC);
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

Node *Node::getHalfNodeImpl(const string_view filePath_)
{
    const auto iterator = nodeAllFiles.lazy_emplace(filePath_, [&](const auto &constructor) {
        nodeStrings.emplace_back(filePath_);
        constructor(string_view(nodeStrings.back()));
    });
    return &const_cast<Node &>(*iterator);
}

Node *Node::getHalfNodeImpl(string &&filePath_)
{
    const string_view lookupPath = filePath_;
    const auto iterator = nodeAllFiles.lazy_emplace(lookupPath, [&](const auto &constructor) {
        nodeStrings.emplace_back(std::move(filePath_));
        constructor(string_view(nodeStrings.back()));
    });
    return &const_cast<Node &>(*iterator);
}

Node *Node::getHalfNodeImpl(const string_view filePath_, const PathType pathType)
{
    bool absolute;
    if (canSkipNormalization(filePath_, pathType, absolute))
    {
        return getHalfNodeImpl(filePath_);
    }

    STACK_PMR_STRING(normalizedPath, 4 * 1024)
    normalizedPath.assign(filePath_);
    normalizeNodePath(normalizedPath, !absolute);
    return getHalfNodeImpl(string_view(normalizedPath));
}

Node *Node::getHalfNodeImpl(string &&filePath_, const PathType pathType)
{
    bool absolute;
    if (canSkipNormalization(filePath_, pathType, absolute))
    {
        return getHalfNodeImpl(std::move(filePath_));
    }
    normalizeNodePath(filePath_, !absolute);
    return getHalfNodeImpl(std::move(filePath_));
}

void Node::normalizeImpl(std::pmr::string &filePath_, const PathType pathType)
{
    bool absolute;
    if (canSkipNormalization(filePath_, pathType, absolute))
    {
        return;
    }
    normalizeNodePath(filePath_, !absolute);
}
