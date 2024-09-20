
#ifdef USE_HEADER_UNITS
#include "BuildSystemFunctions.hpp"
import <iostream>;
import "PlatformSpecific.hpp";
import "lz4.h";
import "rapidjson/prettywriter.h";
import "rapidjson/writer.h";
import <cstdio>;
#else
#include "BuildSystemFunctions.hpp"
#include "PlatformSpecific.hpp"
#include "lz4.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include <cstdio>
#include <iostream>
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

PStringRef ptoref(const pstring_view c)
{
    return PStringRef(c.data(), c.size());
}

RHPOStream::RHPOStream(const pstring_view fileName)
    : of(make_unique<std::basic_ofstream<pchar>>(fileName.data(), std::ios::binary))
{
    if constexpr (std::same_as<pchar, wchar_t>)
    {
        auto *unicodeFacet = new UTF16Facet();
        const std::locale unicodeLocale(std::cout.getloc(), unicodeFacet);
        of->imbue(unicodeLocale);
    }
}

void RHPOStream::Put(const Ch c) const
{
    of->put(c);
}

void RHPOStream::Flush()
{
}

void prettyWritePValueToFile(const pstring_view fileName, const PValue &value)
{
    RHPOStream stream(fileName);
    rapidjson::PrettyWriter<RHPOStream, UTF8<>, UTF8<>> writer(stream, nullptr);
    if (!value.Accept(writer))
    {
        // TODO Check what error
        printErrorMessage(FORMAT("Error Happened in parsing file {}\n", fileName.data()));
        throw std::exception{};
    }
}

size_t pvalueIndexInArray(const PValue &pvalue, const PValue &element)
{
    for (size_t i = 0; i < pvalue.Size(); ++i)
    {
        if (element == pvalue[i])
        {
            return i;
        }
    }
    return UINT64_MAX;
}

#ifndef _WIN32
#define fopen_s(pFile, filename, mode) ((*(pFile)) = fopen((filename), (mode))) == NULL
#endif

unique_ptr<vector<pchar>> readPValueFromFile(const pstring_view fileName, PDocument &document)
{
    // Read whole file into a buffer
    FILE *fp;
    fopen_s(&fp, fileName.data(), "r");
    fseek(fp, 0, SEEK_END);
    const size_t filesize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unique_ptr<vector<pchar>> buffer = std::make_unique<vector<pchar>>(filesize + 1);
    const size_t readLength = fread(buffer->begin().operator->(), 1, filesize, fp);
    fclose(fp);

    // TODO
    //  What should this be for wchar_t
    (*buffer)[readLength] = '\0';

    // In situ parsing the buffer into d, buffer will also be modified
    document.ParseInsitu(buffer->begin().operator->());
    return buffer;
}

void writePValueToFile(pstring_view fileName, const PValue &value)
{
    RHPOStream stream(fileName);
    rapidjson::Writer<RHPOStream> writer(stream, nullptr);
    if (!value.Accept(writer))
    {
        // TODO Check what error
        printErrorMessage(FORMAT("Error Happened in parsing file {}\n", fileName.data()));
        throw std::exception{};
    }
}

unique_ptr<vector<pchar>> readPValueFromCompressedFile(const pstring_view fileName, PDocument &document)
{
#ifndef USE_JSON_FILE_COMPRESSION
    return readPValueFromFile(fileName, document);
#else
    // Read whole file into a buffer
    unique_ptr<vector<pchar>> fileBuffer;
    uint64_t readLength;
    {
        FILE *fp;
        fopen_s(&fp, fileName.data(), "rb");
        fseek(fp, 0, SEEK_END);
        const size_t filesize = (size_t)ftell(fp);
        fseek(fp, 0, SEEK_SET);
        fileBuffer = std::make_unique<vector<pchar>>(filesize);
        readLength = fread(fileBuffer->begin().operator->(), 1, filesize, fp);
        fclose(fp);
    }

    uint64_t decompressedSize = *reinterpret_cast<uint64_t *>(&(*fileBuffer)[0]);
    unique_ptr<vector<pchar>> decompressedBuffer = std::make_unique<vector<pchar>>(decompressedSize + 1);

    int decompressSize =
        LZ4_decompress_safe(&(*fileBuffer)[8], &(*decompressedBuffer)[0], readLength - 8, decompressedSize);

    if (decompressSize < 0)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        exit(EXIT_FAILURE);
    }

    if (decompressedSize != decompressSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        exit(EXIT_FAILURE);
    }

    // TODO
    //  What should this be for wchar_t
    // Neccessary for in-situ parsing
    (*decompressedBuffer)[decompressSize] = '\0';

    // In situ parsing the buffer into d, buffer will also be modified
    document.ParseInsitu(&(*decompressedBuffer)[0]);

    /*rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    document.Accept(writer);
    std::ofstream("nodes2.json") << pstring(buffer.GetString(), buffer.GetLength());*/
    return decompressedBuffer;
#endif
}

void writePValueToCompressedFile(const pstring_view fileName, const PValue &value)
{
#ifndef USE_JSON_FILE_COMPRESSION
    writePValueToFile(fileName, value);
#else
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    value.Accept(writer);

    // printMessage(fmt::format("file {} \n{}\n", fileName, pstring(buffer.GetString(), buffer.GetLength())));
    pstring compressed;
    const uint64_t maxCompressedSize = LZ4_compressBound(buffer.GetLength());
    // first eight bytes for the compressed size
    compressed.reserve(maxCompressedSize + 8);

    int compressedSize = LZ4_compress_default(buffer.GetString(), const_cast<char *>(compressed.c_str()) + 8,
                                              buffer.GetLength(), maxCompressedSize);

    // printMessage(fmt::format("\n{}\n{}\n", buffer.GetLength(), compressedSize + 8));
    if (!compressedSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        exit(EXIT_FAILURE);
    }
    *reinterpret_cast<uint64_t *>(const_cast<char *>(compressed.c_str())) = buffer.GetLength();

    std::fstream file;
    file.open(fileName.data(), std::ios::out | std::ios::binary);
    file.write(compressed.c_str(), compressedSize + 8);
    file.close();
#endif
}

uint64_t pvalueIndexInSubArray(const PValue &pvalue, const PValue &element)
{
    for (uint64_t i = 0; i < pvalue.Size(); ++i)
    {
        if (element == pvalue[i][0])
        {
            return i;
        }
    }
    return UINT64_MAX;
}

uint64_t pvalueIndexInSubArrayConsidered(const PValue &pvalue, const PValue &element)
{
    const uint64_t old = currentTargetIndex;
    for (uint64_t i = currentTargetIndex; i < pvalue.Size(); ++i)
    {
        if (element == pvalue[i][0])
        {
            currentTargetIndex = i;
            return i;
        }
    }
    for (uint64_t i = 0; i < currentTargetIndex; ++i)
    {
        if (element == pvalue[i][0])
        {
            currentTargetIndex = i;
            return i;
        }
    }
    currentTargetIndex = old;
    return UINT64_MAX;
}

bool compareStringsFromEnd(pstring_view lhs, pstring_view rhs)
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

void lowerCasePStringOnWindows(pchar *ptr, uint64_t size)
{
    for (uint64_t i = 0; i < size; ++i)
    {
        ptr[i] = tolower(ptr[i]);
    }
}

bool childInParentPathRecursiveNormalized(pstring_view parent, pstring_view child)
{
    // Adding +1 so we -1 in while loop find_las_not_of so we always give one position before the last found slashc,
    // otherwise it will return same index and it will be a forever loop.
    uint64_t i = child.size() + 1;
    if (compareStringsFromEnd(parent, pstring_view(child.data(), i - 1)))
    {
        return true;
    }
    while (true)
    {
        if (parent.size() > i)
        {
            return false;
        }
        i = child.find_last_of(slashc, i - 1);
        if (i != pstring::npos)
        {
            if (compareStringsFromEnd(parent, pstring_view(child.data(), i)))
            {
                return true;
            }
        }
        else
        {
            i = 0;
        }
    }
}