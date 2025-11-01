
#include "HashedCommand.hpp"
#ifdef USE_COMMAND_HASH
#include "rapidhash/rapidhash.h"
#endif

void HashedCommand::setCommand(string_view command_)
{
    command = std::move(command_);
#ifdef USE_COMMAND_HASH
    hash = rapidhash(command.data(), command.size());
#endif
}

string_view HashedCommand::getCommand() const
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