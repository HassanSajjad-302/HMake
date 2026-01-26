#include "IPCManagerBS.hpp"
#include "Manager.hpp"
#include "Messages.hpp"
#include "expected.hpp"
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include "rapidhash/rapidhash.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace N2978
{

tl::expected<IPCManagerBS, std::string> makeIPCManagerBS(std::string BMIIfHeaderUnitObjOtherwisePath)
{
#ifdef _WIN32
    BMIIfHeaderUnitObjOtherwisePath = R"(\\.\pipe\)" + BMIIfHeaderUnitObjOtherwisePath;

    HANDLE hPipe = CreateNamedPipeA(BMIIfHeaderUnitObjOtherwisePath.c_str(), // pipe name
                                    PIPE_ACCESS_DUPLEX |                     // read/write access
                                        FILE_FLAG_OVERLAPPED |               // overlapped mode for IOCP
                                        FILE_FLAG_FIRST_PIPE_INSTANCE,       // first instance only
                                    PIPE_TYPE_BYTE |                         // message-type pipe
                                        PIPE_READMODE_BYTE |                 // message read mode
                                        PIPE_WAIT,                           // blocking mode (for sync operations)
                                    1,                                       // max instances
                                    BUFFERSIZE * sizeof(TCHAR),              // output buffer size
                                    BUFFERSIZE * sizeof(TCHAR),              // input buffer size
                                    PIPE_TIMEOUT,                            // client time-out
                                    nullptr);                                // default security attributes

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        return tl::unexpected(getErrorString());
    }

    return IPCManagerBS(reinterpret_cast<uint64_t>(server));

#else
    // Named Pipes are used but Unix Domain sockets could have been used as well. The tradeoff is that a file is created
    // and there needs to be bind, listen, accept calls which means that an extra fd is created is temporarily on the
    // server side. it can be closed immediately after.

    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    // Create server socket
    if (fd == -1)
    {
        return tl::unexpected(getErrorString());
    }

    // Prepare address structure
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    // We use file hash to make a file path smaller, since there is a limit of NAME_MAX that is generally 108 bytes.
    // TODO
    // Have an option to receive this path in constructor to make it compatible with Android and IOS.
    std::string prependDir = "/tmp/";
    const uint64_t hash = rapidhash(BMIIfHeaderUnitObjOtherwisePath.c_str(), BMIIfHeaderUnitObjOtherwisePath.size());
    prependDir.append(to16charHexString(hash));
    std::copy(prependDir.begin(), prependDir.end(), addr.sun_path);

    // Remove any existing socket
    unlink(prependDir.c_str());

    // Bind socket to the file system path
    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        return tl::unexpected(getErrorString());
    }
    if (chmod(prependDir.c_str(), 0666) == -1)
    {
        return tl::unexpected(getErrorString());
    }

    // Listen for incoming connections
    if (listen(fd, 1) == -1)
    {
        close(fd);
        return tl::unexpected(getErrorString());
    }
#endif

    return IPCManagerBS(fd);
}

tl::expected<void, std::string> IPCManagerBS::registerManager(const uint64_t serverFd,
                                                              const uint64_t completionKey) const
{
#ifdef _WIN32
    // Associate the pipe with the existing IOCP handle. completionKey is either 0 or a legit value. if 0 then we pass
    // the fd as completion-key. Test checks after the GetQueuedCompletionStatus() call to ensure that the completionKey
    // is same as the fd.
    HANDLE h = CreateIoCompletionPort((HANDLE)fd,                         // handle to associate
                                      reinterpret_cast<HANDLE>(serverFd), // existing IOCP handle
                                      completionKey,                      // completion key
                                      0                                   // number of concurrent threads (0 = default)
    );
    if (h == nullptr)
    {
        CloseHandle((HANDLE)fd);
        return tl::unexpected(getErrorString());
    }
    return {};
#endif
    return {};
}

IPCManagerBS::IPCManagerBS(const uint64_t fd_)
{
    fd = fd_;
    isServer = true;
}

tl::expected<bool, std::string> IPCManagerBS::completeConnection() const
{
#ifdef _WIN32
    // For IOCP, we need to use overlapped I/O
    OVERLAPPED overlapped = {};

    if (ConnectNamedPipe((HANDLE)fd, &overlapped))
    {
        return tl::unexpected("ConnectNamedPipe should not complete synchronously\n");
    }

    DWORD error = GetLastError();

    if (error == ERROR_IO_PENDING)
    {
        // Connection is pending - need to wait for IOCP notification
        // This is the normal case for async operation
        return false;
    }
    if (error == ERROR_PIPE_CONNECTED)
    {
        // Client connected before we called ConnectNamedPipe
        // This can happen if client connects very quickly
        return true;
    }
    if (error == ERROR_NO_DATA)
    {
        // Client connected and exited before we called ConnectnamedPipe
        return true;
    }

    // Actual error occurred
    return tl::unexpected(getErrorString());
#else
    const int fd = accept4(fd, nullptr, nullptr, O_NONBLOCK);
    if (fd == -1)
    {
        if (errno == EAGAIN)
        {
            return false;
        }
        return tl::unexpected(getErrorString());
    }
    close(fd);
    const_cast<int &>(fd) = fd;
    return true;

#endif
}

tl::expected<void, std::string> IPCManagerBS::receiveMessage(char (&ctbBuffer)[320], CTB &messageType) const
{
    //    raise(SIGTRAP); // At the location of the BP.

    // Read from the pipe.
    char buffer[BUFFERSIZE];
    uint32_t bytesRead;
    if (const auto &r = readInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    else
    {
        bytesRead = *r;
    }

    uint32_t bytesProcessed = 1;

    // read call fails if zero byte is read, so safe to process 1 byte
    switch (static_cast<CTB>(buffer[0]))
    {

    case CTB::MODULE: {

        const auto &r = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r)
        {
            return tl::unexpected(r.error());
        }

        messageType = CTB::MODULE;
        getInitializedObjectFromBuffer<CTBModule>(ctbBuffer).moduleName = *r;
    }

    break;

    case CTB::NON_MODULE: {

        const auto &r = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r)
        {
            return tl::unexpected(r.error());
        }

        const auto &r2 = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!r2)
        {
            return tl::unexpected(r.error());
        }

        messageType = CTB::NON_MODULE;
        auto &[isHeaderUnit, str] = getInitializedObjectFromBuffer<CTBNonModule>(ctbBuffer);
        isHeaderUnit = *r;
        str = *r2;
    }

    break;

    case CTB::LAST_MESSAGE: {

        const auto &exitStatusExpected = readBoolFromPipe(buffer, bytesRead, bytesProcessed);
        if (!exitStatusExpected)
        {
            return tl::unexpected(exitStatusExpected.error());
        }

        const auto &outputExpected = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!outputExpected)
        {
            return tl::unexpected(outputExpected.error());
        }

        const auto &errorOutputExpected = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!errorOutputExpected)
        {
            return tl::unexpected(errorOutputExpected.error());
        }

        const auto &logicalNameExpected = readStringFromPipe(buffer, bytesRead, bytesProcessed);
        if (!logicalNameExpected)
        {
            return tl::unexpected(logicalNameExpected.error());
        }

        const auto &fileSizeExpected = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
        if (!fileSizeExpected)
        {
            return tl::unexpected(fileSizeExpected.error());
        }

        messageType = CTB::LAST_MESSAGE;

        auto &[exitStatus, output, errorOutput, logicalName, fileSize] =
            getInitializedObjectFromBuffer<CTBLastMessage>(ctbBuffer);

        exitStatus = *exitStatusExpected;
        output = *outputExpected;
        errorOutput = *errorOutputExpected;
        logicalName = *logicalNameExpected;
        fileSize = *fileSizeExpected;
    }
    break;
    }

    if (bytesRead != bytesProcessed)
    {
        return tl::unexpected(getErrorString(bytesRead, bytesProcessed));
    }

    return {};
}

tl::expected<void, std::string> IPCManagerBS::sendMessage(const BTCModule &moduleFile) const
{
    std::string buffer;
    writeProcessMappingOfBMIFile(buffer, moduleFile.requested);
    buffer.push_back(moduleFile.isSystem);
    writeVectorOfModuleDep(buffer, moduleFile.modDeps);
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<void, std::string> IPCManagerBS::sendMessage(const BTCNonModule &nonModule) const
{
    std::string buffer;
    buffer.push_back(nonModule.isHeaderUnit);
    buffer.push_back(nonModule.isSystem);
    writeString(buffer, nonModule.filePath);
    writeUInt32(buffer, nonModule.fileSize);
    writeVectorOfStrings(buffer, nonModule.logicalNames);
    writeVectorOfHeaderFiles(buffer, nonModule.headerFiles);
    writeVectorOfHuDeps(buffer, nonModule.huDeps);
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<void, std::string> IPCManagerBS::sendMessage(const BTCLastMessage &) const
{
    std::string buffer;
    buffer.push_back(true);
    if (const auto &r = writeInternal(buffer); !r)
    {
        return tl::unexpected(r.error());
    }
    return {};
}

tl::expected<ProcessMappingOfBMIFile, std::string> IPCManagerBS::createSharedMemoryBMIFile(BMIFile &bmiFile)
{
    ProcessMappingOfBMIFile sharedFile{};
#ifdef _WIN32

    std::string mappingName = bmiFile.filePath;
    for (char &c : mappingName)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    if (bmiFile.fileSize == UINT32_MAX)
    {
        const HANDLE hFile = CreateFileA(bmiFile.filePath.c_str(), GENERIC_READ,
                                         0, // no sharing during setup
                                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            return tl::unexpected(getErrorString());
        }

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize))
        {
            return tl::unexpected(getErrorString());
        }

        sharedFile.mapping =
            CreateFileMappingA(hFile, nullptr, PAGE_READONLY, fileSize.HighPart, fileSize.LowPart, mappingName.c_str());

        CloseHandle(hFile);

        if (!sharedFile.mapping)
        {
            return tl::unexpected(getErrorString());
        }

        bmiFile.fileSize = fileSize.QuadPart;
        return sharedFile;
    }

    // 1) Open the existing file‐mapping object (must have been created by another process)
    sharedFile.mapping = OpenFileMappingA(FILE_MAP_READ,      // read‐only access
                                          FALSE,              // do not inherit handle
                                          mappingName.c_str() // name of mapping
    );

    if (sharedFile.mapping == nullptr)
    {
        return tl::unexpected(getErrorString());
    }

    return sharedFile;
#else
    const int fd = open(bmiFile.filePath.data(), O_RDONLY);
    if (fd == -1)
    {
        return tl::unexpected(getErrorString());
    }
    if (bmiFile.fileSize == UINT32_MAX)
    {
        struct stat st;
        if (fstat(fd, &st) == -1)
        {
            return tl::unexpected(getErrorString());
        }

        bmiFile.fileSize = st.st_size;
    }
    void *mapping = mmap(nullptr, bmiFile.fileSize, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (close(fd) == -1)
    {
        return tl::unexpected(getErrorString());
    }
    if (mapping == MAP_FAILED)
    {
        return tl::unexpected(getErrorString());
    }
    sharedFile.file = std::string_view(static_cast<char *>(mapping), bmiFile.fileSize);
    return sharedFile;
#endif
}

tl::expected<void, std::string> IPCManagerBS::closeBMIFileMapping(
    const ProcessMappingOfBMIFile &processMappingOfBMIFile)
{
#ifdef _WIN32
    CloseHandle(processMappingOfBMIFile.mapping);
#else
    if (munmap((void *)processMappingOfBMIFile.file.data(), processMappingOfBMIFile.file.size()) == -1)
    {
        return tl::unexpected(getErrorString());
    }
#endif
    return {};
}

void IPCManagerBS::closeConnection() const
{
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(fd));
#else
    close(fd);
#endif
}

} // namespace N2978