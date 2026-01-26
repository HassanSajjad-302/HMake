
#include "HashedCommand.hpp"
#ifdef USE_COMMAND_HASH
#include "rapidhash/rapidhash.h"
#endif

void HashedCommand::setCommand(string_view command_)
{
#ifdef USE_COMMAND_HASH
    hash = rapidhash(command_.data(), command_.size());
#else
    command = std::move(command_);
#endif
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