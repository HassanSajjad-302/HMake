
#ifndef HMAKE_CUSTOMCODEGENERATOR_HPP
#define HMAKE_CUSTOMCODEGENERATOR_HPP

#include "BTarget.hpp"
#include "LOAT.hpp"
#include "Node.hpp"
#include "rapidhash/rapidhash.h"

/// Minimal asynchronous generated-header target.
///
/// This example currently treats every successful invocation as an output change. A generator that can compare its
/// previous and new outputs may instead apply HMake's unchanged-output cutoff: after a successful byte-for-byte or
/// semantic comparison, set `updateStatus` to `UPDATE_NOT_NEEDED`. That single assignment is the complete cutoff
/// operation; HMake conservatively re-evaluates affected dependents and preserves the cached `completionTime`. See
/// "Unchanged-output cutoff" in the project README.
struct HeaderGen : BTarget
{
    string command;
    LOAT *codeGenerator;
    Node *myBuildDir;
    Node *sourceNode;
    Node *outputHeader;
    HeaderGen(const string &name, LOAT *codeGenerator_, const string &macroName, const string &macroValueFile);
    void setUpdateStatus() override;
    bool isEventRegistered(Builder &builder) override;
    bool isEventCompleted(Builder &builder, string_view message) override;
    void writeConfigCacheAtConfigTime(string &buffer) override;
};

struct LlvmHeaderGen : BTarget
{
    LOAT *codeGenerator;
    Node *sourceNode;
    string command;

    LlvmHeaderGen(const string &name, LOAT *codeGenerator_, const string &filePath, const string &command_)
        : BTarget(name, rapidhash(name.data(), name.size()), false, BTargetType::UNKNOWN), codeGenerator(codeGenerator_),
          command(command_)
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            const Node *node = Node::getNode(filePath, true, false);
            string *configBuffer = new string{};
            writeNode(*configBuffer, node);
            bTargetCaches[cacheIndex].configCache = string_view{configBuffer->data(), configBuffer->size()};
        }
        else
        {
            uint32_t bytesRead = 0;
            sourceNode = readHalfNode(bTargetCaches[cacheIndex].getBuildCache().data(), bytesRead);
        }
    }
};

#endif // HMAKE_CUSTOMCODEGENERATOR_HPP
