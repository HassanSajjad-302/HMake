
#ifdef USE_HEADER_UNITS
#include "BuildSystemFunctions.hpp"
import <iostream>;
import "PlatformSpecific.hpp";
import "rapidjson/prettywriter.h";
import "rapidjson/writer.h";
import <cstdio>;
#else
#include "BuildSystemFunctions.hpp"
#include "PlatformSpecific.hpp"
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
        for (; (from < from_end) && (to < to_limit); from += 2, ++to)
        {
            /*Input the Data*/
            /* As the input 16 bits may not fill the wchar_t object
             * Initialise it so that zero out all its bit's. This
             * is important on systems with 32bit wchar_t objects.
             */
            (*to) = L'\0';

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

        return ((from > from_end) ? partial : ok);
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
        for (; (from < from_end) && (to < to_limit); ++from, to += 2)
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

        return ((to > to_limit) ? partial : ok);
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

void prettyWritePValueToFile(const pstring_view fileName, PValue &value)
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

void writePValueToFile(const pstring_view fileName, PValue &value)
{
    RHPOStream stream(fileName);
    rapidjson::Writer<RHPOStream, UTF8<>, UTF8<>> writer(stream, nullptr);
    if (!value.Accept(writer))
    {
        // TODO Check what error
        printErrorMessage(FORMAT("Error Happened in parsing file {}\n", fileName.data()));
        throw std::exception{};
    }
}

#ifndef _WIN32
#define fopen_s(pFile, filename, mode) ((*(pFile)) = fopen((filename), (mode))) == NULL
#endif

unique_ptr<pchar[]> readPValueFromFile(const pstring_view fileName, PDocument &document)
{
    // Read whole file into a buffer
    FILE *fp;
    fopen_s(&fp, fileName.data(), "r");
    fseek(fp, 0, SEEK_END);
    const size_t filesize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unique_ptr<pchar[]> buffer = std::make_unique<pchar[]>(filesize + 1);
    const size_t readLength = fread(buffer.get(), 1, filesize, fp);
    fclose(fp);

    // TODO
    //  What should this be for wchar_t
    buffer[readLength] = '\0';

    // In situ parsing the buffer into d, buffer will also be modified
    document.ParseInsitu(buffer.get());
    return buffer;
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

size_t pvalueIndexInSubArray(const PValue &pvalue, const PValue &element)
{
    for (size_t i = 0; i < pvalue.Size(); ++i)
    {
        if (element == pvalue[i][0])
        {
            return i;
        }
    }
    return UINT64_MAX;
}