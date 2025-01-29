#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "ConfigHelpers.hpp";
#else
#include "BuildSystemFunctions.hpp"
#include "ConfigHelpers.hpp"
#endif


void testVectorHasUniqueElementsIncrement(const Value &container, const string &containerName, uint64_t increment)
{
    for (uint64_t i = 0; i < container.Size(); i = i + increment)
    {
        uint8_t count = 0;
        for (uint64_t j = 0; j < container.Size(); j = j + increment)
        {
            if (getNodeForEquality(container[i])->filePath == getNodeForEquality(container[j])->filePath)
            {
                ++count;
            }
        }
        if (count != 1)
        {
            printErrorMessage(FORMAT("Repeat Value {} in container {}\n",
                                          getNodeForEquality(container[i])->filePath, containerName));
        }
    }
}
