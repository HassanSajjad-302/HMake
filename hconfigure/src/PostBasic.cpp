
#ifdef USE_HEADER_UNITS
import "PostBasic.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import "Utilities.hpp";
import <fstream>;
#else
#include "PostBasic.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <fstream>
#endif

using std::ofstream, fmt::format;

pstring getThreadId()
{
    pstring threadId;
    auto myId = std::this_thread::get_id();
    pstringstream ss;
    ss << myId;
    threadId = ss.str();
    return threadId;
}

PostBasic::PostBasic(const BuildTool &buildTool, const pstring &commandFirstHalf, pstring printCommandFirstHalf,
                     const pstring &buildCacheFilesDirPath, const pstring &fileName, const PathPrint &pathPrint,
                     bool isTarget_)
    : isTarget{isTarget_}
{
    // TODO
    // This does process setup for the output and error stream in a bit costly manner by redirecting it to files and
    // then reading from those files. More native APIS should be used. Also if there could be a way to preserve the
    // coloring of the output.
    pstring str = isTarget ? "_t" : "";

    pstring outputFileName = buildCacheFilesDirPath + fileName + "_output" + str;
    pstring errorFileName = buildCacheFilesDirPath + fileName + "_error" + str;

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
    pstring cmdCharLimitMitigateCommand = addQuotes((toolPath.make_preferred().*toPStr)()) + " @" +
                                          addQuotes((responseFile.*toPStr)()) + "> " + addQuotes(outputFileName) +
                                          " 2>" + addQuotes(errorFileName);
    exitStatus = system(addQuotes(cmdCharLimitMitigateCommand).c_str());
#else
    pstring finalCompileCommand = (buildTool.bTPath.*toPStr)() + " " + commandFirstHalf + "> " +
                                  addQuotes(outputFileName) + " 2>" + addQuotes(errorFileName);
    exitStatus = system(finalCompileCommand.c_str());
#endif
    if (exitStatus == EXIT_SUCCESS)
    {
        commandSuccessOutput = fileToPString(outputFileName);
        commandErrorOutput = fileToPString(errorFileName);
    }
    else
    {
        commandSuccessOutput = fileToPString(outputFileName);
        commandErrorOutput = fileToPString(errorFileName);
    }
}

void PostBasic::executePrintRoutine(uint32_t color, bool printOnlyOnError) const
{
    if (!printOnlyOnError)
    {
        preintMessageColor(fmt::format("{}", printCommand + " " + getThreadId() + "\n"), color);
        if (exitStatus == EXIT_SUCCESS)
        {
            if (!commandSuccessOutput.empty())
            {
                preintMessageColor(fmt::format("{}", commandSuccessOutput + "\n"),
                                   static_cast<int>(fmt::color::light_green));
            }
            if (!commandErrorOutput.empty())
            {
                printErrorMessageColor(fmt::format("{}", commandErrorOutput + "\n"),
                                       static_cast<int>(fmt::color::light_green));
            }
        }
    }
    if (exitStatus != EXIT_SUCCESS)
    {
        if (!commandSuccessOutput.empty())
        {
            printErrorMessageColor(fmt::format("{}", commandSuccessOutput + "\n"), settings.pcSettings.toolErrorOutput);
        }
        if (!commandErrorOutput.empty())
        {
            printErrorMessageColor(fmt::format("{}", commandErrorOutput + "\n"), settings.pcSettings.toolErrorOutput);
        }
    }
}

PostCompile::PostCompile(const CppSourceTarget &target_, const BuildTool &buildTool, const pstring &commandFirstHalf,
                         pstring printCommandFirstHalf, const pstring &buildCacheFilesDirPath, const pstring &fileName,
                         const PathPrint &pathPrint)
    : PostBasic(buildTool, commandFirstHalf, std::move(printCommandFirstHalf), buildCacheFilesDirPath, fileName,
                pathPrint, false),
      target{const_cast<CppSourceTarget &>(target_)}
{
}

bool PostCompile::ignoreHeaderFile(pstring_view str)
{
    //  Premature Optimization Hahacd
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

    for (const InclNode &inclNode : target.requirementIncludes)
    {
        if (inclNode.ignoreHeaderDeps && equivalent(inclNode.node->filePath, path(str).parent_path()))
        {
            return true;
        }
    }
    return false;
}

void PostCompile::parseDepsFromMSVCTextOutput(SourceNode &sourceNode, pstring &output, PValue &headerDepsJson)
{
    vector<pstring> outputLines = split(output, "\n");
    pstring includeFileNote = "Note: including file:";

    if (sourceNode.ignoreHeaderDeps)
    {
        for (auto iter = outputLines.begin(); iter != outputLines.end();)
        {
            if (iter->contains(includeFileNote))
            {
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
    }
    else
    {
        for (auto iter = outputLines.begin(); iter != outputLines.end();)
        {
            if (iter->contains(includeFileNote))
            {
                size_t pos = iter->find_first_not_of(includeFileNote);
                pos = iter->find_first_not_of(" ", pos);

                // TODO
                // If compile-command is all lower-cased, then this might not be needed
                if (!ignoreHeaderFile(pstring_view(iter->begin() + (int)pos, iter->end())))
                {
                    for (auto it = iter->begin() + (int)pos; it != iter->end(); ++it)
                    {
                        *it = tolower(*it);
                    }

                    headerDepsJson.PushBack(
                        PValue(PStringRef(iter->c_str() + pos, iter->size()), sourceNode.sourceNodeAllocator),
                        sourceNode.sourceNodeAllocator);
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
    }

    if (settings.ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput)
    {
        if (!outputLines.empty())
        {
            outputLines.erase(outputLines.begin());
        }
    }

    pstring treatedOutput; // Output With All information of include files removed.
    for (const auto &i : outputLines)
    {
        treatedOutput += i + "\n";
    }
    if (!treatedOutput.empty())
    {
        treatedOutput.pop_back();
    }
    output = std::move(treatedOutput);
}

void PostCompile::parseDepsFromGCCDepsOutput(SourceNode &sourceNode, PValue &headerDepsJson)
{
    if (!sourceNode.ignoreHeaderDeps)
    {
        pstring headerFileContents = fileToPString(target.buildCacheFilesDirPath +
                                                   (path(sourceNode.node->filePath).filename().*toPStr)() + ".d");
        vector<pstring> headerDeps = split(headerFileContents, "\n");

        // First 2 lines are skipped as these are .o and .cpp file.
        // If the file is preprocessed, it does not generate the extra line
        auto endIt = headerDeps.end() - (sourceNode.target->compileTargetType == TargetType::LIBRARY_OBJECT ? 1 : 0);
        for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
        {
            size_t pos = iter->find_first_not_of(" ");
            pstring headerDep = iter->substr(pos, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos);
            if (!ignoreHeaderFile(headerDep))
            {
                headerDepsJson.PushBack(PValue(headerDep.c_str(), headerDep.size(), sourceNode.sourceNodeAllocator),
                                        sourceNode.sourceNodeAllocator);
            }
        }
    }
}

void PostCompile::parseHeaderDeps(SourceNode &sourceNode, unsigned short round)
{
    PValue &headerDepsJson = (*sourceNode.sourceJson)[2];
    headerDepsJson.Clear();

    if (target.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(sourceNode, commandSuccessOutput, headerDepsJson);
        // In case of GenerateSMRules header-file info is printed to stderr instead of stout. Just one more wrinkle.
        // Hahaha
        parseDepsFromMSVCTextOutput(sourceNode, commandErrorOutput, headerDepsJson);
    }
    else
    {
        if (exitStatus == EXIT_SUCCESS)
        {
            parseDepsFromGCCDepsOutput(sourceNode, headerDepsJson);
        }
    }
}
