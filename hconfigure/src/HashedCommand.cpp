
#ifdef USE_HEADER_UNITS

import "HashedCommand.hpp";

#ifdef USE_COMMAND_HASH
import "komihash.h";
#endif

#else
#include "HashedCommand.hpp"

#ifdef USE_COMMAND_HASH
#include "komihash.h"
#endif
#endif

void HashedCommand::setCommand(string command_)
{
    command = command_;
#ifdef USE_COMMAND_HASH
    hash = komihash(command.c_str(), command.size(), 0);
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