
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

#ifdef USE_COMMAND_HASH
uint64_t HashedCommand::getHash() const
{
    return hash;
}
#else
string_view HashedCommand::getHash() const
{
    return command;
}
#endif