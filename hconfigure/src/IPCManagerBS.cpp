#include "IPCManagerBS.hpp"
#include "Manager.hpp"
#include "Messages.hpp"
#include "expected.hpp"
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include "rapidhash.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace N2978
{

tl::expected<uint32_t, std::string> IPCManagerBS::readInternal(char (&buffer)[4096]) const
{
    const uint32_t serverReadStringSize = serverReadString.size();
    const uint32_t bytesRead = serverReadStringSize < BUFFERSIZE ? serverReadStringSize : BUFFERSIZE;
    for (uint32_t i = 0; i < bytesRead; ++i)
    {
        buffer[i] = serverReadString[i];
    }
    const_cast<std::string_view &>(serverReadString) =
        std::string_view{serverReadString.data() + bytesRead, serverReadString.size() - bytesRead};
    return bytesRead;
}

tl::expected<void, std::string> IPCManagerBS::writeInternal(const std::string_view buffer) const
{
#ifdef _WIN32
    const bool success = WriteFile(reinterpret_cast<HANDLE>(writeFd), // pipe handle
                                   buffer.data(),                     // message
                                   buffer.size(),                     // message length
                                   nullptr,                           // bytes written
                                   nullptr);                          // not overlapped
    if (!success)
    {
        return tl::unexpected(getErrorString());
    }
#else
    if (const auto &r = writeAll(writeFd, buffer.data(), buffer.size()); !r)
    {
        return tl::unexpected(r.error());
    }
#endif
    return {};
}

IPCManagerBS::IPCManagerBS(const uint64_t writeFd_) : writeFd(writeFd_)
{
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

        const auto &fileSizeExpected = readUInt32FromPipe(buffer, bytesRead, bytesProcessed);
        if (!fileSizeExpected)
        {
            return tl::unexpected(fileSizeExpected.error());
        }

        messageType = CTB::LAST_MESSAGE;

        auto &[fileSize] = getInitializedObjectFromBuffer<CTBLastMessage>(ctbBuffer);

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

} // namespace N2978