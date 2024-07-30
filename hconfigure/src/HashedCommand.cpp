
#ifdef USE_HEADER_UNITS

import "HashedCommand.hpp";

#ifdef USE_COMMAND_HASH
import "rapidhash.h";
#endif

#else
#include "HashedCommand.hpp"

#ifdef USE_COMMAND_HASH
#include "rapidhash.h"
#endif
#endif

void HashedCommand::setCommand(string command_)
{
    command = command_;
#ifdef USE_COMMAND_HASH
    hash = rapidhash(command.c_str(), command.size());
#endif
}

string HashedCommand::getCommand() const
{
    return command;
}

PValue HashedCommand::getHash() const
{

#ifdef USE_COMMAND_HASH

    return PValue(hash);

#else

    return PValue(ptoref(command));

#endif
}