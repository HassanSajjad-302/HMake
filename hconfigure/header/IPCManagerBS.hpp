
#ifndef IPC_MANAGER_BS_HPP
#define IPC_MANAGER_BS_HPP

#include "Manager.hpp"
#include "Messages.hpp"

namespace N2978
{

// IPC Manager BuildSystem
class IPCManagerBS : public Manager
{
    friend tl::expected<IPCManagerBS, std::string> makeIPCManagerBS(std::string BMIIfHeaderUnitObjOtherwisePath);

    explicit IPCManagerBS(uint64_t fd_);

  public:
    IPCManagerBS(const IPCManagerBS &) = default;
    IPCManagerBS &operator=(const IPCManagerBS &) = default;
    IPCManagerBS(IPCManagerBS &&) = default;
    IPCManagerBS &operator=(IPCManagerBS &&) = default;
    tl::expected<void, std::string> registerManager(uint64_t serverFd, uint64_t completionKey) const;
    tl::expected<bool, std::string> completeConnection() const;
    tl::expected<void, std::string> receiveMessage(char (&ctbBuffer)[320], CTB &messageType) const;
    [[nodiscard]] tl::expected<void, std::string> sendMessage(const BTCModule &moduleFile) const;
    [[nodiscard]] tl::expected<void, std::string> sendMessage(const BTCNonModule &nonModule) const;
    [[nodiscard]] tl::expected<void, std::string> sendMessage(const BTCLastMessage &lastMessage) const;
    static tl::expected<ProcessMappingOfBMIFile, std::string> createSharedMemoryBMIFile(BMIFile &bmiFile);
    static tl::expected<void, std::string> closeBMIFileMapping(const ProcessMappingOfBMIFile &processMappingOfBMIFile);
    void closeConnection() const;
};

tl::expected<IPCManagerBS, std::string> makeIPCManagerBS(std::string BMIIfHeaderUnitObjOtherwisePath);
} // namespace N2978
#endif // IPC_MANAGER_BS_HPP
