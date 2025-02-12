
#ifndef OBJECTFILE_HPP
#define OBJECTFILE_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "Node.hpp";
import "Settings.hpp";
#else
#include "BTarget.hpp"
#include "Node.hpp"
#include "Settings.hpp"
#endif

class ObjectFile : public BTarget
{
public:
    ObjectFile();
    ObjectFile(bool add0, bool add1, bool add2);
    Node *objectFileOutputFilePath = nullptr;
    virtual string getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const = 0;
};

#endif //OBJECTFILE_HPP
