#!/usr/bin/env python3
"""Scan decentralized UE HMake files and generate the root hmake.cpp."""

from __future__ import annotations

import argparse
import json
import re
import shlex
from dataclasses import dataclass
from pathlib import Path

IGNORED_DIRECTORIES = {".git", "Binaries", "Build", "Intermediate", "Saved"}
DEFAULT_METADATA = Path(
    "Engine/Intermediate/Build/Linux/x64/UnrealServer/Debug/UnrealServerMetadata.txt"
)
DEFAULT_SHARED_DEFINITIONS = Path("Engine/Source/HMakeSharedDefs.h")
HMAKE_SUFFIX = ".hmake.hpp"
METADATA_KEYS = {"name", "kind", "platform", "platformGroup", "configuration"}
UE_FILE_KINDS = {"Module", "Prebuilt", "Target"}
UE_CONFIGURATIONS = {"Default", "RttiExcept"}
UE_PLATFORMS = {"Linux", "Windows", "Mac", "Android", "IOS"}
UE_PLATFORM_GROUPS = {
    "Unix",
    "Windows",
    "Microsoft",
    "Apple",
    "Desktop",
    "Linux",
    "Android",
}
RESERVED_FILENAME_MARKERS = (".module", ".prebuilt", ".target", ".rttiexcept")


@dataclass(frozen=True)
class UeFile:
    path: Path
    logical_name: str
    kind: str = "Module"
    configuration: str = "Default"
    platform_group: str | None = None
    platform: str | None = None


def default_ue_root() -> Path:
    current = Path.cwd().resolve()
    if (current / "Engine" / "Source").is_dir():
        return current

    sibling = Path(__file__).resolve().parents[3] / "UnrealEngine"
    return sibling if (sibling / "Engine" / "Source").is_dir() else current


def scan(ue_root: Path) -> list[UeFile]:
    files: list[UeFile] = []
    for file in ue_root.rglob("*.hmake.hpp"):
        relative_parts = file.relative_to(ue_root).parts
        if any(part in IGNORED_DIRECTORIES for part in relative_parts):
            continue
        files.append(parse_ue_file(file.resolve()))

    files.sort(key=lambda file: file.path.as_posix())
    validate_ue_files(files)
    return files


def parse_front_matter(file: Path) -> dict[str, str]:
    """Parse scanner metadata before the first physical blank line."""

    metadata_lines: list[tuple[int, str]] = []
    found_boundary = False
    for line_number, line in enumerate(file.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            found_boundary = True
            break
        metadata_lines.append((line_number, line))
    if not found_boundary:
        raise ValueError(
            f"{file}: metadata must end with a physical blank line; "
            "put a blank first line in files that use only defaults"
        )

    values: dict[str, str] = {}
    in_block_comment = False
    block_comment_line = 0
    for line_number, line in metadata_lines:
        stripped = line.strip()
        if in_block_comment:
            closing = stripped.find("*/")
            if closing == -1:
                continue
            if stripped[closing + 2 :].strip():
                raise ValueError(f"{file}:{line_number}: text after a metadata block comment is not allowed")
            in_block_comment = False
            continue

        if stripped.startswith("/*"):
            closing = stripped.find("*/", 2)
            if closing == -1:
                in_block_comment = True
                block_comment_line = line_number
            elif stripped[closing + 2 :].strip():
                raise ValueError(f"{file}:{line_number}: text after a metadata block comment is not allowed")
            continue
        if "/*" in stripped or "*/" in stripped:
            raise ValueError(
                f"{file}:{line_number}: metadata block comments must start at the beginning of a line"
            )

        if not stripped.startswith("//"):
            raise ValueError(
                f"{file}:{line_number}: metadata must use '// key = value' assignments or '/* ... */' comments"
            )
        assignment = stripped[2:].strip()
        match = re.fullmatch(r"([A-Za-z][A-Za-z0-9]*)\s*=\s*(\S+)", assignment)
        if match is None:
            raise ValueError(
                f"{file}:{line_number}: expected '// key = value'; use '/* ... */' for metadata comments"
            )
        key, value = match.groups()
        if key not in METADATA_KEYS:
            raise ValueError(f"{file}:{line_number}: unknown UE metadata key '{key}'")
        if key in values:
            raise ValueError(f"{file}:{line_number}: duplicate UE metadata key '{key}'")
        values[key] = value

    if in_block_comment:
        raise ValueError(
            f"{file}:{block_comment_line}: metadata block comment must close before the first blank line"
        )
    return values


def parse_ue_file(file: Path) -> UeFile:
    """Return one strictly validated UE registration parsed from scanner front matter."""

    name = file.name
    if not name.endswith(HMAKE_SUFFIX):
        raise ValueError(f"UE specification does not end in '{HMAKE_SUFFIX}': {file}")
    stem = name[: -len(HMAKE_SUFFIX)]
    if (
        any(stem.endswith(marker) for marker in RESERVED_FILENAME_MARKERS)
        or ".group." in stem
        or ".platform." in stem
    ):
        raise ValueError(
            f"{file}: semantic filename suffixes are invalid; "
            "use '<name>.hmake.hpp' and declare settings in scanner front matter"
        )
    metadata = parse_front_matter(file)
    logical_name = metadata.get("name", stem)
    if not logical_name:
        raise ValueError(f"{file}: UE specification has no logical name")

    kind = metadata.get("kind", "Module")
    configuration = metadata.get("configuration", "Default")
    platform_group = metadata.get("platformGroup")
    platform = metadata.get("platform")
    if kind not in UE_FILE_KINDS:
        raise ValueError(f"{file}: unknown UE file kind '{kind}'")
    if configuration not in UE_CONFIGURATIONS:
        raise ValueError(f"{file}: unknown UE configuration profile '{configuration}'")
    if platform_group is not None and platform_group not in UE_PLATFORM_GROUPS:
        raise ValueError(f"{file}: unknown UE platform group '{platform_group}'")
    if platform is not None and platform not in UE_PLATFORMS:
        raise ValueError(f"{file}: unknown UE platform '{platform}'")
    if platform_group is not None and platform is not None:
        raise ValueError(f"{file}: UE specification cannot select both platformGroup and platform")
    if configuration != "Default" and kind != "Module":
        raise ValueError(f"{file}: configuration = {configuration} is valid only for UE modules")

    return UeFile(
        path=file,
        logical_name=logical_name,
        kind=kind,
        configuration=configuration,
        platform_group=platform_group,
        platform=platform,
    )


def validate_ue_files(files: list[UeFile]) -> None:
    """Reject inconsistent identities and duplicate base/specialized registrations."""

    identities: dict[str, tuple[str, str, Path]] = {}
    registrations: dict[tuple[str, str, str], Path] = {}
    for file in files:
        previous = identities.get(file.logical_name)
        if previous is None:
            identities[file.logical_name] = (file.kind, file.configuration, file.path)
        else:
            previous_kind, previous_configuration, previous_file = previous
            if previous_kind != file.kind:
                raise ValueError(
                    f"UE logical target '{file.logical_name}' has conflicting kinds:\n"
                    f"  {previous_file}\n  {file.path}"
                )
            if previous_configuration != file.configuration:
                raise ValueError(
                    f"Every specialization of UE target '{file.logical_name}' must use the same configuration:\n"
                    f"  {previous_file}\n  {file.path}"
                )

        if file.platform_group is not None:
            selector = (file.logical_name, "platformGroup", file.platform_group)
        elif file.platform is not None:
            selector = (file.logical_name, "platform", file.platform)
        else:
            selector = (file.logical_name, "base", "")
        previous_file = registrations.get(selector)
        if previous_file is not None:
            raise ValueError(
                f"Duplicate UE registration for '{file.logical_name}' ({selector[1]} {selector[2]}):\n"
                f"  {previous_file}\n  {file.path}"
            )
        registrations[selector] = file.path

    for logical_name, (_, _, first_file) in identities.items():
        if (logical_name, "base", "") not in registrations:
            raise ValueError(
                f"UE logical target '{logical_name}' has specialized registrations but no base registration:\n"
                f"  {first_file}"
            )


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


DEFINE_PATTERN = re.compile(r"^#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)(?:[ \t]+(.*))?$")


def read_definition_arguments(header: Path, names: set[str] | None = None) -> list[str]:
    """Translate object-like definitions from one UBT-generated header to compiler arguments."""

    if not header.is_file():
        raise ValueError(f"UBT definitions header does not exist; run the UnrealServer bootstrap first: {header}")

    arguments: list[str] = []
    found: set[str] = set()
    for line in header.read_text(encoding="utf-8").splitlines():
        match = DEFINE_PATTERN.fullmatch(line)
        if match is None:
            continue
        name, value = match.groups()
        if names is not None and name not in names:
            continue
        arguments.append(f"-D{name}={value or ''}")
        found.add(name)

    if names is not None and found != names:
        missing = ", ".join(sorted(names - found))
        raise ValueError(f"UBT definitions header is missing expected definitions ({missing}): {header}")
    return arguments


def ubt_definition_arguments(base_command: str, metadata: Path) -> list[str]:
    """Recover definitions that UBT emits into generated headers rather than BASE-COMMAND."""

    standard = next(
        (argument.removeprefix("-std=c++") for argument in shlex.split(base_command) if argument.startswith("-std=c++")),
        None,
    )
    if standard is None:
        raise ValueError("UBT BASE-COMMAND does not select a C++ standard")

    shared_header = metadata.parent / "Core" / f"SharedDefinitions.Core.Cpp{standard}.h"
    arguments = read_definition_arguments(shared_header)

    # These target/toolchain values are consumed only by BuildSettings.cpp, but UBT emits them after its exported
    # ModuleRules metadata. Supplying them in the shared command keeps the decentralized specification portable.
    build_settings_header = metadata.parent / "BuildSettings" / "Definitions.h"
    arguments.extend(read_definition_arguments(build_settings_header, {"UE_WITH_DEBUG_INFO", "UE_VFS_PATHS"}))
    return arguments


def generate_shared_definitions(metadata: Path, ue_root: Path) -> str:
    """Generate the target-wide non-attributed module API definitions expected by UHT output."""

    modules = load_module_generated_metadata(metadata, ue_root)
    macros: set[str] = set()
    for logical_name in modules:
        short_name = logical_name.rsplit("/", 1)[-1]
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", short_name) is None:
            raise ValueError(f"Cannot form a UE module API macro from metadata name: {logical_name}")
        macros.add(f"{short_name.upper()}_NON_ATTRIBUTED_API")

    if not macros:
        raise ValueError(f"UBT metadata contains no modules: {metadata}")

    lines = [
        "// Generated by Projects/UE5/scanner.py. Do not edit.",
        "#pragma once",
        "",
        "// A monolithic target gives every module an empty non-attributed API macro. UHT uses these",
        "// macros for declarations whose position cannot accept the ordinary MODULE_API attribute.",
    ]
    for macro in sorted(macros):
        lines.extend((f"#ifndef {macro}", f"#define {macro}", "#endif"))
    lines.append("")
    return "\n".join(lines)


def compile_commands(
    base_command: str,
    ue_root: Path,
    extra_definition_arguments: list[str] | None = None,
    forced_include: Path | None = None,
) -> tuple[str, str]:
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
        if argument in ("-frtti", "-fno-rtti", "-fexceptions", "-fno-exceptions"):
            continue
        if argument.startswith("-DPLATFORM_EXCEPTIONS_DISABLED="):
            continue
        if argument in ("-I.", "-I./"):
            continue
        normalized.append(normalize_path_argument(argument, ue_root))

    engine_source = (ue_root / "Engine" / "Source").resolve()
    normalized.append(f"-I{engine_source.as_posix()}")
    existing_definitions = {
        argument[2:].partition("=")[0] for argument in normalized if argument.startswith("-D")
    }
    for argument in extra_definition_arguments or ():
        if argument[2:].partition("=")[0] not in existing_definitions:
            normalized.append(argument)
    if forced_include is not None:
        normalized.append(f"-include{forced_include.resolve().as_posix()}")

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


def load_module_generated_metadata(metadata: Path, ue_root: Path) -> dict[str, dict[str, object]]:
    """Read UBT's generated include directories for scanner-time validation."""

    module_pattern = re.compile(r"^Module: (.+)$")
    uht_pattern = re.compile(r"^  8\) UHT-include-dirs \((\d+)\):$")
    vni_pattern = re.compile(r"^  9\) VNI-include-dirs \((\d+)\):$")
    lines = metadata.read_text(encoding="utf-8").splitlines()
    modules: dict[str, dict[str, object]] = {}
    current: dict[str, object] | None = None

    for index, line in enumerate(lines):
        match = module_pattern.match(line)
        if match:
            name = match.group(1)
            current = {
                "logicalName": name,
                "uhtDirectory": "",
                "vniDirectory": "",
            }
            modules[name] = current
            continue
        if current is None:
            continue

        for pattern, key in ((uht_pattern, "uhtDirectory"), (vni_pattern, "vniDirectory")):
            match = pattern.match(line)
            if not match:
                continue
            count = int(match.group(1))
            if count > 1:
                raise ValueError(
                    f"Expected at most one {key} for module {current['logicalName']}, found {count}"
                )
            if count == 1:
                current[key] = resolve_ubt_path(lines[index + 1].strip(), ue_root)
            break

    return modules


SHORT_NAME_PATTERN = re.compile(r'\.setShortName\s*\(\s*"([^"]*)"\s*\)')


def module_intermediate_name(file: UeFile) -> str:
    """Mirror UeCppTarget::intermediateName."""

    match = SHORT_NAME_PATTERN.search(file.path.read_text(encoding="utf-8"))
    return match.group(1) if match else file.logical_name


def module_generated_include_root(configured_root: Path, module_directory: Path) -> Path:
    """Mirror the plugin-aware generated-include calculation in hconfigure/src/ue.cpp."""

    plugin_root: Path | None = None
    for directory in (module_directory, *module_directory.parents):
        if any(directory.glob("*.uplugin")):
            plugin_root = directory
            break
    if plugin_root is None:
        return configured_root

    suffix_parts: list[str] = []
    for component in configured_root.parts:
        if suffix_parts or component == "Intermediate":
            suffix_parts.append(component)
    return plugin_root.joinpath(*suffix_parts) if suffix_parts else configured_root


CYCLE_DEPENDENCY_PATTERN = re.compile(r'add(Private|Public)CycleDependency\("([^"]+)"\)')
UBT_MODULE_LIST_PATTERN = re.compile(r"(Public|Private)(IncludePathModuleNames|DependencyModuleNames)")


def ubt_relation_kinds(module_directory: Path, dependency: str) -> set[str]:
    """Return the ModuleRules list kinds that name one dependency."""

    kinds: set[str] = set()
    for rules in module_directory.glob("*.Build.cs"):
        text = rules.read_text(encoding="utf-8", errors="replace")
        for match in UBT_MODULE_LIST_PATTERN.finditer(text):
            end = text.find(";", match.end())
            if end != -1 and re.search(r'"%s"' % re.escape(dependency), text[match.end() : end]):
                kinds.add(match.group(2))
    return kinds


def verify_ue_specifications(files: list[UeFile], metadata: Path, ue_root: Path) -> None:
    """Cross-check mechanically derived UE data against UBT without emitting it into hmake.cpp."""

    modules = load_module_generated_metadata(metadata, ue_root)
    configured_root = ue_root / "Engine" / "Intermediate" / "Build" / "Linux" / "UnrealServer" / "Inc"
    problems: list[str] = []

    for file in files:
        if file.kind != "Module":
            continue

        # IncludePathModuleNames carries compile visibility but no linker input, so a cyclic form must use the OP API.
        for visibility, dependency in CYCLE_DEPENDENCY_PATTERN.findall(
            file.path.read_text(encoding="utf-8", errors="replace")
        ):
            if ubt_relation_kinds(file.path.parent, dependency) == {"IncludePathModuleNames"}:
                problems.append(
                    f"{file.path}: '{dependency}' is an IncludePathModuleNames relation in UBT, so it must use "
                    f"add{visibility}CycleOpDependency"
                )

        if file.platform_group is not None or file.platform is not None:
            continue
        module = modules.get(file.logical_name)
        if module is None:
            continue

        generated_root = module_generated_include_root(configured_root, file.path.parent)
        generated_root /= module_intermediate_name(file)
        for key, leaf in (("uhtDirectory", "UHT"), ("vniDirectory", "VNI")):
            reported = str(module[key])
            if not reported:
                continue
            computed = (generated_root / leaf).as_posix()
            if computed != reported:
                problems.append(
                    f"{file.path}: HMake derives the {leaf} directory as\n"
                    f"      {computed}\n"
                    f"    but UBT reported\n"
                    f"      {reported}\n"
                    f"    Set the matching cpp.setShortName(...) in this file."
                )

    if problems:
        raise ValueError(
            "UBT metadata disagrees with the decentralized UE specifications:\n  " + "\n  ".join(problems)
        )


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
    # HMake places every graph-discovered static dependency in one group. Close it before replaying UBT's suffix,
    # which can contain a group of its own (lld rejects nested --start-group directives).
    link_command_suffix = shlex.join(
        [
            "-Wl,--end-group",
            *(argument for argument in linker_arguments[inputs_index + 1 :] if argument != "-lmsquic"),
        ]
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


def load_build_commands(metadata: Path, ue_root: Path, shared_definitions: Path) -> list[dict[str, object]]:
    base_command = read_base_command(metadata)
    cpp_command, c_command = compile_commands(
        base_command, ue_root, ubt_definition_arguments(base_command, metadata), shared_definitions
    )
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


def generate(files: list[UeFile], commands: list[dict[str, object]], ue_root: Path) -> str:
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
                f"#include {cpp_string(file.path.as_posix())}",
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
        fields = [
            f".path = {cpp_string(file.path.as_posix())}",
            f".logicalName = {cpp_string(file.logical_name)}",
            f".kind = UeFileKind::{file.kind}",
            f".ueConfProfile = UeConfProfile::{file.configuration}",
        ]
        if file.platform_group is not None:
            fields.append(f".platformGroup = UePlatformGroup::{file.platform_group}")
        if file.platform is not None:
            fields.append(f".platform = UePlatform::{file.platform}")
        fields.append(f".func = ue_generated_file_{index}::specify")
        lines.append(f"    {{{', '.join(fields)}}},")
    lines.extend(
        [
            "};",
            "",
            "// Bootstrap command row derived from this checkout's successful local UBT invocation:",
            "//     make UnrealServer-Linux-Debug",
            "// cppCompileCommand comes from UnrealServerMetadata.txt's BASE-COMMAND and maps to the shared",
            "// CppCompileEnvironment/toolchain prefix. HMake removes UBT's -c and semantic RTTI/exception",
            "// policy because CppSrc appends -c and each Configuration appends its own semantic policy,",
            "// dependency, source, and -o arguments per file. cCompileCommand selects C mode on that prefix.",
            "// Link fields come from UBT's adjacent response files; archiveCommand uses llvm-ar beside clang++.",
            "// HMake launches these commands directly while they fit the configured command-line",
            "// threshold; oversized commands use a generated response file in the owning target's build directory.",
            "//",
            "// Engine/Source is appended as the target-wide default include root. This maps to",
            "// UBT's default include-path setup and supports includes beginning with Runtime/,",
            "// Developer/, Editor/, and Programs/ without exposing an HMake-only module API.",
            "// HMakeSharedDefs.h supplies the empty target-wide non-attributed API macros used by",
            "// UHT-generated declarations in this monolithic target.",
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
            "// Expand the UE graph only when this configuration is active. Profile-marked modules create their",
            "// producer archives lazily as the consumer graph reaches them.",
            "void configurationSpecification(Configuration &config)",
            "{",
            "    auto &configuration = static_cast<UeConfiguration &>(config);",
            "    configuration.createProducerConfigurations();",
            "",
            "    for (const string &target : configuration.requestedTargets)",
            "    {",
            "        configuration.getOrAddTarget(target);",
            "    }",
            "",
            "    // configurationSpecification() is not invoked for dynamically created producers; finalize them here.",
            "    configuration.finalizeProducerConfigurations();",
            "}",
            "",
            "void buildSpecification()",
            "{",
            "    // Keep locally edited Git files standalone while unchanged files use adaptive jumbo compilation.",
            "    adaptiveBuildWorkingSetProvider = WorkingSetProvider::GIT;",
            "",
            "    // A full UnrealServer build can otherwise exhaust Linux memory.",
            "    projectCache.defaultJobs = 28;",
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
            f"            {cpp_string(ispc_compiler)}, true));",
            "    configuration.jumboFileSize = 384 * 1024;",
            '    configuration.requestTarget("UnrealServer");',
            "    // UBT's compile command already selects the bundled libc++ headers. Do not",
            "    // add HMake's host standard-library target (which would leak /usr/include).",
            "    configuration.assign(ConfigType::DEBUG, TargetType::LIBRARY_OBJECT, IsCppMod::NO, JumboBuild::YES,",
            "                         AssignStandardCppTarget::NO, RTTI::OFF, ExceptionHandling::OFF);",
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
    try:
        files = scan(ue_root)
        verify_ue_specifications(files, metadata, ue_root)
        shared_definitions_path = (ue_root / DEFAULT_SHARED_DEFINITIONS).resolve()
        shared_definitions = generate_shared_definitions(metadata, ue_root)
        commands = load_build_commands(metadata, ue_root, shared_definitions_path)
        generated = generate(files, commands, ue_root)
    except ValueError as error:
        parser.error(str(error))

    shared_definitions_changed = (
        not shared_definitions_path.exists()
        or shared_definitions_path.read_text(encoding="utf-8") != shared_definitions
    )
    if shared_definitions_changed:
        shared_definitions_path.write_text(shared_definitions, encoding="utf-8")

    output_changed = not output.exists() or output.read_text(encoding="utf-8") != generated
    if output_changed:
        output.write_text(generated, encoding="utf-8")

    disposition = "Generated" if output_changed or shared_definitions_changed else "Unchanged"
    print(f"{disposition}: {output} ({len(files)} files, {len(commands)} command rows)")


if __name__ == "__main__":
    main()
