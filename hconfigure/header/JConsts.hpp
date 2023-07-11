#ifndef HMAKE_JCONSTS_H
#define HMAKE_JCONSTS_H
#ifdef USE_HEADER_UNITS
import <PlatformSpecific.hpp>;
#else
#include "PlatformSpecific.hpp"
#endif

struct JConsts
{
    inline static const pstring addQuotes = "add-quotes";
    inline static const pstring archive = "archive";
    inline static const pstring archived = "archived";
    inline static const pstring archiveCommandColor = "archive-command-color";
    inline static const pstring archivePrintSettings = "archive-print-settings";
    inline static const pstring archiver = "archiver";
    inline static const pstring archiverArray = "archiver-array";
    inline static const pstring archivers = "archivers";
    inline static const pstring archiverSelectedArrayIndex = "archiver-selected-array-index";
    inline static const pstring binary = "binary";
    inline static const pstring cacheIncludes = "cache-includes";
    inline static const pstring cacheVariables = "cache-variables";
    inline static const pstring clang = "clang";
    inline static const pstring command = "command";
    inline static const pstring commandArguments = "commandArguments";
    inline static const pstring compile = "compile";
    inline static const pstring compileCommand = "compile-command";
    inline static const pstring compileCommandColor = "compile-command-color";
    inline static const pstring compileConfigureCommands = "compile-configureDerived-commands";
    inline static const pstring compileDefinitions = "compile-definitions";
    inline static const pstring compilePrintSettings = "compile-print-settings";
    inline static const pstring compiler = "compiler";
    inline static const pstring compilerArray = "compiler-array";
    inline static const pstring compilerFlags = "compiler-flags";
    inline static const pstring compilers = "compilers";
    inline static const pstring compilerSelectedArrayIndex = "compiler-selected-array-index";
    inline static const pstring compilerTransitiveFlags = "compiler-transitive-flags";
    inline static const pstring configuration = "configuration";
    inline static const pstring configurationScope = "configuration-scope";
    inline static const pstring consumerDependencies = "consumer-dependencies";
    inline static const pstring copy = "copy";
    inline static const pstring copyingTarget = "copying-target";
    inline static const pstring cppSourceTargets = "cpp-source-targets";
    inline static const pstring debug = "debug";
    inline static const pstring dependencies = "dependencies";
    inline static const pstring depth = "depth";
    inline static const pstring directories = "directories";
    inline static const pstring elements = "elements";
    inline static const pstring standardCompilerFlags = "standard-compiler-flags";
    inline static const pstring standardIncludeDirectories = "standard-include-directories";
    inline static const pstring standardLibraryDirectories = "standard-library-directories";
    inline static const pstring executable = "executable";
    inline static const pstring family = "family";
    inline static const string fileName = "fileName";
    inline static const pstring files = "files";
    inline static const pstring gcc = "gcc";
    inline static const pstring generalPrintSettings = "general-print-settings";
    inline static const pstring hbuildErrorOutput = "hbuild-error-output";
    inline static const pstring hbuildSequenceOutput = "hbuild-sequence-output";
    inline static const pstring hbuildStatementOutput = "hbuild-statement-output";
    inline static const pstring headerDependencies = "header-dependencies";
    inline static const pstring headerUnits = "header-units";
    inline static const pstring hmakeFilePath = "hmake-file-path";
    inline static const pstring hostArchitecture = "host-architecture";
    inline static const pstring hostArddressModel = "host-address-model";
    inline static const pstring huIncludeDirectories = "header-unit-include-directories";
    inline static const pstring ifcOutputFile = "ifc-output-file";
    inline static const pstring includeAngle = "include-angle";
    inline static const pstring includeDirectories = "include-directories";
    inline static const pstring index = "index";
    inline static const pstring infrastructureFlags = "infrastructure-flags";
    inline static const pstring interface_ = "interface";
    inline static const pstring isCompilerInToolsArray = "is-compiler-in-tools-array";
    inline static const pstring isLinkerInToolsArray = "is-linker-in-tools-array";
    inline static const pstring isArchiverInToolsArray = "is-archiver-in-tools-array";
    inline static const pstring libraryDependencies = "library-dependencies";
    inline static const pstring libraryDirectories = "library-directories";
    inline static const pstring libraryType = "library-type";
    inline static const pstring linkCommand = "link-command";
    inline static const pstring linkCommandColor = "link-command-color";
    inline static const pstring linker = "linker";
    inline static const pstring linkerArray = "linker-array";
    inline static const pstring linkerFlags = "linker-flags";
    inline static const pstring linkers = "linkers";
    inline static const pstring linkerSelectedArrayIndex = "linker-selected-array-index";
    inline static const pstring linkerTransitiveFlags = "linker-transitive-flags";
    inline static const pstring linkPrintSettings = "link-print-settings";
    inline static const pstring linuxTools = "linux-tools";
    inline static const pstring linuxUnix = "linux-unix";
    inline static const pstring logicalName = "logical-name";
    inline static const string lookupMethod = "lookup-method";
    inline static const pstring maximumBuildThreads = "maximum-build-threads";
    inline static const pstring maximumLinkThreads = "maximum-link-threads";
    inline static const pstring moduleDependencies = "module-dependencies";
    inline static const pstring moduleScope = "module-scope";
    inline static const pstring msvc = "msvc";
    inline static const pstring name = "name";
    inline static const pstring object = "object";
    inline static const pstring objectFile = "object-file";
    inline static const pstring objectFiles = "object-files";
    inline static const pstring onlyLogicalNameOfRequireIfc = "only-logical-name-of-require-ifc";
    inline static const pstring outputAndErrorFiles = "output-and-error-files";
    inline static const pstring outputDirectory = "output-directory";
    inline static const pstring outputName = "output-name";
    inline static const pstring packageName = "package-name";
    inline static const pstring packagePath = "package-path";
    inline static const pstring packageVariantIndex = "package-variant-index";
    inline static const pstring packageVariantJson = "package-variant-json";
    inline static const pstring packageVersion = "package-version";
    inline static const pstring path = "path";
    inline static const pstring pathPrintLevel = "path-print-level";
    inline static const pstring plibraryShared = "plibrary-shared";
    inline static const pstring plibraryStatic = "plibrary-static";
    inline static const pstring postBuildCommands = "post-build-commands";
    inline static const pstring postBuildCommandsStatement = "post-build-commands-statement";
    inline static const pstring postBuildCustomCommands = "post-build-custom-commands";
    inline static const pstring preBuildCommands = "pre-build-commands";
    inline static const pstring preBuildCommandsStatement = "pre-build-commands-statement";
    inline static const pstring preBuildCustomCommands = "pre-build-custom-commands";
    inline static const pstring prebuilt = "prebuilt";
    inline static const pstring preprocess = "preprocess";
    inline static const pstring printColorSettings = "print-color-settings";
    inline static const pstring private_ = "private";
    inline static const pstring provides = "provides";
    inline static const pstring project = "project";
    inline static const pstring projectIncludeDirectories = "project-include-directories";
    inline static const pstring pruneCompiledSourceFileNameFromMsvcOutput =
        "prune-compiled-source-file-name-from-msvc-output";
    inline static const pstring pruneHeaderDependenciesFromMsvcOutput = "prune-header-dependencies-from-msvc-output";
    inline static const pstring public_ = "public";
    inline static const pstring ratioForHmakeTime = "ratio-for-hmake-time";
    inline static const pstring regexPString = "regex-pstring";
    inline static const pstring release = "release";
    inline static const pstring requireIfcs = "require-ifcs";
    inline static const pstring requires_ = "requires";
    inline static const pstring rules = "rules";
    inline static const pstring run = "run";
    inline static const pstring shared = "shared";
    inline static const pstring showPercentage = "show-percentage";
    inline static const pstring smFileType = "smfile-type";
    inline static const pstring smrules = "smrules";
    inline static const pstring sourceDependencies = "source-dependencies";
    inline static const pstring sourceDirectory = "source-directory";
    inline static const pstring sourceFile = "source-file";
    inline static const pstring sourcePath = "source-path";
    inline static const pstring srcFile = "src-file";
    inline static const pstring static_ = "static";
    inline static const pstring targetAddressModel = "target-address-model";
    inline static const pstring targetArchitecture = "target-architecture";
    inline static const pstring targets = "targets";
    inline static const pstring targetType = "target-type";
    inline static const pstring threadId = "thread-id";
    inline static const pstring tool = "tool";
    inline static const pstring toolErrorOutput = "tool-error-output";
    inline static const pstring useIndex = "use-index";
    inline static const pstring value = "value";
    inline static const pstring variant = "variant";
    inline static const pstring variants = "variants";
    inline static const pstring variantsIndices = "variants-indices";
    inline static const pstring version = "version";
    inline static const pstring vsTools = "vs-tools";
    inline static const pstring windows = "windows";
    inline static const pstring x86 = "X86";
};

#endif // HMAKE_JCONSTS_H

