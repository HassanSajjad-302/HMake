#ifndef HMAKE_JCONSTS_H
#define HMAKE_JCONSTS_H
#ifdef USE_HEADER_UNITS
import <string>;
#else
#include <string>
#endif

using std::string;
struct JConsts
{
    inline static const string addQuotes = "add-quotes";
    inline static const string archive = "archive";
    inline static const string archived = "archived";
    inline static const string archiveCommandColor = "archive-command-color";
    inline static const string archivePrintSettings = "archive-print-settings";
    inline static const string archiver = "archiver";
    inline static const string archiverArray = "archiver-array";
    inline static const string archivers = "archivers";
    inline static const string archiverSelectedArrayIndex = "archiver-selected-array-index";
    inline static const string binary = "binary";
    inline static const string cacheIncludes = "cache-includes";
    inline static const string cacheVariables = "cache-variables";
    inline static const string clang = "clang";
    inline static const string command = "command";
    inline static const string commandArguments = "commandArguments";
    inline static const string compile = "compile";
    inline static const string compileCommand = "compile-command";
    inline static const string compileCommandColor = "compile-command-color";
    inline static const string compileConfigureCommands = "compile-configureDerived-commands";
    inline static const string compileDefinitions = "compile-definitions";
    inline static const string compilePrintSettings = "compile-print-settings";
    inline static const string compiler = "compiler";
    inline static const string compilerArray = "compiler-array";
    inline static const string compilerFlags = "compiler-flags";
    inline static const string compilers = "compilers";
    inline static const string compilerSelectedArrayIndex = "compiler-selected-array-index";
    inline static const string compilerTransitiveFlags = "compiler-transitive-flags";
    inline static const string configuration = "configuration";
    inline static const string configurationScope = "configuration-scope";
    inline static const string consumerDependencies = "consumer-dependencies";
    inline static const string copy = "copy";
    inline static const string copyingTarget = "copying-target";
    inline static const string cppSourceTargets = "cpp-source-targets";
    inline static const string debug = "debug";
    inline static const string dependencies = "dependencies";
    inline static const string depth = "depth";
    inline static const string directories = "directories";
    inline static const string elements = "elements";
    inline static const string standardCompilerFlags = "standard-compiler-flags";
    inline static const string standardIncludeDirectories = "standard-include-directories";
    inline static const string standardLibraryDirectories = "standard-library-directories";
    inline static const string executable = "executable";
    inline static const string family = "family";
    inline static const string files = "files";
    inline static const string gcc = "gcc";
    inline static const string generalPrintSettings = "general-print-settings";
    inline static const string hbuildErrorOutput = "hbuild-error-output";
    inline static const string hbuildSequenceOutput = "hbuild-sequence-output";
    inline static const string hbuildStatementOutput = "hbuild-statement-output";
    inline static const string headerDependencies = "header-dependencies";
    inline static const string headerUnits = "header-units";
    inline static const string hmakeFilePath = "hmake-file-path";
    inline static const string hostArchitecture = "host-architecture";
    inline static const string hostArddressModel = "host-address-model";
    inline static const string huIncludeDirectories = "header-unit-include-directories";
    inline static const string ifcOutputFile = "ifc-output-file";
    inline static const string includeDirectories = "include-directories";
    inline static const string index = "index";
    inline static const string infrastructureFlags = "infrastructure-flags";
    inline static const string interface_ = "interface";
    inline static const string isCompilerInToolsArray = "is-compiler-in-tools-array";
    inline static const string isLinkerInToolsArray = "is-linker-in-tools-array";
    inline static const string isArchiverInToolsArray = "is-archiver-in-tools-array";
    inline static const string libraryDependencies = "library-dependencies";
    inline static const string libraryDirectories = "library-directories";
    inline static const string libraryType = "library-type";
    inline static const string linkCommand = "link-command";
    inline static const string linkCommandColor = "link-command-color";
    inline static const string linker = "linker";
    inline static const string linkerArray = "linker-array";
    inline static const string linkerFlags = "linker-flags";
    inline static const string linkers = "linkers";
    inline static const string linkerSelectedArrayIndex = "linker-selected-array-index";
    inline static const string linkerTransitiveFlags = "linker-transitive-flags";
    inline static const string linkPrintSettings = "link-print-settings";
    inline static const string linuxTools = "linux-tools";
    inline static const string linuxUnix = "linux-unix";
    inline static const string maximumBuildThreads = "maximum-build-threads";
    inline static const string maximumLinkThreads = "maximum-link-threads";
    inline static const string moduleDependencies = "module-dependencies";
    inline static const string moduleScope = "module-scope";
    inline static const string msvc = "msvc";
    inline static const string name = "name";
    inline static const string object = "object";
    inline static const string objectFile = "object-file";
    inline static const string objectFiles = "object-files";
    inline static const string onlyLogicalNameOfRequireIfc = "only-logical-name-of-require-ifc";
    inline static const string outputAndErrorFiles = "output-and-error-files";
    inline static const string outputDirectory = "output-directory";
    inline static const string outputName = "output-name";
    inline static const string packageName = "package-name";
    inline static const string packagePath = "package-path";
    inline static const string packageVariantIndex = "package-variant-index";
    inline static const string packageVariantJson = "package-variant-json";
    inline static const string packageVersion = "package-version";
    inline static const string path = "path";
    inline static const string pathPrintLevel = "path-print-level";
    inline static const string plibraryShared = "plibrary-shared";
    inline static const string plibraryStatic = "plibrary-static";
    inline static const string postBuildCommands = "post-build-commands";
    inline static const string postBuildCommandsStatement = "post-build-commands-statement";
    inline static const string postBuildCustomCommands = "post-build-custom-commands";
    inline static const string preBuildCommands = "pre-build-commands";
    inline static const string preBuildCommandsStatement = "pre-build-commands-statement";
    inline static const string preBuildCustomCommands = "pre-build-custom-commands";
    inline static const string prebuilt = "prebuilt";
    inline static const string preprocess = "preprocess";
    inline static const string printColorSettings = "print-color-settings";
    inline static const string private_ = "private";
    inline static const string project = "project";
    inline static const string projectIncludeDirectories = "project-include-directories";
    inline static const string pruneCompiledSourceFileNameFromMsvcOutput =
        "prune-compiled-source-file-name-from-msvc-output";
    inline static const string pruneHeaderDependenciesFromMsvcOutput = "prune-header-dependencies-from-msvc-output";
    inline static const string public_ = "public";
    inline static const string ratioForHmakeTime = "ratio-for-hmake-time";
    inline static const string regexString = "regex-string";
    inline static const string release = "release";
    inline static const string requireIfcs = "require-ifcs";
    inline static const string run = "run";
    inline static const string shared = "shared";
    inline static const string showPercentage = "show-percentage";
    inline static const string sourceDependencies = "source-dependencies";
    inline static const string sourceDirectory = "source-directory";
    inline static const string sourceFile = "source-file";
    inline static const string srcFile = "src-file";
    inline static const string static_ = "static";
    inline static const string targetAddressModel = "target-address-model";
    inline static const string targetArchitecture = "target-architecture";
    inline static const string targets = "targets";
    inline static const string targetType = "target-type";
    inline static const string threadId = "thread-id";
    inline static const string tool = "tool";
    inline static const string toolErrorOutput = "tool-error-output";
    inline static const string useIndex = "use-index";
    inline static const string value = "value";
    inline static const string variant = "variant";
    inline static const string variants = "variants";
    inline static const string variantsIndices = "variants-indices";
    inline static const string version = "version";
    inline static const string vsTools = "vs-tools";
    inline static const string windows = "windows";
    inline static const string x86 = "X86";
};

#endif // HMAKE_JCONSTS_H

