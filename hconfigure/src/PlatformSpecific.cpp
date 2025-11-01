
#include "PlatformSpecific.hpp"
#include "BuildSystemFunctions.hpp"
#include "TargetCache.hpp"
#include "lz4/lib/lz4.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <utility>
#ifdef WIN32
#include <Windows.h>
#endif

// Copied from https://stackoverflow.com/a/208431
class UTF16Facet : public std::codecvt<wchar_t, char, std::char_traits<wchar_t>::state_type>
{
    typedef std::codecvt<wchar_t, char, std::char_traits<wchar_t>::state_type> MyType;
    typedef state_type state_type;
    typedef result result;

    /* This function deals with converting data from the input stream into the internal stream.*/
    /*
     * from, from_end:  Points to the beginning and end of the input that we are converting 'from'.
     * to,   to_limit:  Points to where we are writing the conversion 'to'
     * from_next:       When the function exits this should have been updated to point at the next location
     *                  to read from. (ie the first unconverted input character)
     * to_next:         When the function exits this should have been updated to point at the next location
     *                  to write to.
     *
     * status:          This indicates the status of the conversion.
     *                  possible values are:
     *                  error:      An error occurred the bad file bit will be set.
     *                  ok:         Everything went to plan
     *                  partial:    Not enough input data was supplied to complete any conversion.
     *                  nonconv:    no conversion was done.
     */
    result do_in(state_type &s, const char *from, const char *from_end, const char *&from_next, wchar_t *to,
                 wchar_t *to_limit, wchar_t *&to_next) const override
    {
        // Loop over both the input and output array/
        for (; from < from_end && to < to_limit; from += 2, ++to)
        {
            /*Input the Data*/
            /* As the input 16 bits may not fill the wchar_t object
             * Initialise it so that zero out all its bit's. This
             * is important on systems with 32bit wchar_t objects.
             */
            *to = L'\0';

            /* Next read the data from the input stream into
             * wchar_t object. Remember that we need to copy
             * into the bottom 16 bits no matter what size the
             * the wchar_t object is.
             */
            reinterpret_cast<char *>(to)[0] = from[0];
            reinterpret_cast<char *>(to)[1] = from[1];
        }
        from_next = from;
        to_next = to;

        return from > from_end ? partial : ok;
    }

    /* This function deals with converting data from the internal stream to a C/C++ file stream.*/
    /*
     * from, from_end:  Points to the beginning and end of the input that we are converting 'from'.
     * to,   to_limit:  Points to where we are writing the conversion 'to'
     * from_next:       When the function exits this should have been updated to point at the next location
     *                  to read from. (ie the first unconverted input character)
     * to_next:         When the function exits this should have been updated to point at the next location
     *                  to write to.
     *
     * status:          This indicates the status of the conversion.
     *                  possible values are:
     *                  error:      An error occurred the bad file bit will be set.
     *                  ok:         Everything went to plan
     *                  partial:    Not enough input data was supplied to complete any conversion.
     *                  nonconv:    no conversion was done.
     */
    result do_out(state_type &state, const wchar_t *from, const wchar_t *from_end, const wchar_t *&from_next, char *to,
                  char *to_limit, char *&to_next) const override
    {
        for (; from < from_end && to < to_limit; ++from, to += 2)
        {
            /* Output the Data */
            /* NB I am assuming the characters are encoded as UTF-16.
             * This means they are 16 bits inside a wchar_t object.
             * As the size of wchar_t varies between platforms I need
             * to take this into consideration and only take the bottom
             * 16 bits of each wchar_t object.
             */
            to[0] = reinterpret_cast<const char *>(from)[0];
            to[1] = reinterpret_cast<const char *>(from)[1];
        }
        from_next = from;
        to_next = to;

        return to > to_limit ? partial : ok;
    }
};

// Rapid Helper PlatformSpecific OStream
struct RHPOStream
{
    FILE *fp = nullptr;
    RHPOStream(string_view fileName);
    ~RHPOStream();
    typedef char Ch;
    void Put(Ch c) const;
    void Flush();
};

RHPOStream::RHPOStream(const string_view fileName)
{
    fp = fopen(fileName.data(), "wb");
}

RHPOStream::~RHPOStream()
{
    int result = fclose(fp);
    if (result != 0)
    {
        printErrorMessage("Error closing the file \n");
    }
}

void RHPOStream::Put(const Ch c) const
{
    fputc(c, fp);
}

void RHPOStream::Flush()
{
    if (int result = fflush(fp); result != 0)
    {
        printErrorMessage("Error flushing the file \n");
    }
}

vector<char> readBufferFromFile(const string &fileName)
{
    vector<char> fileBuffer;
    FILE *fp;

#ifdef WIN32
    fopen_s(&fp, fileName.data(), "rb");
#else
    fp = fopen(fileName.c_str(), "r");
#endif

    fseek(fp, 0, SEEK_END);
    const size_t filesize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fileBuffer.resize(filesize);
    const uint64_t readLength = fread(fileBuffer.data(), 1, filesize, fp);
    fclose(fp);
    return fileBuffer;
}

vector<char> readBufferFromCompressedFile(const string &fileName)
{
#ifndef USE_JSON_FILE_COMPRESSION
    return readBufferFromFile(fileName);
#else
    vector<char> compressedBuffer = readBufferFromFile(fileName);
    vector<char> fileBuffer;
    fileBuffer.resize(*reinterpret_cast<uint64_t *>(compressedBuffer.data()));

    const int decompressSize =
        LZ4_decompress_safe(&compressedBuffer[8], fileBuffer.data(), compressedBuffer.size() - 8, fileBuffer.size());

    if (decompressSize < 0)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        errorExit();
    }

    if (fileBuffer.size() != decompressSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        errorExit();
    }

    return fileBuffer;

#endif
}

string getThreadId()
{
    const auto myId = std::this_thread::get_id();
    std::stringstream ss;
    ss << myId;
    return ss.str() + '\n';
}

void readConfigCache()
{
    const uint32_t bufferSize = configCacheGlobal.size();
    uint32_t bufferRead = 0;

    uint32_t count = 0;
    const char *ptr = configCacheGlobal.data();
    while (bufferRead != bufferSize)
    {
        FileTargetCache fileCacheTarget;

        fileCacheTarget.name = readStringView(ptr, bufferRead);
        fileCacheTarget.configCache = readStringView(ptr, bufferRead);

        fileTargetCaches.emplace_back(fileCacheTarget);
        nameToIndexMap.emplace(fileCacheTarget.name, count);

        ++count;
    }
}

void readBuildCache()
{
    const uint32_t bufferSize = buildCacheGlobal.size();
    uint32_t bufferRead = 0;

    const char *ptr = buildCacheGlobal.data();
    for (FileTargetCache &fileCacheTarget : fileTargetCaches)
    {
        fileCacheTarget.buildCache = readStringView(ptr, bufferRead);
    }

    if (bufferRead != bufferSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
}

void writeConfigBuffer(vector<char> &buffer)
{
    for (FileTargetCache &fileCacheTarget : fileTargetCaches)
    {
        writeStringView(buffer, fileCacheTarget.name);
        writeStringView(buffer, fileCacheTarget.configCache);
    }
}

void writeBuildBuffer(vector<char> &buffer)
{
    buffer = vector<char>();
    for (const FileTargetCache &fileCacheTarget : fileTargetCaches)
    {
        const uint32_t currentSize = buffer.size();
        // reserve space of 4bytes.
        writeUint32(buffer, 0);
        if (fileCacheTarget.targetCache)
        {
            fileCacheTarget.targetCache->writeBuildCache(buffer);
        }
        else
        {
            buffer.insert(buffer.end(), fileCacheTarget.buildCache.begin(), fileCacheTarget.buildCache.end());
        }
        const uint32_t size = buffer.size() - (currentSize + 4);
        *static_cast<uint32_t *>(static_cast<void *>(&buffer[currentSize])) = size;
    }
}

#ifndef _WIN32
#define fopen_s(pFile, filename, mode) ((*(pFile)) = fopen((filename), (mode))) == NULL
#endif

extern string GetLastErrorString();

// In configure mode only 2 files target-cache.json and nodes.json are written which are written at the end.
// While in build-mode TargetCacheDisWriteManager asynchronously writes these files multiple times as the data is
// updated. Hence, these file write is atomic in build mode
static void writeFileAtomically(const string &fileName, const char *buffer, uint64_t bufferSize, bool binary)
{
    const string str = fileName + ".tmp";
    if constexpr (bsMode == BSMode::BUILD)
    {
#ifdef WIN32
        // Open the existing file for writing, replacing its content
        const HANDLE hFile = CreateFile(str.c_str(),
                                        GENERIC_WRITE,         // Open for writing
                                        0,                     // Do not share
                                        NULL,                  // Default security
                                        CREATE_ALWAYS,         // Always create a new file (replace if exists)
                                        FILE_ATTRIBUTE_NORMAL, // Normal file
                                        NULL                   // No template
        );

        // Check if the file handle is valid
        if (hFile == INVALID_HANDLE_VALUE)
        {
            printErrorMessage(FORMAT("Failed to open file for writing. Error: {}\n", GetLastErrorString()));
        }

        // Content to write to the file
        DWORD bytesWritten;

        // Write to the file
        if (!WriteFile(hFile, buffer, bufferSize, &bytesWritten, nullptr))
        {
            printErrorMessage(FORMAT("Failed to write to file. Error: {}\n", GetLastErrorString()));
            CloseHandle(hFile);
        }

        if (!FlushFileBuffers(hFile))
        {
            printErrorMessage(FORMAT("Failed to flush file buffers. Error: {}\n", GetLastErrorString()));
        }

        if (bytesWritten != bufferSize)
        {
            printErrorMessage("Failed to write the full file\n");
        }

        // Close the file handle
        CloseHandle(hFile);

#else
        // For some reason, this does not work in Windows.
        if (binary)
        {
            std::ofstream f(str, std::ios::binary);
            f.write(buffer, bufferSize);
        }
        else
        {
            std::ofstream(str) << buffer;
        }
#endif
    }

    else
    {
        if (binary)
        {
            std::ofstream f(fileName, std::ios::binary);
            f.write(buffer, bufferSize);
        }
        else
        {
            std::ofstream(fileName) << buffer;
        }
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
#ifdef WIN32
        // Use ReplaceFile API which is designed for atomic replacement
        if (!ReplaceFileA(fileName.c_str(), // File to be replaced
                          str.c_str(),      // Replacement file
                          NULL,             // No backup
                          0,                // No flags
                          NULL,             // Reserved
                          NULL))            // Reserved
        {
            // If ReplaceFile fails (e.g., target doesn't exist), fall back to MoveFileEx
            if (!MoveFileExA(str.c_str(), fileName.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                printErrorMessage(FORMAT("Error:{}\n while writing file {}\n", GetLastErrorString(), fileName));
                fflush(stdout);
            }
        }
#else

        if (rename(str.c_str(), fileName.c_str()) != 0)
        {
            printMessage(
                FORMAT("Renaming File from {} to {} Not Successful. Error {}\n", str.c_str(), fileName.c_str(), errno));
        }

#endif
    }
}

void writeBufferToCompressedFile(const string &fileName, const vector<char> &fileBuffer)
{
#ifndef USE_JSON_FILE_COMPRESSION
    writeFileAtomically(fileName, fileBuffer.data(), fileBuffer.size(), true);
#else
    const uint64_t maxCompressedSize = LZ4_compressBound(fileBuffer.size());

    string compressed;
    compressed.resize(maxCompressedSize + 8);

    const int compressedSize =
        LZ4_compress_default(fileBuffer.data(), compressed.data() + 8, fileBuffer.size(), maxCompressedSize);

    // printMessage(FORMAT("\n{}\n{}\n", buffer.GetLength(), compressedSize + 8));
    if (!compressedSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        errorExit();
    }
    *reinterpret_cast<uint64_t *>(compressed.data()) = fileBuffer.size();

    writeFileAtomically(fileName, compressed.c_str(), compressedSize + 8, true);
#endif
}

bool compareStringsFromEnd(const string_view lhs, const string_view rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (int64_t j = lhs.size() - 1; j >= 0; --j)
    {
        if (lhs[j] != rhs[j])
        {
            return false;
        }
    }
    return true;
}

void lowerCaseOnWindows(char *ptr, const uint64_t size)
{
    if constexpr (os == OS::NT)
    {
        for (uint64_t i = 0; i < size; ++i)
        {
            ptr[i] = tolower(ptr[i]);
        }
    }
}

string getNormalizedPath(path filePath)
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
    return filePath.string();
}

// TODO
// Review this function and its usage.
bool childInParentPathNormalized(const string_view parent, const string_view child)
{
    if (child.size() < parent.size())
    {
        return false;
    }

    return compareStringsFromEnd(parent, string_view(child.data(), parent.size()));
}