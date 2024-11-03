
#ifdef USE_HEADER_UNITS
import "Utilities.hpp";
import "BuildSystemFunctions.hpp";
#include <fmt/format.h>
import <fstream>;
import <sstream>;
;
#else
#include "BuildSystemFunctions.hpp"
#include "Utilities.hpp"
#include "fmt/format.h"
#include <fstream>
#include <sstream>
#endif

using std::ifstream, fmt::format;

pstring addQuotes(const pstring_view pstr)
{
    return "\"" + pstring(pstr) + "\"";
}

pstring addEscapedQuotes(const pstring &pstr)
{
    const pstring q = R"(\")";
    return q + pstr + q;
}

pstring fileToPString(const pstring &file_name)
{
    ifstream file_stream{file_name};

    if (file_stream.fail())
    {
        // Error opening file.
        printErrorMessage(fmt::format("Error opening file {}\n", file_name));
        throw std::exception();
    }

    const opstringstream str_stream;
    file_stream >> str_stream.rdbuf(); // NOT str_stream << file_stream.rdbuf()

    if (file_stream.fail() && !file_stream.eof())
    {
        // Error reading file.
        printErrorMessage(fmt::format("Error reading file {}\n", file_name));
        throw std::exception();
    }

    return str_stream.str();
}

vector<pstring> split(pstring str, const pstring &token)
{
    vector<pstring> result;
    while (!str.empty())
    {
        if (const pstring::size_type index = str.find(token); index != pstring::npos)
        {
            result.emplace_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.empty())
            {
                result.emplace_back(str);
            }
        }
        else
        {
            result.emplace_back(str);
            str = "";
        }
    }
    return result;
}
