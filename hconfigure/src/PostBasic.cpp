#include "PostBasic.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <fstream>

using std::ofstream;

string getThreadId()
{
    string threadId;
    auto myId = std::this_thread::get_id();
    std::stringstream ss;
    ss << myId;
    threadId = ss.str();
    return threadId;
}

PostBasic::PostBasic(const BuildTool &buildTool, const string &commandFirstHalf, string printCommandFirstHalf,
                     const string &buildCacheFilesDirPath, const string &fileName, const PathPrint &pathPrint,
                     bool isTarget_)
    : isTarget{isTarget_}
{
    string str = isTarget ? "_t" : "";

    string outputFileName = buildCacheFilesDirPath + fileName + "_output" + str;
    string errorFileName = buildCacheFilesDirPath + fileName + "_error" + str;

    if (pathPrint.printLevel != PathPrintLevel::NO)
    {
        printCommandFirstHalf +=
            "> " + getReducedPath(outputFileName, pathPrint) + " 2>" + getReducedPath(errorFileName, pathPrint);
    }

    printCommand = std::move(printCommandFirstHalf);

#ifdef _WIN32
    path responseFile = path(buildCacheFilesDirPath + fileName + ".response");
    ofstream(responseFile) << commandFirstHalf;

    path toolPath = buildTool.bTPath;
    string cmdCharLimitMitigateCommand = addQuotes(toolPath.make_preferred().string()) + " @" +
                                         addQuotes(responseFile.generic_string()) + "> " + addQuotes(outputFileName) +
                                         " 2>" + addQuotes(errorFileName);
    exitStatus = system(addQuotes(cmdCharLimitMitigateCommand).c_str());
#else
    string finalCompileCommand = buildTool.bTPath.generic_string() + " " + commandFirstHalf + "> " +
                                 addQuotes(outputFileName) + " 2>" + addQuotes(errorFileName);
    exitStatus = system(finalCompileCommand.c_str());
#endif
    if (exitStatus == EXIT_SUCCESS)
    {
        commandSuccessOutput = file_to_string(outputFileName);
    }
    else
    {
        commandSuccessOutput = file_to_string(outputFileName);
        commandErrorOutput = file_to_string(errorFileName);
    }
}

void PostBasic::executePrintRoutine(uint32_t color, bool printOnlyOnError) const
{
    if (!printOnlyOnError)
    {
        print(fg(static_cast<fmt::color>(color)), pes, printCommand + " " + getThreadId() + "\n");
        if (exitStatus == EXIT_SUCCESS)
        {
            if (!commandSuccessOutput.empty())
            {
                print(fg(static_cast<fmt::color>(color)), pes, commandSuccessOutput + "\n");
            }
            if (!commandErrorOutput.empty())
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes,
                      commandErrorOutput + "\n");
            }
        }
    }
    if (exitStatus != EXIT_SUCCESS)
    {
        if (!commandSuccessOutput.empty())
        {
            print(fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes, commandSuccessOutput + "\n");
        }
        if (!commandErrorOutput.empty())
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)), pes,
                  commandErrorOutput + "\n");
        }
    }
}

PostCompile::PostCompile(const CppSourceTarget &target_, const BuildTool &buildTool, const string &commandFirstHalf,
                         string printCommandFirstHalf, const string &buildCacheFilesDirPath, const string &fileName,
                         const PathPrint &pathPrint)
    : target{const_cast<CppSourceTarget &>(target_)},
      PostBasic(buildTool, commandFirstHalf, std::move(printCommandFirstHalf), buildCacheFilesDirPath, fileName,
                pathPrint, false)
{
}

bool PostCompile::checkIfFileIsInEnvironmentIncludes(const string &str)
{
    //  Premature Optimization Haha
    // TODO:
    //  Add a key in hconfigure that informs hbuild that the library isn't to be
    //  updated, so includes from the directories coming from it aren't mentioned
    //  in targetCache and neither are those libraries checked for an edit for
    //  faster startup times.

    // If a file is in environment includes, it is not marked as dependency as an
    // optimization. If a file is in subdirectory of environment include, it is
    // still marked as dependency. It is not checked if any of environment
    // includes is related(equivalent, subdirectory) with any of normal includes
    // or vice-versa.

    for (const Node *node : target.standardIncludes)
    {
        if (node->ignoreIncludes && equivalent(node->filePath, path(str).parent_path()))
        {
            return true;
        }
    }
    for (const Node *node : target.requirementIncludes)
    {
        if (node->ignoreIncludes && equivalent(node->filePath, path(str).parent_path()))
        {
            return true;
        }
    }
    return false;
}

void PostCompile::parseDepsFromMSVCTextOutput(SourceNode &sourceNode)
{
    vector<string> outputLines = split(commandSuccessOutput, "\n");
    string includeFileNote = "Note: including file:";

    for (auto iter = outputLines.begin(); iter != outputLines.end();)
    {
        if (iter->contains(includeFileNote))
        {
            size_t pos = iter->find_first_not_of(includeFileNote);
            pos = iter->find_first_not_of(" ", pos);
            iter->erase(iter->begin(), iter->begin() + (int)pos);
            if (!checkIfFileIsInEnvironmentIncludes(*iter))
            {
                sourceNode.headerDependencies.emplace(Node::getNodeFromString(*iter, true));
            }
            if (settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
            {
                iter = outputLines.erase(iter);
            }
        }
        else
        {
            ++iter;
        }
    }

    if (settings.ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput)
    {
        outputLines.erase(outputLines.begin());
    }

    string treatedOutput; // Output With All information of include files removed.
    for (const auto &i : outputLines)
    {
        treatedOutput += i + "\n";
    }
    if (!treatedOutput.empty())
    {
        treatedOutput.pop_back();
    }
    commandSuccessOutput = std::move(treatedOutput);
}

void PostCompile::parseDepsFromGCCDepsOutput(SourceNode &sourceNode)
{
    string headerFileContents =
        file_to_string(target.buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".d");
    vector<string> headerDeps = split(headerFileContents, "\n");

    // First 2 lines are skipped as these are .o and .cpp file.
    // If the file is preprocessed, it does not generate the extra line
    auto endIt = headerDeps.end() - (sourceNode.target->compileTargetType == TargetType::OBJECT ? 1 : 0);
    for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
    {
        size_t pos = iter->find_first_not_of(" ");
        string headerDep = iter->substr(pos, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos);
        if (!checkIfFileIsInEnvironmentIncludes(headerDep))
        {
            sourceNode.headerDependencies.emplace(Node::getNodeFromString(headerDep, true));
        }
    }
}

void PostCompile::executePostCompileRoutineWithoutMutex(SourceNode &sourceNode)
{
    // Clearing old header-deps and adding the new ones.
    sourceNode.headerDependencies.clear();
    if (exitStatus == EXIT_SUCCESS)
    {
        if (target.compiler.bTFamily == BTFamily::MSVC)
        {
            parseDepsFromMSVCTextOutput(sourceNode);
        }
        else if (target.compiler.bTFamily == BTFamily::GCC && exitStatus == EXIT_SUCCESS)
        {
            parseDepsFromGCCDepsOutput(sourceNode);
        }
    }
}
