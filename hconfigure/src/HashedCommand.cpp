
#ifdef USE_HEADER_UNITS

import "HashedCommand.hpp";

#ifdef USE_COMMAND_HASH
import "rapidhash/rapidhash.h";
#endif

#else
#include "HashedCommand.hpp"

#ifdef USE_COMMAND_HASH
#include "rapidhash/rapidhash.h"
#endif
#endif

void HashedCommand::setCommand(string command_)
{
    command = std::move(command_);
#ifdef USE_COMMAND_HASH
    hash = rapidhash(command.c_str(), command.size());
#endif
}

string HashedCommand::getCommand() const
{
    return command;
}

Value HashedCommand::getHash() const
{

#ifdef USE_COMMAND_HASH

    return Value(hash);

#else

    return Value(svtogsr(command));

#endif
}