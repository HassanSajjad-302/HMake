
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

using std::ifstream, fmt::format, std::ostringstream;

string addQuotes(const string_view pstr)
{
    return "\"" + string(pstr) + "\"";
}

string addEscapedQuotes(const string &pstr)
{
    const string q = R"(\")";
    return q + pstr + q;
}

string fileToString(const string &file_name)
{
    ifstream file_stream{file_name};

    if (file_stream.fail())
    {
        // Error opening file.
        printErrorMessage(FORMAT("Error opening file {}\n", file_name));
    }

    const ostringstream str_stream;
    file_stream >> str_stream.rdbuf(); // NOT str_stream << file_stream.rdbuf()

    if (file_stream.fail() && !file_stream.eof())
    {
        // Error reading file.
        printErrorMessage(FORMAT("Error reading file {}\n", file_name));
    }

    return str_stream.str();
}

vector<string> split(string str, const string &token)
{
    vector<string> result;
    while (!str.empty())
    {
        if (const string::size_type index = str.find(token); index != string::npos)
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
