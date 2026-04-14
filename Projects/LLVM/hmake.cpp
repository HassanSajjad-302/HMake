#include "Configure.hpp"
#include <set>

using std::set;
using std::filesystem::recursive_directory_iterator;

void editOutFilesRecursive(CppTarget *t, string directory, string extension, set<string> doNotInclude)
{
    set<Node *> noInclude;
    for (const string &str : doNotInclude)
    {
        noInclude.emplace(Node::getNodeNonNormalized(directory + slashc + str, true));
    }

    for (const auto &f : recursive_directory_iterator(srcNode->filePath / path(directory)))
    {
        if (f.is_regular_file() && f.path().extension() == extension)
        {
            string str = f.path().string();
            if (Node *n = Node::getNode(str, true); !noInclude.contains(n))
            {
                t->moduleFiles(n->filePath);
            }
        }
    }
}

void addLlvmDirectory(DSC<CppTarget> &target, const string &directory, const string &privatePrefix = "",
                      const bool addInReq = true, const bool addInUseReq = true)
{
    CppTarget &llvmCpp = target.getSourceTarget();

    if (addInReq)
    {
        if (const string privateDir = "llvm/lib/" + directory; exists(path(srcNode->filePath) / privateDir))
        {
            llvmCpp.moduleDirsRE(privateDir, ".*cpp")
                .privateHUDirsRE(privateDir, privatePrefix, ".*\\.h")
                .privateIncDirsRE(privateDir, privatePrefix, ".*\\.inc")
                .privateIncDirsRE(privateDir, privatePrefix, ".*\\.def");
        }

        if (const string cmakePrivateDir = "llvm/my-fork/lib/" + directory;
            exists(path(srcNode->filePath) / cmakePrivateDir))
        {
            llvmCpp.publicHUDirsRE(cmakePrivateDir, "", ".*\\.h")
                .publicIncDirsRE(cmakePrivateDir, "", ".*\\.inc")
                .publicIncDirsRE(cmakePrivateDir, "", ".*\\.def");
        }
    }

    if (addInUseReq)
    {
        if (const string publicDir = "llvm/include/llvm/" + directory; exists(path(srcNode->filePath) / publicDir))
        {
            llvmCpp.publicHUDirsRE(publicDir, "llvm/" + directory + "/", ".*\\.h")
                .publicIncDirsRE(publicDir, "llvm/" + directory + "/", ".*\\.inc")
                .publicIncDirsRE(publicDir, "llvm/" + directory + "/", ".*\\.def");
        }

        if (const string cmakePublicDir = "llvm/my-fork/include/llvm/" + directory;
            exists(path(srcNode->filePath) / cmakePublicDir))
        {
            llvmCpp.publicHUDirsRE(cmakePublicDir, "llvm/" + directory + "/", ".*\\.h")
                .publicIncDirsRE(cmakePublicDir, "llvm/" + directory + "/", ".*\\.inc")
                .publicIncDirsRE(cmakePublicDir, "llvm/" + directory + "/", ".*\\.def");
        }
    }
}

void addClangDirectory(DSC<CppTarget> &target, const string &directory, const string &privatePrefix = "",
                       const bool addInReq = true, const bool addInUseReq = true)
{
    CppTarget &clangCpp = target.getSourceTarget();

    if (addInReq)
    {
        if (const string privateDir = "clang/lib/" + directory; exists(path(srcNode->filePath) / privateDir))
        {
            clangCpp.moduleDirsRE(privateDir, ".*cpp")
                .privateHUDirsRE(privateDir, privatePrefix, ".*\\.h")
                .privateIncDirsRE(privateDir, privatePrefix, ".*\\.inc")
                .privateIncDirsRE(privateDir, privatePrefix, ".*\\.def");
        }

        if (const string cmakePrivateDir = "llvm/my-fork/tools/clang/lib/" + directory;
            exists(path(srcNode->filePath) / cmakePrivateDir))
        {
            clangCpp.publicHUDirsRE(cmakePrivateDir, "", ".*\\.h")
                .publicIncDirsRE(cmakePrivateDir, "", ".*\\.inc")
                .publicIncDirsRE(cmakePrivateDir, "", ".*\\.def");
        }
    }

    if (addInUseReq)
    {
        if (const string publicDir = "clang/include/clang/" + directory; exists(path(srcNode->filePath) / publicDir))
        {
            clangCpp.publicHUDirsRE(publicDir, "clang/" + directory + "/", ".*\\.h")
                .publicIncDirsRE(publicDir, "clang/" + directory + "/", ".*\\.inc")
                .publicIncDirsRE(publicDir, "clang/" + directory + "/", ".*\\.def");
        }

        if (const string cmakePublicDir = "llvm/my-fork/tools/clang/include/clang/" + directory;
            exists(path(srcNode->filePath) / cmakePublicDir))
        {
            clangCpp.publicHUDirsRE(cmakePublicDir, "clang/" + directory + "/", ".*\\.h")
                .publicIncDirsRE(cmakePublicDir, "clang/" + directory + "/", ".*\\.inc")
                .publicIncDirsRE(cmakePublicDir, "clang/" + directory + "/", ".*\\.def");
        }
    }
}

void addDirectory(CppTarget &target, const string &directory, const string &prefix)
{
    target.publicHUDirsRE(directory, prefix, ".*\\.h")
        .publicIncDirsRE(directory, prefix, ".*\\.inc")
        .publicIncDirsRE(directory, prefix, ".*\\.def");
}

void configurationSpecification(Configuration &config)
{
    config.cppCompileCommand =
        '\"' + config.compilerFeatures.compiler.bTPath +
        "\" -DGTEST_HAS_RTTI=0 -DNDEBUG -D_GLIBCXX_USE_CXX11_ABI=1 -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS "
        "-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -O3 -Wall -Wc++98-compat-extra-semi -Wcast-qual "
        "-Wcovered-switch-default -Wctad-maybe-unsupported -Wdelete-non-virtual-dtor -Werror=date-time "
        "-Werror=unguarded-availability-new -Wextra -Wimplicit-fallthrough -Wmisleading-indentation "
        "-Wmissing-field-initializers -Wno-long-long -Wno-noexcept-type -Wno-pass-failed -Wno-unused-parameter "
        "-Wnon-virtual-dtor -Wstring-conversion -Wsuggest-override -Wwrite-strings  -fPIC -fdata-sections "
        "-fdiagnostics-color -ffunction-sections -fno-exceptions -fno-rtti -fno-semantic-interposition -funwind-tables "
        "-fvisibility-inlines-hidden  -pedantic -std=c++20 ";

    config.cCompileCommand =
        '\"' + config.compilerFeatures.compiler.bTPath +
        "\" -Wall -Wc++98-compat-extra-semi -Wcast-qual -Wcovered-switch-default -Wctad-maybe-unsupported "
        "-Werror=date-time -Werror=unguarded-availability-new -Wextra -Wimplicit-fallthrough -Wmisleading-indentation "
        "-Wmissing-field-initializers -Wno-long-long -Wno-unused-parameter -Wstring-conversion -Wwrite-strings "
        "-fdata-sections -fdiagnostics-color -ffunction-sections -fno-semantic-interposition -pedantic "
        "-DGTEST_HAS_RTTI=0 -DNDEBUG -D_GLIBCXX_USE_CXX11_ABI=1 -D_GNU_SOURCE "
        "-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -O3 -fPIC ";

    config.assemblyCompileCommand = '\"' + config.compilerFeatures.compiler.bTPath +
                                    "\" -DGTEST_HAS_RTTI=0 -DNDEBUG -D_GLIBCXX_USE_CXX11_ABI=1 -D_GNU_SOURCE "
                                    "-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -O3 -fPIC ";

    config.linkCommand =
        '\"' + config.linkerFeatures.linker.bTPath +
        "\" -DNDEBUG -O3 -Wl,--gc-sections -Wall -Wc++98-compat-extra-semi -Wcast-qual -Wcovered-switch-default "
        "-Wctad-maybe-unsupported "
        "-Wdelete-non-virtual-dtor -Werror=date-time -Werror=unguarded-availability-new -Wextra -Wimplicit-fallthrough "
        " -Wmisleading-indentation "
        "-Wmissing-field-initializers -Wno-long-long -Wno-noexcept-type -Wno-pass-failed -Wno-unused-parameter "
        "-Wnon-virtual-dtor -Wstring-conversion -Wsuggest-override -Wwrite-strings -fPIC -fdata-sections "
        "-fdiagnostics-color -ffunction-sections -fno-semantic-interposition -fvisibility-inlines-hidden -ldl -lm -lrt "
        "-pedantic /usr/lib/x86_64-linux-gnu/libz.so -o \"";

    DSC<CppTarget> llvmConfig = config.getCppStaticDSC("LLVMConfig");
    addLlvmDirectory(llvmConfig, "Config");

    DSC<CppTarget> &llvmDemangle = config.getCppStaticDSC("LLVMDemangle");
    addLlvmDirectory(llvmDemangle, "Demangle");

    // LLVMSupport C and Assembly object files (totaling 13). These are only compiled in standard configuration and are
    // skipped in hu configuration. In hu configuration, these are instead loaded as a static library of the standard
    // configuration. These are skipped because clang currently fails with IPC compilation of c and assembly files with
    // header-units.

    DSC<CppTarget> *llvmSupportCDscPointer = nullptr;
    PLOAT *llvmSupportCPloatPointer = nullptr;

    if (config.name == "hu")
    {
        Node *supportCBuildDir = Node::getNodeNonNormalized(
            configureNode->filePath + string{slashc} + "standard" + string{slashc} + "LLVMSupportC", false);
        PLOAT &llvmSupportCPloat = config.getStaticPLOAT("LLVMSupportC", supportCBuildDir);
        llvmSupportCPloatPointer = &llvmSupportCPloat;
    }
    else
    {
        DSC<CppTarget> &llvmSupportCDsc = config.getCppStaticDSC("LLVMSupportC");
        if constexpr (bsMode == BSMode::CONFIGURE)
        {

            set<string> blakeCNoInclude;
            blakeCNoInclude.emplace("rpmalloc/rpmalloc.c");
            blakeCNoInclude.emplace("rpmalloc/malloc.c");
            blakeCNoInclude.emplace("BLAKE3/blake3_sse41.c");
            blakeCNoInclude.emplace("BLAKE3/blake3_avx512.c");
            blakeCNoInclude.emplace("BLAKE3/blake3_sse2.c");
            blakeCNoInclude.emplace("BLAKE3/blake3_avx2.c");

            editOutFilesRecursive(llvmSupportCDsc.getSourceTargetPointer(), "llvm/lib/Support", ".c", blakeCNoInclude);

            set<string> blakeAssemblyNoInclude;
            blakeAssemblyNoInclude.emplace("BLAKE3/blake3_sse2_x86-64_windows_gnu.S");
            blakeAssemblyNoInclude.emplace("BLAKE3/blake3_avx512_x86-64_windows_gnu.S");
            blakeAssemblyNoInclude.emplace("BLAKE3/blake3_avx2_x86-64_windows_gnu.S");
            blakeAssemblyNoInclude.emplace("BLAKE3/blake3_sse41_x86-64_windows_gnu.S");
            editOutFilesRecursive(llvmSupportCDsc.getSourceTargetPointer(), "llvm/lib/Support", ".S",
                                  blakeAssemblyNoInclude);
        }
        llvmSupportCDscPointer = &llvmSupportCDsc;
    }

    // llvm/lib/Support/LSP files not included
    DSC<CppTarget> &llvmSupport = config.getCppStaticDSC("LLVMSupport").publicDeps(llvmDemangle, llvmConfig);
    addLlvmDirectory(llvmSupport, "Support");
    addLlvmDirectory(llvmSupport, "ADT");
    addLlvmDirectory(llvmSupport, "Support/Unix", "Unix/");
    addLlvmDirectory(llvmSupport, "Support/HTTP", "", false, true);
    llvmSupport.getSourceTarget().privateIncludesSource("llvm/lib/Support");
    llvmSupport.getSourceTarget().makeHeaderUnitHeaderFile("llvm/Support/UniqueBBID.h", true, true);

    llvmSupport.getSourceTarget()
        .privateIncDirsRE("llvm/lib/Support/BLAKE3", "", ".*\\.h")
        .privateIncDirsRE("llvm/lib/Support/rpmalloc", "", ".*\\.h")
        .privateHeaderUnits("valgrind/valgrind.h", "/usr/include/valgrind/valgrind.h", "siphash/SipHash.h",
                            "third-party/siphash/include/siphash/SipHash.h")
        .publicHeaderUnits("llvm/Support/FileSystem/UniqueID.h", "llvm/include/llvm/Support/FileSystem/UniqueID.h")
        .publicHUDirsRE("llvm/include/llvm-c", "llvm-c/", ".*\\.h")
        .publicHUDirsRE("llvm/include/llvm-c/Transforms", "llvm-c/Transforms/", ".*\\.h")
        .privateHUIncludes("third-party/siphash/include");

    DSC<CppTarget> &llvmPlugins = config.getCppStaticDSC("LLVMPlugins").publicDeps(llvmSupport);
    addLlvmDirectory(llvmPlugins, "Plugins");

    void *ptr = config.name == "hu" ? (void *)llvmSupportCPloatPointer : (void *)llvmSupportCDscPointer;
    if (!ptr)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    if (config.name == "hu")
    {
        llvmSupport.getLOAT().publicDeps(*llvmSupportCPloatPointer);
    }
    else
    {
        llvmSupport.publicDeps(*llvmSupportCDscPointer);
    }

    DSC<CppTarget> &llvmTargetParser = config.getCppStaticDSC("LLVMTargetParser").publicDeps(llvmSupport);
    addLlvmDirectory(llvmTargetParser, "TargetParser");
    addLlvmDirectory(llvmTargetParser, "TargetParser/Unix", "Unix/");

    DSC<CppTarget> &llvmFrontendDirective = config.getCppStaticDSC("LLVMFrontendDirective").publicDeps(llvmSupport);
    addLlvmDirectory(llvmFrontendDirective, "Frontend/Directive");

    DSC<CppTarget> &llvmBinaryFormat =
        config.getCppStaticDSC("LLVMBinaryFormat").publicDeps(llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmBinaryFormat, "BinaryFormat");
    addLlvmDirectory(llvmBinaryFormat, "BinaryFormat/ELFRelocs");

    DSC<CppTarget> &llvmOption = config.getCppStaticDSC("LLVMOption").publicDeps(llvmSupport);
    addLlvmDirectory(llvmOption, "Option");

    DSC<CppTarget> &llvmDebugInfoMSF = config.getCppStaticDSC("LLVMDebugInfoMSF").publicDeps(llvmSupport);
    addLlvmDirectory(llvmDebugInfoMSF, "DebugInfo/MSF");

    DSC<CppTarget> &llvmDebugInfoCodeView = config.getCppStaticDSC("LLVMDebugInfoCodeView").publicDeps(llvmSupport);
    addLlvmDirectory(llvmDebugInfoCodeView, "DebugInfo/CodeView");

    DSC<CppTarget> &llvmDebugInfoDWARFLowLevel = config.getCppStaticDSC("LLVMDebugInfoDWARFLowLevel")
                                                     .publicDeps(llvmBinaryFormat, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmDebugInfoDWARFLowLevel, "DebugInfo/DWARF/LowLevel");

    DSC<CppTarget> &llvmTextAPI =
        config.getCppStaticDSC("LLVMTextAPI").publicDeps(llvmSupport, llvmBinaryFormat, llvmTargetParser);
    addLlvmDirectory(llvmTextAPI, "TextAPI");

    DSC<CppTarget> &llvmBitstreamReader = config.getCppStaticDSC("LLVMBitstreamReader").publicDeps(llvmSupport);
    addLlvmDirectory(llvmBitstreamReader, "Bitstream/Reader");
    addLlvmDirectory(llvmBitstreamReader, "Bitstream");

    DSC<CppTarget> &llvmRemarks = config.getCppStaticDSC("LLVMRemarks").publicDeps(llvmBitstreamReader, llvmSupport);
    addLlvmDirectory(llvmRemarks, "Remarks");

    DSC<CppTarget> &dlDebugInfoDIContext = config.getCppObjectDSC("DLDebugInfoDIContext");
    dlDebugInfoDIContext.getSourceTarget().publicHeaderUnits("llvm/DebugInfo/DIContext.h",
                                                             "llvm/include/llvm/DebugInfo/DIContext.h");

    DSC<CppTarget> &llvmDebugInfoBTF = config.getCppStaticDSC("LLVMDebugInfoBTF").publicDeps(llvmSupport);
    addLlvmDirectory(llvmDebugInfoBTF, "DebugInfo/BTF");

    DSC<CppTarget> &llvmTableGen = config.getCppStaticDSC("LLVMTableGen").publicDeps(llvmSupport);
    addLlvmDirectory(llvmTableGen, "TableGen");

    DSC<CppTarget> &llvmCore =
        config.getCppStaticDSC("LLVMCore")
            .publicDeps(llvmBinaryFormat, llvmDemangle, llvmRemarks, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmCore, "IR");
    llvmCore.getSourceTarget()
        .publicIncDirsRE("llvm/include/llvm", "llvm/", ".*\\.h")
        .makeHeaderUnitHeaderFile("llvm/IR/DroppedVariableStatsIR.h", true, true);

    DSC<CppTarget> &llvmTableGenBasic =
        config.getCppStaticDSC("LLVMTableGenBasic").publicDeps(llvmSupport, llvmTableGen);
    llvmTableGenBasic.getSourceTarget()
        .publicIncludesSource("llvm/utils/TableGen")
        .moduleDirsRE("llvm/utils/TableGen/Basic", ".*cpp");

    // addLLVMDirectory is for targets in llvm/lib directory but not in llvm/utils directory
    addDirectory(llvmTableGenBasic.getSourceTarget(), "llvm/utils/TableGen/Basic", "Basic/");
    addDirectory(llvmTableGenBasic.getSourceTarget(), "llvm/utils/TableGen", "");
    addDirectory(llvmTableGenBasic.getSourceTarget(), "llvm/utils/TableGen/Common", "Common/");
    addDirectory(llvmTableGenBasic.getSourceTarget(), "llvm/utils/TableGen/Common/GlobalISel", "Common/GlobalISel/");

    DSC<CppTarget> &llvmMinTableGen =
        config.getCppExeDSC("LLVMMinTableGen")
            .privateDeps(llvmDemangle, llvmSupport, llvmTableGen, llvmTableGenBasic, llvmTargetParser);
    llvmMinTableGen.getLOAT().setOutputName("llvm-min-tblgen");
    llvmMinTableGen.getSourceTarget().moduleFiles("llvm/utils/TableGen/llvm-min-tblgen.cpp");

    DSC<CppTarget> &dlBitCode = config.getCppObjectDSC("DLBitCode");
    addLlvmDirectory(dlBitCode, "Bitcode", "", false, true);

    DSC<CppTarget> &llvmBitReader =
        config.getCppStaticDSC("LLVMBitReader")
            .publicDeps(llvmBitstreamReader, llvmCore, llvmSupport, llvmTargetParser, dlBitCode);
    addLlvmDirectory(llvmBitReader, "Bitcode/Reader");

    DSC<CppTarget> &llvmMC = config.getCppStaticDSC("LLVMMC").publicDeps(llvmBinaryFormat, llvmDebugInfoDWARFLowLevel,
                                                                         llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmMC, "MC");

    DSC<CppTarget> &llvmFrontendHLSL =
        config.getCppStaticDSC("LLVMFrontendHLSL").publicDeps(llvmBinaryFormat, llvmCore, llvmSupport);
    addLlvmDirectory(llvmFrontendHLSL, "Frontend/HLSL");

    DSC<CppTarget> &dlTransforms = config.getCppObjectDSC("DLTransforms");
    addLlvmDirectory(dlTransforms, "Transforms", "", false, true);

    DSC<CppTarget> &llvmCFGuard =
        config.getCppStaticDSC("LLVMCFGuard").publicDeps(llvmCore, llvmSupport, llvmTargetParser, dlTransforms);
    addLlvmDirectory(llvmCFGuard, "Transforms/CFGuard");

    DSC<CppTarget> &llvmAsmParser =
        config.getCppStaticDSC("LLVMAsmParser").publicDeps(llvmBinaryFormat, llvmCore, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmAsmParser, "AsmParser");

    DSC<CppTarget> &llvmIRReader =
        config.getCppStaticDSC("LLVMIRReader").publicDeps(llvmAsmParser, llvmBitReader, llvmCore, llvmSupport);
    addLlvmDirectory(llvmIRReader, "IRReader");

    DSC<CppTarget> &llvmMCParser =
        config.getCppStaticDSC("LLVMMCParser").publicDeps(llvmMC, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmMCParser, "MC/MCParser");

    DSC<CppTarget> &llvmObject = config.getCppStaticDSC("LLVMObject")
                                     .publicDeps(llvmBitReader, llvmCore, llvmMC, llvmIRReader, llvmBinaryFormat,
                                                 llvmMCParser, llvmSupport, llvmTargetParser, llvmTextAPI);
    addLlvmDirectory(llvmObject, "Object", "");

    DSC<CppTarget> &llvmMCDisassembler =
        config.getCppStaticDSC("LLVMMCDisassembler").publicDeps(llvmMC, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmMCDisassembler, "MC/MCDisassembler");

    DSC<CppTarget> &llvmObjectYAML =
        config.getCppStaticDSC("LLVMObjectYAML")
            .publicDeps(llvmBinaryFormat, llvmObject, llvmSupport, llvmTargetParser, llvmDebugInfoCodeView, llvmMC);
    addLlvmDirectory(llvmObjectYAML, "ObjectYAML");

    DSC<CppTarget> &llvmDebugInfoDWARF =
        config.getCppStaticDSC("LLVMDebugInfoDWARF")
            .publicDeps(llvmBinaryFormat, llvmDebugInfoDWARFLowLevel, llvmObject, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmDebugInfoDWARF, "DebugInfo/DWARF");

    DSC<CppTarget> &llvmDebugInfoPDB =
        config.getCppStaticDSC("LLVMDebugInfoPDB")
            .publicDeps(llvmBinaryFormat, llvmObject, llvmSupport, llvmDebugInfoCodeView, llvmDebugInfoMSF);
    addLlvmDirectory(llvmDebugInfoPDB, "DebugInfo/PDB");
    addLlvmDirectory(llvmDebugInfoPDB, "DebugInfo/PDB/Native");

    DSC<CppTarget> &llvmDebugInfoGSYM =
        config.getCppStaticDSC("LLVMDebugInfoGSYM")
            .publicDeps(llvmMC, llvmObject, llvmSupport, llvmTargetParser, llvmDebugInfoDWARF);
    addLlvmDirectory(llvmDebugInfoGSYM, "DebugInfo/GSYM");

    DSC<CppTarget> &llvmTextAPIBinaryReader =
        config.getCppStaticDSC("LLVMTextAPIBinaryReader")
            .publicDeps(llvmDebugInfoDWARF, llvmSupport, llvmObject, llvmTextAPI, llvmTargetParser);
    addLlvmDirectory(llvmTextAPIBinaryReader, "TextAPI/BinaryReader");

    DSC<CppTarget> &llvmSymbolize =
        config.getCppStaticDSC("LLVMSymbolize")
            .publicDeps(llvmDebugInfoDWARF, llvmDebugInfoGSYM, llvmDebugInfoPDB, llvmDebugInfoBTF, llvmObject,
                        llvmSupport, llvmDemangle, llvmTargetParser);
    addLlvmDirectory(llvmSymbolize, "DebugInfo/Symbolize");

    // An incomplete target. We are not compiling its source as it is not being used in Clang compilation.
    DSC<CppTarget> &llvmDebuginfod = config.getCppObjectDSC("LLVMDebuginfod");
    addLlvmDirectory(llvmDebuginfod, "Debuginfod", "");

    DSC<CppTarget> &llvmProfileData =
        config.getCppStaticDSC("LLVMProfileData")
            .publicDeps(llvmBitstreamReader, llvmCore, llvmObject, llvmSupport, llvmDemangle, llvmSymbolize,
                        llvmDebugInfoDWARF, llvmDebugInfoDWARFLowLevel, llvmTargetParser);
    addLlvmDirectory(llvmProfileData, "ProfileData", "");

    DSC<CppTarget> &llvmCoverage =
        config.getCppStaticDSC("LLVMCoverage")
            .publicDeps(llvmCore, llvmObject, llvmProfileData, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmCoverage, "ProfileData/Coverage");

    DSC<CppTarget> &llvmAnalysis = config.getCppStaticDSC("LLVMAnalysis")
                                       .publicDeps(llvmBinaryFormat, llvmCore, llvmFrontendHLSL, llvmObject,
                                                   llvmProfileData, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmAnalysis, "Analysis");
    addLlvmDirectory(llvmAnalysis, "Analysis/Utils");
    llvmAnalysis.getSourceTarget().makeHeaderUnitHeaderFile("llvm/Analysis/MustExecute.h", true, true);

    DSC<CppTarget> &llvmIRPrinter =
        config.getCppStaticDSC("LLVMIRPrinter").publicDeps(llvmAnalysis, llvmCore, llvmSupport);
    addLlvmDirectory(llvmIRPrinter, "IRPrinter");

    DSC<CppTarget> &llvmBitWriter = config.getCppStaticDSC("LLVMBitWriter")
                                        .publicDeps(llvmAnalysis, llvmCore, llvmMC, llvmObject, llvmProfileData,
                                                    llvmSupport, llvmTargetParser, dlBitCode);
    addLlvmDirectory(llvmBitWriter, "Bitcode/Writer");

    DSC<CppTarget> &llvmFrontendAtomic =
        config.getCppStaticDSC("LLVMFrontendAtomic").publicDeps(llvmCore, llvmSupport, llvmAnalysis);
    addLlvmDirectory(llvmFrontendAtomic, "Frontend/Atomic");

    DSC<CppTarget> &llvmTransformUtils =
        config.getCppStaticDSC("LLVMTransformUtils")
            .publicDeps(llvmAnalysis, llvmCore, llvmProfileData, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmTransformUtils, "Transforms/Utils");

    DSC<CppTarget> &llvmTarget =
        config.getCppStaticDSC("LLVMTarget").publicDeps(llvmAnalysis, llvmCore, llvmMC, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmTarget, "Target");
    DSC<CppTarget> &llvmSandboxIR =
        config.getCppStaticDSC("LLVMSandboxIR").publicDeps(llvmCore, llvmSupport, llvmAnalysis);
    addLlvmDirectory(llvmSandboxIR, "SandboxIR");

    DSC<CppTarget> &llvmCGData = config.getCppStaticDSC("LLVMCGData")
                                     .publicDeps(llvmBitReader, llvmBitWriter, llvmCore, llvmSupport, llvmObject);
    addLlvmDirectory(llvmCGData, "CGData");

    DSC<CppTarget> &llvmFrontendOffloading = config.getCppStaticDSC("LLVMFrontendOffloading")
                                                 .publicDeps(llvmCore, llvmBinaryFormat, llvmObject, llvmObjectYAML,
                                                             llvmSupport, llvmTransformUtils, llvmTargetParser);
    addLlvmDirectory(llvmFrontendOffloading, "Frontend/Offloading");

    DSC<CppTarget> &llvmAggressiveInstCombine =
        config.getCppStaticDSC("LLVMAggressiveInstCombine")
            .publicDeps(llvmAnalysis, llvmCore, llvmSupport, llvmTransformUtils);
    addLlvmDirectory(llvmAggressiveInstCombine, "Transforms/AggressiveInstCombine");

    DSC<CppTarget> &llvmInstCombine =
        config.getCppStaticDSC("LLVMInstCombine").publicDeps(llvmAnalysis, llvmCore, llvmSupport, llvmTransformUtils);
    addLlvmDirectory(llvmInstCombine, "Transforms/InstCombine");

    DSC<CppTarget> &llvmScalarOpts = config.getCppStaticDSC("LLVMScalarOpts")
                                         .publicDeps(llvmAggressiveInstCombine, llvmAnalysis, llvmCore, llvmInstCombine,
                                                     llvmProfileData, llvmSupport, llvmTransformUtils);
    addLlvmDirectory(llvmScalarOpts, "Transforms/Scalar");

    DSC<CppTarget> &llvmVectorize =
        config.getCppStaticDSC("LLVMVectorize")
            .publicDeps(llvmAnalysis, llvmCore, llvmSupport, llvmTransformUtils, llvmSandboxIR);
    addLlvmDirectory(llvmVectorize, "Transforms/Vectorize");
    addLlvmDirectory(llvmVectorize, "Transforms/Vectorize/SandboxVectorizer");
    addLlvmDirectory(llvmVectorize, "Transforms/Vectorize/SandboxVectorizer/Passes", "Passes/");
    llvmVectorize.getSourceTarget().makeHeaderUnitHeaderFile("LoopVectorizationPlanner.h", true, false);
    llvmVectorize.getSourceTarget().makeHeaderUnitHeaderFile("VPRecipeBuilder.h", true, false);

    DSC<CppTarget> &llvmInstrumentation = config.getCppStaticDSC("LLVMInstrumentation")
                                              .publicDeps(llvmAnalysis, llvmCore, llvmDemangle, llvmMC, llvmSupport,
                                                          llvmTargetParser, llvmTransformUtils, llvmProfileData);
    addLlvmDirectory(llvmInstrumentation, "Transforms/Instrumentation");

    DSC<CppTarget> &llvmObjCARCOpts =
        config.getCppStaticDSC("LLVMObjCARCOpts")
            .publicDeps(llvmAnalysis, llvmCore, llvmSupport, llvmTargetParser, llvmTransformUtils);
    addLlvmDirectory(llvmObjCARCOpts, "Transforms/ObjCARC");

    DSC<CppTarget> &llvmHipStdPar =
        config.getCppStaticDSC("LLVMHipStdPar").publicDeps(llvmAnalysis, llvmCore, llvmSupport, llvmTransformUtils);
    addLlvmDirectory(llvmHipStdPar, "Transforms/HipStdPar");

    DSC<CppTarget> &llvmLinker =
        config.getCppStaticDSC("LLVMLinker")
            .publicDeps(llvmCore, llvmObject, llvmSupport, llvmTransformUtils, llvmTargetParser);
    addLlvmDirectory(llvmLinker, "Linker");

    DSC<CppTarget> &llvmFrontendDriver = config.getCppStaticDSC("LLVMFrontendDriver")
                                             .publicDeps(llvmCore, llvmSupport, llvmAnalysis, llvmInstrumentation);
    addLlvmDirectory(llvmFrontendDriver, "Frontend/Driver");

    DSC<CppTarget> &llvmCodeGenTypes = config.getCppStaticDSC("LLVMCodeGenTypes").publicDeps(llvmSupport);
    addLlvmDirectory(llvmCodeGenTypes, "CodeGenTypes", "");

    DSC<CppTarget> &llvmCodeGen =
        config.getCppStaticDSC("LLVMCodeGen")
            .publicDeps(llvmAnalysis, llvmBitReader, llvmBitWriter, llvmCGData, llvmCodeGenTypes, llvmCore, llvmMC,
                        llvmObjCARCOpts, llvmProfileData, llvmScalarOpts, llvmSupport, llvmTarget, llvmTargetParser,
                        llvmTransformUtils);
    addLlvmDirectory(llvmCodeGen, "CodeGen", "");
    addLlvmDirectory(llvmCodeGen, "CodeGen/PBQP");
    addLlvmDirectory(llvmCodeGen, "CodeGen/LiveDebugValues", "LiveDebugValues/");
    llvmCodeGen.getSourceTarget().privateIncludesSource("llvm/lib/CodeGen");
    llvmCodeGen.getSourceTarget().makeHeaderUnitHeaderFile("llvm/CodeGen/WasmEHFuncInfo.h", true, true);
    llvmCodeGen.getSourceTarget().makeHeaderUnitHeaderFile("LiveDebugValues/InstrRefBasedImpl.h", true, false);

    DSC<CppTarget> &llvmSelectionDAG = config.getCppStaticDSC("LLVMSelectionDAG")
                                           .publicDeps(llvmAnalysis, llvmCodeGen, llvmCodeGenTypes, llvmCore, llvmMC,
                                                       llvmSupport, llvmTarget, llvmTargetParser, llvmTransformUtils);
    addLlvmDirectory(llvmSelectionDAG, "CodeGen/SelectionDAG");

    DSC<CppTarget> &llvmGlobalISel = config.getCppStaticDSC("LLVMGlobalISel")
                                         .publicDeps(llvmAnalysis, llvmCodeGen, llvmCodeGenTypes, llvmCore, llvmMC,
                                                     llvmSelectionDAG, llvmSupport, llvmTarget, llvmTransformUtils);
    addLlvmDirectory(llvmGlobalISel, "CodeGen/GlobalISel", "");

    DSC<CppTarget> &llvmABI = config.getCppObjectDSC("LLVMABI");
    addLlvmDirectory(llvmABI, "ABI", "", false, true);

    DSC<CppTarget> &llvmMirParser = config.getCppObjectDSC("LLVMIRParser");
    addLlvmDirectory(llvmMirParser, "CodeGen/MIRParser", "", true);

    DSC<CppTarget> &llvmFrontendOpenMP =
        config.getCppStaticDSC("LLVMFrontendOpenMP")
            .publicDeps(llvmCore, llvmSupport, llvmTargetParser, llvmTransformUtils, llvmAnalysis, llvmDemangle, llvmMC,
                        llvmScalarOpts, llvmBitReader, llvmFrontendOffloading, llvmFrontendAtomic,
                        llvmFrontendDirective);
    addLlvmDirectory(llvmFrontendOpenMP, "Frontend/OpenMP");
    addLlvmDirectory(llvmFrontendOpenMP, "Frontend/OpenACC", "", false, true);

    DSC<CppTarget> &llvmAsmPrinter =
        config.getCppStaticDSC("LLVMAsmPrinter")
            .publicDeps(llvmAnalysis, llvmBinaryFormat, llvmCodeGen, llvmCodeGenTypes, llvmCore, llvmDebugInfoCodeView,
                        llvmDebugInfoDWARF, llvmDebugInfoDWARFLowLevel, llvmMC, llvmMCParser, llvmProfileData,
                        llvmRemarks, llvmSupport, llvmTarget, llvmTargetParser);
    addLlvmDirectory(llvmAsmPrinter, "CodeGen/AsmPrinter");
    llvmAsmPrinter.getSourceTarget().makeHeaderUnitHeaderFile("PseudoProbePrinter.h", true, false);

    DSC<CppTarget> &llvmipo = config.getCppStaticDSC("LLVMipo").publicDeps(
        llvmAggressiveInstCombine, llvmAnalysis, llvmBitReader, llvmBitWriter, llvmCore, llvmFrontendOpenMP,
        llvmInstCombine, llvmIRReader, llvmDemangle, llvmLinker, llvmObject, llvmProfileData, llvmScalarOpts,
        llvmSupport, llvmTargetParser, llvmTransformUtils, llvmVectorize, llvmInstrumentation);
    addLlvmDirectory(llvmipo, "Transforms/IPO");

    DSC<CppTarget> &llvmCoroutines = config.getCppStaticDSC("LLVMCoroutines")
                                         .publicDeps(llvmAnalysis, llvmCore, llvmipo, llvmScalarOpts, llvmSupport,
                                                     llvmTransformUtils, llvmTargetParser);
    addLlvmDirectory(llvmCoroutines, "Transforms/Coroutines");

    DSC<CppTarget> &llvmPasses =
        config.getCppStaticDSC("LLVMPasses")
            .publicDeps(llvmAggressiveInstCombine, llvmAnalysis, llvmCFGuard, llvmCodeGen, llvmGlobalISel, llvmCore,
                        llvmCoroutines, llvmHipStdPar, llvmipo, llvmInstCombine, llvmIRPrinter, llvmObjCARCOpts,
                        llvmScalarOpts, llvmSupport, llvmTarget, llvmTransformUtils, llvmVectorize,
                        llvmInstrumentation);
    addLlvmDirectory(llvmPasses, "Passes");

    DSC<CppTarget> &llvmExtensions = config.getCppStaticDSC("LLVMExtensions").publicDeps(llvmSupport);
    addLlvmDirectory(llvmExtensions, "Extensions");

    config.assign(TreatHUAsHeaderFile::NO);

    // This target has one include directory of the llvmX86CodeGen target. As the headers from this same include-dir
    // are being used by other targets. We had to make dummy target for unique ownership. Other 6 lines are for
    // header-units specification.
    DSC<CppTarget> &dlX86CodeGen = config.getCppObjectDSC("DLX86CodeGen");
    dlX86CodeGen.getSourceTarget()
        .publicIncludesSource("llvm/my-fork/lib/Target/X86", "llvm/lib/Target/X86")
        .publicHUDirsRE("llvm/my-fork/lib/Target/X86", "", ".*\\.h")
        .publicIncDirsRE("llvm/my-fork/lib/Target/X86", "", ".*\\.inc")
        .publicHUDirsRE("llvm/lib/Target/X86", "", ".*\\.h")
        .publicIncDirsRE("llvm/lib/Target/X86", "", ".*\\.def")
        .publicHUDirsRE("llvm/lib/Target/X86/MCTargetDesc", "MCTargetDesc/", ".*\\.h")
        .publicIncDirsRE("llvm/lib/Target/X86/MCTargetDesc", "MCTargetDesc/", ".*\\.def")
        .publicHUDirsRE("llvm/lib/Target/X86/TargetInfo", "TargetInfo/", ".*\\.h")
        .publicIncDirsRE("llvm/lib/Target/X86/TargetInfo", "TargetInfo/", ".*\\.def")
        .publicHUDirsRE("llvm/lib/Target/X86/GISel", "GISel/", ".*\\.h")
        .publicIncDirsRE("llvm/lib/Target/X86/GISel", "GISel/", ".*\\.def");
    dlX86CodeGen.getSourceTarget().makeHeaderUnitHeaderFile("X86AsmPrinter.h", true, true);

    DSC<CppTarget> &llvmX86Info = config.getCppStaticDSC("LLVMX86Info").privateDeps(dlX86CodeGen);
    llvmX86Info.getSourceTarget().moduleDirsRE("llvm/lib/Target/X86/TargetInfo", ".*cpp");

    DSC<CppTarget> &llvmX86Desc = config.getCppStaticDSC("LLVMX86Desc").privateDeps(dlX86CodeGen);
    llvmX86Desc.getSourceTarget().moduleDirsRE("llvm/lib/Target/X86/MCTargetDesc", ".*cpp");

    DSC<CppTarget> &llvmX86CodeGen = config.getCppStaticDSC("LLVMX86CodeGen").privateDeps(dlX86CodeGen);
    llvmX86CodeGen.getSourceTarget().moduleDirsRE("llvm/lib/Target/X86/GISel", ".*cpp");
    llvmX86CodeGen.getSourceTarget().moduleDirsRE("llvm/lib/Target/X86", ".*cpp");

    DSC<CppTarget> &llvmX86Disassembler = config.getCppStaticDSC("LLVMX86Disassembler").privateDeps(dlX86CodeGen);
    addLlvmDirectory(llvmX86Disassembler, "Target/X86/Disassembler");

    DSC<CppTarget> &llvmX86AsmParser = config.getCppStaticDSC("LLVMX86AsmParser").privateDeps(dlX86CodeGen);
    addLlvmDirectory(llvmX86AsmParser, "Target/X86/AsmParser");
    llvmX86AsmParser.getSourceTarget().makeHeaderUnitHeaderFile("X86Operand.h", true, false);

    config.assign(TreatHUAsHeaderFile::NO);

    DSC<CppTarget> &llvmLTO = config.getCppStaticDSC("LLVMLTO").publicDeps(
        llvmAggressiveInstCombine, llvmAnalysis, llvmBinaryFormat, llvmBitReader, llvmBitWriter, llvmCGData,
        llvmCodeGen, llvmCodeGenTypes, llvmCore, llvmExtensions, llvmipo, llvmInstCombine, llvmInstrumentation,
        llvmLinker, llvmMC, llvmObjCARCOpts, llvmObject, llvmPasses, llvmRemarks, llvmScalarOpts, llvmSupport,
        llvmTarget, llvmTargetParser, llvmTransformUtils);
    addLlvmDirectory(llvmLTO, "LTO");
    addLlvmDirectory(llvmLTO, "LTO/legacy");

    DSC<CppTarget> &llvmFrontend = config.getCppObjectDSC("LLVMFrontend");
    addLlvmDirectory(llvmFrontend, "Frontend/Debug");

    DSC<CppTarget> &clangSupport = config.getCppStaticDSC("clangSupport").publicDeps(llvmSupport);
    addClangDirectory(clangSupport, "Support");

    DSC<CppTarget> &clangIPC2978 = config.getCppStaticDSC("clangIPC2978");
    clangIPC2978.getSourceTarget().publicHUDirsRE("clang/include/clang/IPC2978", "clang/IPC2978/", ".*\\.hpp");
    addClangDirectory(clangIPC2978, "IPC2978");

    DSC<CppTarget> clangConfig = config.getCppStaticDSC("clangConfig");
    addClangDirectory(clangConfig, "Config");

    DSC<CppTarget> &clangBasic =
        config.getCppStaticDSC("clangBasic").publicDeps(llvmSupport, llvmTargetParser, llvmFrontendOpenMP);
    addClangDirectory(clangBasic, "Basic");
    addClangDirectory(clangBasic, "Basic/Targets", "Targets/");
    clangBasic.getSourceTarget().privateIncludesSource("clang/lib/Basic",
                                                       "llvm/my-fork/tools/clang/lib/Basic");

    DSC<CppTarget> &clangAPINotes =
        config.getCppStaticDSC("clangAPINotes").publicDeps(llvmBitReader, llvmBitstreamReader, llvmSupport);
    addClangDirectory(clangAPINotes, "APINotes");

    DSC<CppTarget> &clangLex = config.getCppStaticDSC("clangLex").publicDeps(llvmSupport, llvmTargetParser);
    addClangDirectory(clangLex, "Lex");

    DSC<CppTarget> &clangOptions = config.getCppStaticDSC("clangOptions").publicDeps(llvmOption, llvmSupport);
    addClangDirectory(clangOptions, "Options");

    DSC<CppTarget> &clangAST = config.getCppStaticDSC("clangAST")
                                   .publicDeps(llvmBinaryFormat, llvmCore, llvmFrontendOpenMP, llvmFrontendHLSL,
                                               llvmSupport, llvmTargetParser);
    addClangDirectory(clangAST, "AST");
    addClangDirectory(clangAST, "AST/ByteCode", "ByteCode/");
    clangAST.getSourceTarget().privateIncludesSource("llvm/my-fork/tools/clang/lib/AST", "clang/lib/AST");

    DSC<CppTarget> &clangUnifiedSymbolResolution =
        config.getCppStaticDSC("clangUnifiedSymbolResolution").publicDeps(clangAST, clangBasic, clangLex, llvmSupport);
    addClangDirectory(clangUnifiedSymbolResolution, "UnifiedSymbolResolution", "");

    DSC<CppTarget> &clangScalableStaticAnalysisFrameworkCore =
        config.getCppStaticDSC("clangScalableStaticAnalysisFrameworkCore")
            .publicDeps(clangAST, clangUnifiedSymbolResolution, llvmSupport);
    clangScalableStaticAnalysisFrameworkCore.getSourceTarget().privateIncludesSource(
        "clang/lib/ScalableStaticAnalysisFramework/Core");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework/Core");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework/Core/EntityLinker");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework/Core/Model");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework/Core/Serialization");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore,
                      "ScalableStaticAnalysisFramework/Core/Serialization/JSONFormat");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework/Core/SummaryData");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework/Core/Support");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore, "ScalableStaticAnalysisFramework/Core/TUSummary");
    addClangDirectory(clangScalableStaticAnalysisFrameworkCore,
                      "ScalableStaticAnalysisFramework/Core/WholeProgramAnalysis");

    DSC<CppTarget> &clangRewrite = config.getCppStaticDSC("clangRewrite").publicDeps(llvmSupport);
    addClangDirectory(clangRewrite, "Rewrite");
    addClangDirectory(clangRewrite, "Rewrite/Frontend", "", false, true);
    addClangDirectory(clangRewrite, "Rewrite/Core");

    DSC<CppTarget> &clangASTMatchers =
        config.getCppStaticDSC("clangASTMatchers").publicDeps(llvmFrontendOpenMP, llvmSupport);
    addClangDirectory(clangASTMatchers, "ASTMatchers");

    DSC<CppTarget> &clangEdit = config.getCppStaticDSC("clangEdit").publicDeps(llvmSupport);
    addClangDirectory(clangEdit, "Edit");

    DSC<CppTarget> &clangAnalysis = config.getCppStaticDSC("clangAnalysis").publicDeps(llvmFrontendOpenMP, llvmSupport);
    addClangDirectory(clangAnalysis, "Analysis");
    addClangDirectory(clangAnalysis, "Analysis/Analyses");
    addClangDirectory(clangAnalysis, "Analysis/Analyses/LifetimeSafety");
    addClangDirectory(clangAnalysis, "Analysis/Support");
    addClangDirectory(clangAnalysis, "Analysis/DomainSpecific");
    addClangDirectory(clangAnalysis, "Analysis/FlowSensitive", "", false, true);

    DSC<CppTarget> &clangAnalysisLifetimeSafety =
        config.getCppStaticDSC("clangAnalysisLifetimeSafety").publicDeps(llvmFrontendOpenMP, llvmSupport);
    addClangDirectory(clangAnalysisLifetimeSafety, "Analysis/LifetimeSafety");

    DSC<CppTarget> &clangSema = config.getCppStaticDSC("clangSema")
                                    .publicDeps(llvmCore, llvmDemangle, llvmFrontendHLSL, llvmFrontendOpenMP, llvmMC,
                                                llvmSupport, llvmTargetParser);
    addClangDirectory(clangSema, "Sema", "");
    clangSema.getSourceTarget().publicHUDirsRE("clang/include/clang-c", "clang-c/", ".*\\.h");
    clangSema.getSourceTarget().privateIncludesSource("llvm/my-fork/tools/clang/lib/Sema");

    DSC<CppTarget> &clangSerialization =
        config.getCppStaticDSC("clangSerialization")
            .publicDeps(llvmBitReader, llvmBitstreamReader, llvmObject, llvmSupport, llvmTargetParser);
    addClangDirectory(clangSerialization, "Serialization");

    DSC<CppTarget> &clangFrontend = config.getCppStaticDSC("clangFrontend")
                                        .publicDeps(llvmBitReader, llvmBitstreamReader, llvmOption, llvmPlugins,
                                                    llvmProfileData, llvmSupport, llvmTargetParser);
    addClangDirectory(clangFrontend, "Frontend", "");

    DSC<CppTarget> &clangScalableStaticAnalysisFrameworkAnalyses =
        config.getCppStaticDSC("clangScalableStaticAnalysisFrameworkAnalyses")
            .publicDeps(clangAST, clangAnalysis, clangBasic, clangScalableStaticAnalysisFrameworkCore);
    addClangDirectory(clangScalableStaticAnalysisFrameworkAnalyses,
                      "ScalableStaticAnalysisFramework/Analyses/CallGraph");
    addClangDirectory(clangScalableStaticAnalysisFrameworkAnalyses,
                      "ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel");
    addClangDirectory(clangScalableStaticAnalysisFrameworkAnalyses,
                      "ScalableStaticAnalysisFramework/Analyses/UnsafeBufferUsage");
    DSC<CppTarget> &clangScalableStaticAnalysisFrameworkFrontend =
        config.getCppStaticDSC("clangScalableStaticAnalysisFrameworkFrontend")
            .publicDeps(clangAST, clangBasic, clangFrontend, clangScalableStaticAnalysisFrameworkCore,
                        clangScalableStaticAnalysisFrameworkAnalyses, clangSema, llvmSupport);
    addClangDirectory(clangScalableStaticAnalysisFrameworkFrontend, "ScalableStaticAnalysisFramework/Frontend");

    DSC<CppTarget> &clangDependencyScanning =
        config.getCppStaticDSC("clangDependencyScanning")
            .publicDeps(llvmCore, llvmOption, llvmSupport, llvmTargetParser, clangAST, clangBasic, clangFrontend,
                        clangLex, clangSerialization);
    addClangDirectory(clangDependencyScanning, "DependencyScanning");

    DSC<CppTarget> &clangCodeGen =
        config.getCppStaticDSC("clangCodeGen")
            .publicDeps(llvmAggressiveInstCombine, llvmAnalysis, llvmBitReader, llvmBitWriter, llvmCodeGenTypes,
                        llvmCore, llvmCoroutines, llvmCoverage, llvmDemangle, llvmExtensions, llvmFrontendDriver,
                        llvmFrontendHLSL, llvmFrontendOpenMP, llvmFrontendOffloading, llvmHipStdPar, llvmipo,
                        llvmIRPrinter, llvmIRReader, llvmInstCombine, llvmInstrumentation, llvmLTO, llvmLinker, llvmMC,
                        llvmObjCARCOpts, llvmObject, llvmPasses, llvmPlugins, llvmProfileData, llvmScalarOpts,
                        llvmSupport, llvmTarget, llvmTargetParser, llvmTransformUtils);
    addClangDirectory(clangCodeGen, "CodeGen");
    addClangDirectory(clangCodeGen, "CodeGen/TargetBuiltins");
    addClangDirectory(clangCodeGen, "CodeGen/Targets");
    clangCodeGen.getSourceTarget().publicIncludesSource("clang/lib/CodeGen");

    config.assign(TreatHUAsHeaderFile::YES);
    DSC<CppTarget> &llvmWindowsDriver =
        config.getCppStaticDSC("LLVMWindowsDriver").publicDeps(llvmOption, llvmSupport, llvmTargetParser);
    addLlvmDirectory(llvmWindowsDriver, "WindowsDriver");
    config.assign(TreatHUAsHeaderFile::NO);

    DSC<CppTarget> &clangDriver =
        config.getCppStaticDSC("clangDriver")
            .publicDeps(llvmBinaryFormat, llvmMC, llvmObject, llvmOption, llvmProfileData, llvmSupport,
                        llvmTargetParser, llvmWindowsDriver, clangScalableStaticAnalysisFrameworkCore,
                        clangScalableStaticAnalysisFrameworkFrontend, clangScalableStaticAnalysisFrameworkAnalyses,
                        clangDependencyScanning);
    addClangDirectory(clangDriver, "Driver", "");
    addClangDirectory(clangDriver, "Driver/ToolChains", "ToolChains/");
    addClangDirectory(clangDriver, "Driver/ToolChains/Arch", "ToolChains/Arch/");
    clangDriver.getSourceTarget().privateIncludesSource("clang/lib/Driver");
    clangDriver.getSourceTarget().makeHeaderUnitHeaderFile("ToolChains/AMDGPUOpenMP.h", true, false);
    clangDriver.getSourceTarget().makeHeaderUnitHeaderFile("ToolChains/HIPAMD.h", true, false);
    clangDriver.getSourceTarget().makeHeaderUnitHeaderFile("ToolChains/AMDGPU.h", true, false);

    DSC<CppTarget> &clangCrossTU =
        config.getCppStaticDSC("clangCrossTU").publicDeps(llvmSupport, llvmTargetParser, clangUnifiedSymbolResolution);
    addClangDirectory(clangCrossTU, "CrossTU");

    DSC<CppTarget> &clangExtractAPI = config.getCppStaticDSC("clangExtractAPI")
                                          .publicDeps(llvmSupport, llvmTargetParser, clangUnifiedSymbolResolution);
    addClangDirectory(clangExtractAPI, "ExtractAPI");
    addClangDirectory(clangExtractAPI, "ExtractAPI/Serialization");

    DSC<CppTarget> &clangToolingCore =
        config.getCppStaticDSC("clangToolingCore").publicDeps(clangSupport, clangDependencyScanning);
    addClangDirectory(clangToolingCore, "Tooling/Core");
    addClangDirectory(clangToolingCore, "Tooling", "", false, true);

    DSC<CppTarget> &clangStaticAnalyzerCore =
        config.getCppStaticDSC("clangStaticAnalyzerCore")
            .publicDeps(llvmFrontendOpenMP, llvmSupport, clangUnifiedSymbolResolution);
    addClangDirectory(clangStaticAnalyzerCore, "StaticAnalyzer/Core", "");
    addClangDirectory(clangStaticAnalyzerCore, "StaticAnalyzer/Core/PathSensitive");
    addClangDirectory(clangStaticAnalyzerCore, "StaticAnalyzer/Core/BugReporter");

    DSC<CppTarget> &clangParse =
        config.getCppStaticDSC("clangParse")
            .publicDeps(llvmFrontendHLSL, llvmFrontendOpenMP, llvmMC, llvmMCParser, llvmSupport, llvmTargetParser);
    addClangDirectory(clangParse, "Parse", "");

    DSC<CppTarget> &clangStaticAnalyzerCheckers = config.getCppStaticDSC("clangStaticAnalyzerCheckers")
                                                      .publicDeps(llvmFrontendOpenMP, llvmSupport, llvmTargetParser);
    addClangDirectory(clangStaticAnalyzerCheckers, "StaticAnalyzer/Checkers", "");
    addClangDirectory(clangStaticAnalyzerCheckers, "StaticAnalyzer/Checkers/cert");
    addClangDirectory(clangStaticAnalyzerCheckers, "StaticAnalyzer/Checkers/MPI-Checker");
    addClangDirectory(clangStaticAnalyzerCheckers, "StaticAnalyzer/Checkers/RetainCountChecker");
    addClangDirectory(clangStaticAnalyzerCheckers, "StaticAnalyzer/Checkers/UninitializedObject");
    addClangDirectory(clangStaticAnalyzerCheckers, "StaticAnalyzer/Checkers/WebKit");

    DSC<CppTarget> &clangStaticAnalyzerFrontend =
        config.getCppStaticDSC("clangStaticAnalyzerFrontend").publicDeps(llvmSupport);
    addClangDirectory(clangStaticAnalyzerFrontend, "StaticAnalyzer/Frontend");

    DSC<CppTarget> &clangFrontendTool =
        config.getCppStaticDSC("clangFrontendTool")
            .publicDeps(llvmOption, llvmSupport, clangScalableStaticAnalysisFrameworkCore,
                        clangScalableStaticAnalysisFrameworkAnalyses);
    addClangDirectory(clangFrontendTool, "FrontendTool");

    DSC<CppTarget> &clangRewriteFrontend = config.getCppStaticDSC("clangRewriteFrontend").publicDeps(llvmSupport);
    addClangDirectory(clangRewriteFrontend, "Frontend/Rewrite");

    DSC<CppTarget> &clangInstallAPI =
        config.getCppStaticDSC("clangInstallAPI")
            .publicDeps(llvmSupport, llvmTextAPI, llvmTextAPIBinaryReader, llvmDemangle, llvmCore);
    addClangDirectory(clangInstallAPI, "InstallAPI", "");

    if (config.name == "standard")
    {
        // Not added because of TableGenBackends.h name collision in "clang/utils/TableGen" directory
        DSC<CppTarget> &clangTableGen =
            config.getCppExeDSC("ClangTableGen").privateDeps(llvmDemangle, llvmSupport, llvmTableGen);
        clangTableGen.getLOAT().setOutputName("clang-tblgen");
        clangTableGen.getSourceTarget()
            .moduleDirsRE("clang/utils/TableGen", ".*cpp")
            .moduleDirsRE("clang/lib/Support", ".*cpp")
            .privateIncludes("clang/utils/TableGen");
    }

    DSC<CppTarget> &llvmTableGenExe =
        config.getCppExeDSC("LLVMTableGenExe").privateDeps(llvmCodeGenTypes, llvmDemangle, llvmSupport, llvmTableGen);
    llvmTableGenExe.getSourceTarget().privateIncludesSource("llvm/utils/TableGen");
    llvmTableGenExe.getLOAT().setOutputName("llvm-tblgen");
    if (bsMode == BSMode::CONFIGURE)
    {
        set<string> llvmTableGenNoInclude;
        llvmTableGenNoInclude.emplace("llvm-min-tblgen.cpp");
        editOutFilesRecursive(llvmTableGenExe.getSourceTargetPointer(), "llvm/utils/TableGen", ".cpp",
                              llvmTableGenNoInclude);
    }

    DSC<CppTarget> &clang = config.getCppExeDSC("clang").privateDeps(
        llvmDemangle, llvmSupport, llvmPlugins, llvmBitstreamReader, llvmExtensions, llvmFrontendDirective, llvmOption,
        llvmDebugInfoMSF, llvmDebugInfoCodeView, clangSupport, llvmCodeGenTypes, llvmRemarks, llvmDebugInfoBTF,
        llvmTargetParser, llvmBinaryFormat, llvmWindowsDriver, llvmCore, llvmDebugInfoDWARFLowLevel, llvmTextAPI,
        llvmBitReader, llvmFrontendHLSL, llvmCFGuard, llvmAsmParser, llvmMC, clangIPC2978, llvmIRReader, llvmMCParser,
        llvmMCDisassembler, llvmX86Info, llvmObject, llvmX86Disassembler, llvmX86Desc, llvmObjectYAML,
        llvmDebugInfoDWARF, llvmDebugInfoPDB, llvmX86AsmParser, llvmDebugInfoGSYM, llvmTextAPIBinaryReader,
        llvmSymbolize, llvmProfileData, llvmAnalysis, llvmCoverage, llvmIRPrinter, llvmBitWriter, llvmFrontendAtomic,
        llvmTransformUtils, llvmTarget, llvmSandboxIR, llvmCGData, llvmFrontendOffloading, llvmInstrumentation,
        llvmAggressiveInstCombine, llvmInstCombine, llvmObjCARCOpts, llvmHipStdPar, llvmLinker, llvmVectorize,
        llvmFrontendDriver, llvmScalarOpts, llvmCodeGen, llvmFrontendOpenMP, llvmSelectionDAG, llvmAsmPrinter, llvmipo,
        clangBasic, llvmGlobalISel, llvmCoroutines, clangAPINotes, clangLex, clangOptions, llvmX86CodeGen, llvmPasses,
        clangAST, clangRewrite, llvmLTO, clangASTMatchers, clangEdit, clangInstallAPI, clangToolingCore, clangAnalysis,
        clangAnalysisLifetimeSafety, clangSema, clangParse, clangSerialization, clangFrontend, clangDriver,
        clangRewriteFrontend, clangCrossTU, clangExtractAPI, clangStaticAnalyzerCore, clangStaticAnalyzerCheckers,
        clangStaticAnalyzerFrontend, clangFrontendTool, clangCodeGen, clangDependencyScanning,
        clangScalableStaticAnalysisFrameworkCore, clangScalableStaticAnalysisFrameworkAnalyses,
        clangScalableStaticAnalysisFrameworkFrontend, clangUnifiedSymbolResolution);
    clang.getLOAT().setOutputName("clang-23");
    clang.getSourceTarget()
        .moduleDirsRE("clang/tools/driver", ".*cpp")
        .moduleFiles("llvm/my-fork/tools/clang/tools/driver/clang-driver.cpp")
        .privateIncludes("clang/tools/driver");

    // We are adding following includes for all the targets. And these are added before the by-default includes of the
    // clang compiler. so system llvm headers do not muddle with these. this should be done with target declaration and
    // should be more nuanced.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (config.name == "standard")
        {
            for (CppTarget *t : config.cppTargets)
            {
                if (t->name == "std-cpp")
                {
                    continue;
                }
                vector<InclNode> vec = std::move(t->reqIncls);
                t->reqIncls.clear();
                t->privateIncludesSource("llvm/my-fork/include", "llvm/include", "clang/include",
                                         "llvm/my-fork/tools/clang/include");
                for (auto &inclNode : vec)
                {
                    t->privateIncludesSource(inclNode.node->filePath);
                }
            }
        }
    }
}

void buildSpecification()
{
    getConfiguration("standard");
    // Compilation does not work with big header-units.
    getConfiguration("hu").assign(IsCppMod::YES, BigHeaderUnit::NO, UseConfigurationScope::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
