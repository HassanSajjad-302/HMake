
#ifndef OBJECTFILE_HPP
#define OBJECTFILE_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
#else
#include "BTarget.hpp"
#endif

class ObjectFile : public BTarget
{
public:
    ObjectFile();
    ObjectFile(bool add0, bool add1, bool add2);
    Node *objectFileOutputFilePath = nullptr;
    virtual pstring getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const = 0;
};

#endif //OBJECTFILE_HPP
