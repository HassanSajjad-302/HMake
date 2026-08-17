#!/usr/bin/env python3
"""Scan decentralized UE HMake files and generate the root hmake.cpp."""

from __future__ import annotations

import argparse
import json
import shlex
from pathlib import Path

IGNORED_DIRECTORIES = {".git", "Binaries", "Build", "Intermediate", "Saved"}
DEFAULT_METADATA = Path(
    "Engine/Intermediate/Build/Linux/x64/UnrealServer/Debug/UnrealServerMetadata.txt"
)


def default_ue_root() -> Path:
    current = Path.cwd().resolve()
    if (current / "Engine" / "Source").is_dir():
        return current

    sibling = Path(__file__).resolve().parents[3] / "UnrealEngine"
    return sibling if (sibling / "Engine" / "Source").is_dir() else current


def scan(ue_root: Path) -> list[Path]:
    files: list[Path] = []
    for file in ue_root.rglob("*.hmake.hpp"):
        relative_parts = file.relative_to(ue_root).parts
        if any(part in IGNORED_DIRECTORIES for part in relative_parts):
            continue
        files.append(file.resolve())

    files.sort(key=lambda file: file.as_posix())
    return files


def cpp_string(value: str) -> str:
    return json.dumps(value)


def cpp_raw_string(value: str) -> str:
    delimiter = "HMAKE_UE_COMMAND"
    if f"){delimiter}\"" in value:
        raise ValueError("Command contains the generated C++ raw-string delimiter")
    return f'R"{delimiter}({value}){delimiter}"'


def read_base_command(metadata: Path) -> str:
    lines = metadata.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        if line.strip() != "BASE-COMMAND:":
            continue
        for command in lines[index + 1 :]:
            if command.strip():
                return command.strip()
    raise ValueError(f"Could not locate BASE-COMMAND in UBT metadata: {metadata}")


def resolve_ubt_path(value: str, ue_root: Path) -> str:
    path = Path(value)
    if not path.is_absolute():
        path = ue_root / "Engine" / "Source" / path
    return path.resolve().as_posix()


def normalize_path_argument(argument: str, ue_root: Path) -> str:
    for prefix in ("-isystem", "--sysroot=", "-include", "-I", "-B", "-L"):
        if argument.startswith(prefix) and len(argument) > len(prefix):
            return prefix + resolve_ubt_path(argument[len(prefix) :], ue_root)
    return argument


def normalize_response_argument(argument: str, ue_root: Path) -> str:
    normalized = normalize_path_argument(argument, ue_root)
    if normalized == argument and not argument.startswith("-") and "/" in argument:
        return resolve_ubt_path(argument, ue_root)
    return normalized


def compile_commands(base_command: str, ue_root: Path) -> tuple[str, str]:
    arguments = shlex.split(base_command)
    if not arguments:
        raise ValueError("UBT BASE-COMMAND is empty")

    compiler = Path(arguments[0])
    if not compiler.is_absolute():
        compiler = ue_root / "Engine" / "Source" / compiler
    compiler = compiler.resolve()
    if not compiler.is_file():
        raise ValueError(f"UBT compiler does not exist: {compiler}")
    arguments[0] = compiler.as_posix()

    normalized: list[str] = []
    for argument in arguments:
        if argument == "-c" or argument == "-DHMAKE_COMPILE_GENERATED_CPP_SEPARATELY=1":
            continue
        if argument in ("-I.", "-I./"):
            continue
        normalized.append(normalize_path_argument(argument, ue_root))

    engine_source = (ue_root / "Engine" / "Source").resolve()
    normalized.extend((f"-I{engine_source.as_posix()}", f"-include{(engine_source / 'HMakeSharedDefs.h').as_posix()}"))

    c_arguments = normalized.copy()
    try:
        language_index = c_arguments.index("-x") + 1
    except ValueError as error:
        raise ValueError("UBT BASE-COMMAND does not select a source language with -x") from error
    if language_index == len(c_arguments) or c_arguments[language_index] != "c++":
        raise ValueError("UBT BASE-COMMAND does not end in the expected C++ language mode")
    c_arguments[language_index] = "c"
    c_arguments = [argument for argument in c_arguments if not argument.startswith("-std=c++")]

    return shlex.join(normalized) + " ", shlex.join(c_arguments) + " "


def read_response_arguments(response_file: Path) -> list[str]:
    arguments: list[str] = []
    for line in response_file.read_text(encoding="utf-8").splitlines():
        if line.strip():
            arguments.extend(shlex.split(line))
    return arguments


def link_commands(base_command: str, metadata: Path, ue_root: Path) -> tuple[str, str, str, str]:
    link_responses = sorted(metadata.parent.glob("*.link.rsp"))
    if len(link_responses) != 1:
        raise ValueError(
            f"Expected exactly one UBT link response beside {metadata}, found {len(link_responses)}"
        )

    compiler = Path(shlex.split(base_command)[0])
    if not compiler.is_absolute():
        compiler = ue_root / "Engine" / "Source" / compiler
    compiler = compiler.resolve()
    linker_arguments = [
        normalize_response_argument(argument, ue_root)
        for argument in read_response_arguments(link_responses[0])
    ]
    try:
        output_index = linker_arguments.index("-o")
        inputs_index = next(
            index for index, argument in enumerate(linker_arguments) if argument.startswith("-Wl,@")
        )
    except (ValueError, StopIteration) as error:
        raise ValueError(f"Unexpected UBT link-response layout: {link_responses[0]}") from error

    link_command = shlex.join([compiler.as_posix(), *linker_arguments[:output_index], "-o"]) + ' "'
    # MsQuic is represented by its prebuilt HMake target, so the graph supplies its link argument.
    link_command_suffix = shlex.join(
        argument for argument in linker_arguments[inputs_index + 1 :] if argument != "-lmsquic"
    ) + " "

    input_response = linker_arguments[inputs_index][len("-Wl,@") :]
    input_response_path = Path(input_response)
    if not input_response_path.is_absolute():
        input_response_path = ue_root / "Engine" / "Source" / input_response_path
    input_response_path = input_response_path.resolve()
    if not input_response_path.is_file():
        raise ValueError(f"UBT linker-input response does not exist: {input_response_path}")

    rpaths: list[str] = []
    shared_library_dirs: list[str] = []
    bootstrap_libraries: list[str] = []
    for argument in read_response_arguments(input_response_path):
        if argument.startswith("-rpath="):
            rpaths.append("-Wl," + argument)
        elif argument.startswith("-rpath-link="):
            rpaths.append("-Wl,-rpath-link=" + resolve_ubt_path(argument[len("-rpath-link=") :], ue_root))
        elif argument.startswith("-L"):
            normalized = normalize_path_argument(argument, ue_root)
            if "/Binaries/ThirdParty/MsQuic/" in normalized:
                shared_library_dirs.append(normalized)
        elif argument.endswith("/uejpeg_linux.a"):
            bootstrap_libraries.append(resolve_ubt_path(argument, ue_root))

    if not bootstrap_libraries:
        raise ValueError(f"Could not locate UEJpegComp's bootstrap archive in {input_response_path}")
    link_dependencies_prefix = shlex.join(
        [*rpaths, *shared_library_dirs, "-Wl,--start-group", *bootstrap_libraries]
    ) + " "

    archiver = compiler.with_name("llvm-ar")
    if not archiver.is_file():
        raise ValueError(f"UBT archiver does not exist beside its compiler: {archiver}")
    archive_command = shlex.join([archiver.as_posix(), "rcs"]) + ' "'
    return link_command, link_dependencies_prefix, link_command_suffix, archive_command


def load_build_commands(metadata: Path, ue_root: Path) -> list[dict[str, str]]:
    base_command = read_base_command(metadata)
    cpp_command, c_command = compile_commands(base_command, ue_root)
    link_command, link_dependencies_prefix, link_command_suffix, archive_command = link_commands(
        base_command, metadata, ue_root
    )
    return [
        {
            "platform": "Linux",
            "architecture": "x64",
            "buildConfiguration": "Debug",
            "targetType": "Server",
            "cppCompileCommand": cpp_command,
            "cCompileCommand": c_command,
            "linkCommand": link_command,
            "linkDependenciesPrefix": link_dependencies_prefix,
            "linkCommandSuffix": link_command_suffix,
            "archiveCommand": archive_command,
        }
    ]


def ispc_compile_environment(cpp_compile_command: str, ue_root: Path) -> tuple[list[str], list[str]]:
    """Extract target-wide ISPC include directories and definitions from UBT's shared C++ command.

    UBT builds each ISPC action from the same target definitions plus the evaluated module's include paths and
    definitions. The latter stay structured in HMake; extracting the former here avoids reparsing the large bootstrap
    command for every module during configuration.
    """

    include_directories = [(ue_root / "Engine" / "Source").as_posix()]
    definition_arguments: list[str] = []
    inside_target_definitions = False
    found_target_definition_end = False
    for argument in shlex.split(cpp_compile_command):
        if argument.startswith("-DIS_PROGRAM="):
            inside_target_definitions = True
        if not inside_target_definitions or not argument.startswith("-D"):
            continue

        # UBT's EscapeDefinitionForISPC removes one ordinary C/C++ quoting layer. shlex has already removed the
        # shell layer, leaving quotes that are part of a macro value (for example UE_APP_NAME="UnrealServer").
        definition = argument[2:]
        if "=" in definition:
            name, value = definition.split("=", 1)
            if len(value) >= 2 and value[0] == value[-1] == '"':
                value = value[1:-1]
            definition = f"{name}={value}"

        # Matches ISPCToolChain's universal-character guard.
        if "\\\\U" not in definition and "\\\\u" not in definition:
            definition_arguments.append(f"-D{definition}")
        if definition.startswith(("UE_BUILD_DEBUG=", "UE_BUILD_DEVELOPMENT=", "UE_BUILD_TEST=", "UE_BUILD_SHIPPING=")):
            found_target_definition_end = True
            break

    if not inside_target_definitions or not found_target_definition_end:
        raise ValueError("Could not locate UBT's target-definition range in cppCompileCommand")
    return include_directories, definition_arguments


def generate(files: list[Path], commands: list[dict[str, str]], ue_root: Path) -> str:
    generated_include_root = (
        ue_root / "Engine" / "Intermediate" / "Build" / "Linux" / "UnrealServer" / "Inc"
    ).as_posix()
    ispc_compiler = (
        ue_root / "Engine" / "Source" / "ThirdParty" / "Intel" / "ISPC" / "bin" / "Linux" / "ispc"
    ).as_posix()
    lines = [
        "// Generated by Projects/UE5/scanner.py. Do not edit.",
        '#include "Configure.hpp"',
        "",
        "namespace",
        "{",
    ]

    for index, file in enumerate(files):
        lines.extend(
            [
                f"namespace ue_generated_file_{index}",
                "{",
                f"#include {cpp_string(file.as_posix())}",
                "}",
                "",
            ]
        )

    lines.extend(
        [
            "constexpr UeIncludedFile generatedUeFiles[] =",
            "{",
        ]
    )
    for index, file in enumerate(files):
        lines.append(
            f"    {{.path = {cpp_string(file.as_posix())}, .func = ue_generated_file_{index}::specify}},"
        )
    lines.extend(
        [
            "};",
            "",
            "// Bootstrap command row derived from this checkout's successful local UBT invocation:",
            "//     make UnrealServer-Linux-Debug",
            "// cppCompileCommand comes from UnrealServerMetadata.txt's BASE-COMMAND and maps to the shared",
            "// CppCompileEnvironment/toolchain prefix. HMake removes UBT's -c because CppSrc appends -c,",
            "// dependency, source, and -o arguments per file. cCompileCommand selects C mode on that prefix.",
            "// Link fields come from UBT's adjacent response files; archiveCommand uses llvm-ar beside clang++.",
            "// HMake launches these commands directly while they fit the configured command-line",
            "// threshold; oversized commands use a generated response file in the owning target's build directory.",
            "//",
            "// Engine/Source is appended as the target-wide default include root. This maps to",
            "// UBT's default include-path setup and supports includes beginning with Runtime/,",
            "// Developer/, Editor/, and Programs/ without exposing an HMake-only module API.",
            "// ispcIncludeDirectories and ispcDefinitionArguments contain UBT's shared target compile environment.",
            "// IspcTarget appends each module's finalized include paths and compile definitions separately.",
            "//",
            "// Generated C++ follows UEBuildModuleCPP: HMake scans handwritten sources for",
            "// UE_INLINE_GENERATED_CPP_BY_NAME(Name), leaves those includes active, and compiles",
            "// only the remaining UHT *.gen.cpp files (including *.init.gen.cpp) separately.",
            "const UeBuildCommandEntry generatedUeBuildCommands[] =",
            "{",
        ]
    )
    for row in commands:
        ispc_include_directories, ispc_definition_arguments = ispc_compile_environment(
            row["cppCompileCommand"], ue_root
        )
        lines.extend(
            [
                "    {",
                f"        .platform = UePlatform::{row['platform']},",
                f"        .architecture = UeArchitecture::{row['architecture']},",
                f"        .buildConfiguration = UeBuildConfiguration::{row['buildConfiguration']},",
                f"        .targetType = UeTargetType::{row['targetType']},",
                "        .commands =",
                "        {",
                f"            .cppCompileCommand = {cpp_raw_string(row['cppCompileCommand'])},",
                f"            .cCompileCommand = {cpp_raw_string(row['cCompileCommand'])},",
                f"            .linkCommand = {cpp_raw_string(row['linkCommand'])},",
                f"            .linkDependenciesPrefix = {cpp_raw_string(row['linkDependenciesPrefix'])},",
                f"            .linkCommandSuffix = {cpp_raw_string(row['linkCommandSuffix'])},",
                f"            .archiveCommand = {cpp_raw_string(row['archiveCommand'])},",
                "            .ispcIncludeDirectories =",
                "            {",
                *(f"                {cpp_string(directory)}," for directory in ispc_include_directories),
                "            },",
                "            .ispcDefinitionArguments =",
                "            {",
                *(f"                {cpp_string(argument)}," for argument in ispc_definition_arguments),
                "            },",
                "        },",
                "    },",
            ]
        )
    lines.extend(
        [
            "};",
            "} // namespace",
            "",
            "void configurationSpecification(Configuration &)",
            "{",
            "}",
            "",
            "void buildSpecification()",
            "{",
            "    // Keep locally edited Git files standalone while unchanged files use adaptive jumbo compilation.",
            "    adaptiveBuildWorkingSetProvider = WorkingSetProvider::GIT;",
            "",
            "    // A full UnrealServer build can otherwise exhaust Linux memory.",
            "    cache.numberOfBuildProcesses = 22;",
            "",
            "    registerGeneratedUeSpecifyFuncs(generatedUeFiles);",
            "",
            '    UeConfiguration &configuration = getUeConfiguration("UnrealServerLinuxDebug");',
            "    configuration",
            "        .setPlatform(UePlatform::Linux, {UePlatformGroup::Unix, UePlatformGroup::Desktop, UePlatformGroup::Linux})",
            "        .setArchitecture(UeArchitecture::x64)",
            "        .setBuildConfiguration(UeBuildConfiguration::Debug)",
            "        .setUeTargetType(UeTargetType::Server)",
            "        .setBuildCommands(generatedUeBuildCommands)",
            "        .setGeneratedIncludeRoot(Node::getNode(",
            f"            {cpp_string(generated_include_root)}, false))",
            "        // UBT: TargetRules.bCompileISPC plus Linux ISPCToolChain's host compiler.",
            "        .setIspcCompiler(Node::getNode(",
            f"            {cpp_string(ispc_compiler)}, true))",
            '        .requestTarget("UnrealServer");',
            "    // UBT's compile command already selects the bundled libc++ headers. Do not",
            "    // add HMake's host standard-library target (which would leak /usr/include).",
            "    configuration.jumboFileSize = 384 * 1024;",
            "    configuration.assign(ConfigType::DEBUG, TargetType::LIBRARY_STATIC, IsCppMod::NO, JumboBuild::YES,",
            "                         AssignStandardCppTarget::NO);",
            "",
            "    // Expand only the requested UE roots; dependencies continue to be discovered lazily.",
            "    for (const string &target : configuration.requestedTargets)",
            "    {",
            "        configuration.getOrAddTarget(target);",
            "    }",
            "",
            "    CALL_CONFIGURATION_SPECIFICATION",
            "}",
            "",
            "MAIN_FUNCTION",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate hmake.cpp from decentralized UE *.hmake.hpp files."
    )
    parser.add_argument(
        "--ue-root",
        type=Path,
        default=default_ue_root(),
        help="Unreal Engine checkout root (default: current UE checkout or sibling UnrealEngine directory).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Generated C++ file (default: <ue-root>/hmake.cpp).",
    )
    parser.add_argument(
        "--metadata",
        type=Path,
        help=(
            "UBT HMake metadata file "
            "(default: <ue-root>/Engine/Intermediate/Build/Linux/x64/UnrealServer/Debug/UnrealServerMetadata.txt)."
        ),
    )
    args = parser.parse_args()

    ue_root = args.ue_root.expanduser().resolve()
    if not (ue_root / "Engine" / "Source").is_dir():
        parser.error(f"UE root does not contain Engine/Source: {ue_root}")

    output = args.output.expanduser().resolve() if args.output else ue_root / "hmake.cpp"
    metadata = args.metadata.expanduser().resolve() if args.metadata else (ue_root / DEFAULT_METADATA).resolve()
    if not metadata.is_file():
        parser.error(f"UBT metadata does not exist; run the UnrealServer bootstrap first: {metadata}")
    files = scan(ue_root)
    commands = load_build_commands(metadata, ue_root)
    generated = generate(files, commands, ue_root)

    previous = output.read_text(encoding="utf-8") if output.exists() else None
    if previous == generated:
        print(f"Unchanged: {output} ({len(files)} files, {len(commands)} command rows)")
        return

    output.write_text(generated, encoding="utf-8")
    print(f"Generated: {output} ({len(files)} files, {len(commands)} command rows)")


if __name__ == "__main__":
    main()
