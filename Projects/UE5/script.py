#!/usr/bin/env python3
"""
script.py
---------
This script automates the translation of Unreal Build Tool (UBT) target export dumps
(typically produced by 'make UnrealServer-Linux-Debug > output.txt' after custom UBT patches)
into configuration files compatible with HMake (a custom C++ build system).

It parses output.txt to extract:
1. The base compile command used by UBT.
2. The list of modules, their source files, public/private dependencies, compile definitions, and include directories.

It then:
1. Normalizes all paths (making them absolute and lexically normal) to ensure portability across different systems.
2. Performs a topological sort of the modules to respect build order.
3. Generates and writes the C++ configuration to 'hmake.cpp'.
"""

import os
import sys
import shlex
import argparse

# Boilerplate C++ template for generating the final hmake.cpp configuration.
# The template receives the resolved compile command and generated module targets.
HMAKE_TEMPLATE = """#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{{
    config.stdCppTarget->getSourceTarget().useReqIncls.clear();
    config.cppCompileCommand =
        R"({compile_command})";

{module_configurations}
}}

void buildSpecification()
{{
    getConfiguration("standard");
    CALL_CONFIGURATION_SPECIFICATION
}}

MAIN_FUNCTION
"""

def to_camel_case(s):
    """
    Converts a string (e.g. target module name) into camelCase.
    Handles multiple leading capital letters correctly (e.g., UElibPNG -> uElibPNG, UBT -> ubt).
    This is useful for generating clean, idiomatic C++ variable names from module names.
    """
    if not s:
        return ""
    uppers = 0
    for char in s:
        if char.isupper():
            uppers += 1
        else:
            break
    if uppers == 0:
        return s
    elif uppers == 1:
        return s[0].lower() + s[1:]
    else:
        if uppers == len(s):
            return s.lower()
        else:
            return s[:uppers-1].lower() + s[uppers-1] + s[uppers:]

def sanitize_var_name(name):
    """
    Creates a valid C++ variable name from a module name.
    1. Converts it to camelCase (e.g., "TraceLog" -> "traceLog").
    2. Replaces non-alphanumeric characters with underscores.
    3. Prepends an underscore if it starts with a number.
    Ensures the generated code compiles cleanly in C++.
    """
    camel = to_camel_case(name)
    sanitized = "".join(c if c.isalnum() or c == '_' else '_' for c in camel)
    if sanitized and sanitized[0].isdigit():
        sanitized = "_" + sanitized
    return sanitized

def escape_cpp_string(val):
    """
    Escapes quotes and backslashes in a string to make it safe for insertion
    into C++ raw or standard string literals (e.g., for macro definitions).
    """
    escaped = val.replace('\\', '\\\\').replace('"', '\\"')
    return f'"{escaped}"'

def topological_sort(modules):
    """
    Sorts modules and prunes cyclic dependency references.

    UBT already writes modules to output.txt in valid topological order (it runs
    its own topo-sort before printing).  Re-sorting with a pure Python DFS on a
    graph that mixes strong and weak edges introduces ordering anomalies — e.g.
    Core ends up at index 132 while Projects (which depends on Core) ends up at
    index 17, making the generated hmake.cpp uncompilable.

    Strategy
    --------
    1.  Trust UBT's order for all modules that were exported with a full detail block.
        Modules without a detail block (phantom stubs added to satisfy dep references)
        are appended at the end.
    2.  Build a position map so we can detect back-edges: a dependency edge A -> B is
        a *back-edge* (i.e. a cycle) when B appears *after* A in the sorted list.
    3.  Remove back-edges from public_deps / private_deps so the generated C++ code
        never references a variable declared later in the file.

    Weak edges (Public/Private-include-path-modules) are not added to the strong
    public_deps / private_deps lists, so they never appear as C++ .publicDeps() /
    .privateDeps() calls and cannot cause forward-reference errors.  They are only
    used for include-directory propagation which is handled separately.
    """
    # Separate modules that have a detail block (exported by UBT) from phantom stubs.
    # Phantom stubs are those we created synthetically with no sources/defs/incs.
    exported_names_ordered = []
    seen = set()
    for mod in modules:
        name = mod['name']
        # A module is "exported" if it was encountered in the Module: ... block.
        # Phantom stubs have all-empty lists except for 'name'.
        is_stub = (
            not mod['sources'] and not mod['public_defs'] and not mod['private_defs']
            and not mod['public_incs'] and not mod['private_incs']
            and not mod['internal_incs'] and not mod['system_incs']
            and not mod['public_deps'] and not mod['private_deps']
            and not mod.get('weak_public_deps') and not mod.get('weak_private_deps')
        )
        if not is_stub and name not in seen:
            exported_names_ordered.append(name)
            seen.add(name)

    # Append phantom stubs at the end (they have no compilable content anyway).
    mod_map = {m['name']: m for m in modules}
    for mod in modules:
        name = mod['name']
        if name not in seen:
            exported_names_ordered.append(name)
            seen.add(name)

    # Build position map: name -> index in the sorted list.
    pos = {name: i for i, name in enumerate(exported_names_ordered)}

    # Remove back-edges from each module's strong dependency lists.
    # A back-edge from module at position P to a dep at position Q > P would mean
    # the C++ variable for the dep is declared *after* the current module's variable,
    # which is a use-before-declaration error.
    cycles_broken = []
    for mod in modules:
        name = mod['name']
        p = pos.get(name, len(pos))

        for dep_list in ('public_deps', 'private_deps'):
            clean = []
            for dep in mod[dep_list]:
                q = pos.get(dep, len(pos))
                if q > p:
                    # Back-edge: dep comes after the current module in sorted order.
                    cycles_broken.append((name, dep))
                else:
                    clean.append(dep)
            mod[dep_list] = clean

        # Deduplicate after pruning (UBT sometimes lists the same dep twice because
        # the same module appears in both Public-dependency-modules and
        # Public-include-path-modules sections which we no longer merge).
        mod['public_deps']  = list(dict.fromkeys(mod['public_deps']))
        mod['private_deps'] = list(dict.fromkeys(mod['private_deps']))

    if cycles_broken:
        print(f"  Pruned {len(cycles_broken)} back-edge(s) to avoid C++ forward references.")

    return [mod_map[name] for name in exported_names_ordered if name in mod_map]

def parse_compile_command(lines):
    """
    Scans the UBT export file for the 'BASE-COMMAND:' marker and extracts
    the raw compiler path and default arguments printed on the following line.
    """
    for i, line in enumerate(lines):
        if "BASE-COMMAND:" in line:
            for j in range(i + 1, len(lines)):
                cmd = lines[j].strip()
                if cmd:
                    return cmd
    return ""

def parse_target_and_platform(lines):
    """
    Parses the target and platform from the UBT command line on the first line.
    Defaults to UnrealServer and Linux if not found.
    """
    if lines:
        first_line = lines[0].strip()
        parts = shlex.split(first_line)
        if len(parts) >= 3:
            for idx, part in enumerate(parts):
                if "RunUBT" in part or "UBT" in part or "Build" in part:
                    if idx + 2 < len(parts):
                        return parts[idx + 1], parts[idx + 2]
    return "UnrealServer", "Linux"

def find_original_ue_root(lines):
    """
    Identifies the absolute Unreal Engine root directory from the system that generated output.txt.
    It does this by searching for absolute paths containing "/Engine/Build/" or "/Engine/Source/"
    within the logs and extracting the prefix.
    """
    # 1. Check the very first line which typically invokes RunUBT.sh
    if lines:
        first_line = lines[0].strip()
        if "/Engine/Build/" in first_line:
            parts = first_line.split("/Engine/Build/")
            path = parts[0].lstrip('"')
            return os.path.normpath(path)
            
    # 2. Check each line for references to Engine/Source
    for line in lines:
        if "/Engine/Source/" in line:
            parts = line.split("/Engine/Source/")
            prefix = parts[0].strip()
            # Split to isolate the path prefix from surrounding tokens/quotes
            subparts = prefix.replace('"', ' ').replace("'", ' ').split()
            if subparts:
                return os.path.normpath(subparts[-1])
        if "/Engine/Intermediate/" in line:
            parts = line.split("/Engine/Intermediate/")
            prefix = parts[0].strip()
            subparts = prefix.replace('"', ' ').replace("'", ' ').split()
            if subparts:
                return os.path.normpath(subparts[-1])
                
    return None

def find_current_ue_root():
    """
    Dynamically resolves the absolute Unreal Engine root directory (ue_root) on the current system.
    This checks multiple options:
    1. An environment variable 'UNREAL_ENGINE_ROOT'
    2. The current working directory (if it contains Engine/Source)
    3. The directory where this script is located (or parent directories)
    4. Sibling directories (e.g. if run from a sibling 'HMake' folder)
    Using dynamic lookup ensures the script works on different systems without hardcoding.
    """
    # 1. Check environment variable
    if "UNREAL_ENGINE_ROOT" in os.environ:
        val = os.environ["UNREAL_ENGINE_ROOT"]
        if os.path.exists(os.path.join(val, "Engine", "Source")):
            return os.path.abspath(os.path.normpath(val))
            
    # 2. Check current working directory
    cwd = os.getcwd()
    if os.path.exists(os.path.join(cwd, "Engine", "Source")):
        return os.path.abspath(os.path.normpath(cwd))
        
    # 3. Check directory of the script file and traverse upwards to find the UE root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    temp = script_dir
    while temp and temp != os.path.dirname(temp):
        if os.path.exists(os.path.join(temp, "Engine", "Source")):
            return os.path.abspath(os.path.normpath(temp))
        temp = os.path.dirname(temp)
        
    # 4. Check adjacent sibling folder (e.g. in the standard setup, HMake and UnrealEngine are siblings)
    parent_sibling = os.path.abspath(os.path.join(cwd, "..", "UnrealEngine"))
    if os.path.exists(os.path.join(parent_sibling, "Engine", "Source")):
        return parent_sibling
        
    # Default fallback
    return "/home/hassan/Projects/UnrealEngine"

def make_path_portable(path, original_ue_root, current_ue_root):
    """
    Converts a path from output.txt to a path suitable for the current system.
    1. If the path is absolute and contains the original_ue_root prefix, it maps it
       to the current_ue_root.
    2. It normalizes the path lexically to resolve any redundant relative segments (..).
    3. It converts path separators to forward slashes for C++ string / Unix platform compatibility.
    """
    if not path:
        return ""
        
    # Standardize separator representation
    path = os.path.normpath(path)
    
    # If the path starts with the original Unreal Engine root, remap it to the current system's root
    if original_ue_root:
        orig_norm = os.path.normpath(original_ue_root)
        if path.startswith(orig_norm):
            path = path.replace(orig_norm, current_ue_root, 1)
            
    # Resolve to an absolute path and normalize lexically
    path = os.path.abspath(os.path.normpath(path))
    # Standardize on forward slashes to ensure C++ compatibility across different compiler backends
    path = path.replace('\\', '/')
    return path

def get_internal_sibling(path):
    """
    Checks if a given public or private include directory has a sibling 'Internal' directory.
    If it exists on disk, returns the absolute path to it. This handles cases where
    internal engine headers (e.g. RenderThreadTimeoutControl.h) are stored in sibling Internal folders.
    """
    path_norm = path.replace('\\', '/')
    if '/Public' in path_norm:
        head, sep, tail = path_norm.rpartition('/Public')
        candidate = head + '/Internal' + tail
        if os.path.isdir(candidate):
            return candidate
    if '/Private' in path_norm:
        head, sep, tail = path_norm.rpartition('/Private')
        candidate = head + '/Internal' + tail
        if os.path.isdir(candidate):
            return candidate
    return None

def generate_shared_defs(current_ue_root, raw_cmd, modules):
    """
    Generates HMakeSharedDefs.h dynamically to include:
    1. Empty monolithic API definitions for all parsed modules.
    2. All public compile defines from all modules (with #ifndef guards),
       matching UBT's SharedDefinitions.h / Definitions.ModuleName.h behavior.
       This is critical because modules that get include paths injected via
       weak_public_deps (Public-include-path-modules) also need the public
       defines of those modules (e.g. WITH_MGPU=0 from RHI) visible.
    3. Portably resolved system and VFS paths.
    """
    # 1. Gather all unique module names and compute their uppercase representation
    unique_names = sorted(list(set(mod['name'] for mod in modules)))
    
    # 2. Extract standard Linux SDK sysroot path from the compile command if possible
    sdk_path = ""
    if raw_cmd and "--sysroot=" in raw_cmd:
        parts = raw_cmd.split("--sysroot=")
        if len(parts) > 1:
            val = parts[1]
            if val.startswith('"'):
                raw_sysroot = val[1:].split('"')[0]
            else:
                raw_sysroot = val.split()[0]
            # Map relative sysroot prefix to the current UE root
            if raw_sysroot.startswith("../Extras/"):
                sdk_path = raw_sysroot.replace("../Extras/", f"{current_ue_root}/Engine/Extras/")
            elif raw_sysroot.startswith("../Extras"):
                sdk_path = raw_sysroot.replace("../Extras", f"{current_ue_root}/Engine/Extras")
            else:
                sdk_path = raw_sysroot
            sdk_path = os.path.abspath(os.path.normpath(sdk_path)).replace('\\', '/')
                
    # If not found, fall back to a standard location based on current_ue_root
    if not sdk_path:
        fallback_sdk = f"{current_ue_root}/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v26_clang-20.1.8-rockylinux8/x86_64-unknown-linux-gnu"
        sdk_path = os.path.abspath(os.path.normpath(fallback_sdk)).replace('\\', '/')

    # Standardize current UE root path representation for VFS
    ue_root_vfs = current_ue_root.replace('\\', '/')
    
    # Generate VFS paths macro (ending each real path with a semicolon)
    vfs_paths = f"/UEVFS/Root;{ue_root_vfs};/UEVFS/LinuxSDK;{sdk_path};"
    
    # Build macro definitions content
    macro_lines = []
    macro_lines.append("#pragma once")
    macro_lines.append("")
    macro_lines.append("// Global preprocessor flags for HMake Unreal Engine build")
    macro_lines.append("#define UE_IS_ENGINE_MODULE 1")
    macro_lines.append("#define UE_VALIDATE_FORMAT_STRINGS 1")
    macro_lines.append("#define UE_VALIDATE_INTERNAL_API 0")
    macro_lines.append("#define UE_VALIDATE_EXPERIMENTAL_API 0")
    macro_lines.append("#define UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4 0")
    macro_lines.append("#define UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5 0")
    macro_lines.append("#define UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6 0")
    macro_lines.append("#define UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_7 0")
    macro_lines.append("")

    # Collect all public defines from all modules and emit them with guards.
    # This matches UBT's SharedDefinitions behavior: when a module gets include
    # paths from a weak dep (Public-include-path-modules), UBT also propagates
    # that dep's public defines. We replicate this globally via HMakeSharedDefs.h.
    # Uses #ifndef guards so module-specific overrides are always respected.
    all_public_defs = {}  # macro_name -> value (last-seen wins; typically all are the same)
    for mod in modules:
        for df in mod.get('public_defs', []):
            df_stripped = df.strip()
            if '=' in df_stripped:
                name_part, val_part = df_stripped.split('=', 1)
                name_part = name_part.strip()
                val_part = val_part.strip()
            elif ' ' in df_stripped:
                parts = df_stripped.split(None, 1)
                name_part = parts[0].strip()
                val_part = parts[1].strip()
            else:
                name_part = df_stripped
                val_part = "1"
            if name_part:
                all_public_defs[name_part] = val_part

    if all_public_defs:
        macro_lines.append("// Public compile defines from all modules (guarded so module-specific overrides win)")
        for macro_name in sorted(all_public_defs.keys()):
            val = all_public_defs[macro_name]
            macro_lines.append(f"#ifndef {macro_name}")
            macro_lines.append(f"#define {macro_name} {val}")
            macro_lines.append(f"#endif")
        macro_lines.append("")

    macro_lines.append("// Empty API macros for monolithic linking of static modules")
    
    for name in unique_names:
        # Skip empty names or standard ones if any
        if not name:
            continue
        # Standardize module name to upper case
        upper_name = name.upper()
        # Some modules may have special characters, replace them with underscores
        upper_name = "".join(c if c.isalnum() else '_' for c in upper_name)
        macro_lines.append(f"#ifndef {upper_name}_API")
        macro_lines.append(f"#define {upper_name}_API ")
        macro_lines.append(f"#endif")
        macro_lines.append(f"#ifndef {upper_name}_NON_ATTRIBUTED_API")
        macro_lines.append(f"#define {upper_name}_NON_ATTRIBUTED_API ")
        macro_lines.append(f"#endif")
        
    macro_lines.append("")
    macro_lines.append("#define UE_WITH_DEBUG_INFO 1")
    macro_lines.append(f'#define UE_VFS_PATHS "{vfs_paths}"')
    macro_lines.append("")
    
    content = "\n".join(macro_lines)
    
    # Write to HMakeSharedDefs.h under current UE root
    target_path = os.path.join(current_ue_root, "Engine", "Source", "HMakeSharedDefs.h")
    with open(target_path, 'w', encoding='utf-8') as f:
        f.write(content)
        
    print(f"Successfully generated portable {target_path}")

def clean_and_extend_compile_cmd(base_cmd, original_ue_root, current_ue_root):
    """
    Cleans and expands compiler flags to make them portable and compatible with HMake:
    1. Remaps the original UE root path (such as in the compiler binary path) to the current UE root.
    2. Translates relative SDK path references (like '../Extras/...') into absolute paths
       using current_ue_root, so standard library headers (e.g. <cstdint>) resolve from HMake's build directory.
    3. Adds Engine/Source as an include path so relative includes (e.g. Runtime/AutoRTFM/...) resolve.
    4. Adds Core/Public and AutoRTFM/Public includes globally to allow low-level modules
       to resolve cross-module references (like HAL/Platform.h, AutoRTFM.h) without circular dependency loops.
    5. Force-includes the HMakeSharedDefs.h file containing monolithic API macro definitions.
    """
    if not base_cmd:
        return ""
        
    # Remap compiler executable and arguments from old UE root to new UE root
    if original_ue_root:
        base_cmd = base_cmd.replace(original_ue_root, current_ue_root)
        
    # Resolve relative SDK references inside BASE-COMMAND to absolute normalized paths
    base_cmd = base_cmd.replace('../Extras/', f'{current_ue_root}/Engine/Extras/')
    base_cmd = base_cmd.replace('../Extras"', f'{current_ue_root}/Engine/Extras"')
    
    # Define absolute include paths for Source, core subsystems, shaders, target platforms, and image core.
    # HMake runs compiler processes from its build directory (uebuild), so relative paths like
    # -I"." or -I"../" won't find headers in the Unreal Engine source tree. Adding absolute paths solves this.
    extra_inc = (
        f'-I"{current_ue_root}/Engine/Source" '
        f'-I"{current_ue_root}/Engine/Source/Runtime/AutoRTFM/Public" '
        f'-I"{current_ue_root}/Engine/Source/Runtime/ImageCore/Public" '
        f'-I"{current_ue_root}/Engine/Shaders/Public" '
        f'-I"{current_ue_root}/Engine/Shaders/Shared" '
        f'-I"{current_ue_root}/Engine/Source/ThirdParty/OpenGL"'
    )
    
    # Include HMakeSharedDefs.h to provide empty macro definitions for module export/import macros.
    # (e.g. CORE_API, BUILDSETTINGS_API) so the monolithic compilation of static libraries builds correctly.
    extra_hdr = f'-include"{current_ue_root}/Engine/Source/HMakeSharedDefs.h"'
    
    # Inject include paths and header include into compile command
    if extra_inc not in base_cmd:
        if '-I"."' in base_cmd:
            base_cmd = base_cmd.replace('-I"."', f'-I"." {extra_inc}')
        else:
            base_cmd += f" {extra_inc}"
            
    if extra_hdr not in base_cmd:
        base_cmd += f" {extra_hdr}"
        
    if " -Wno-missing-braces" not in base_cmd:
        base_cmd += " -Wno-missing-braces"

    # Ensure compile command always ends with a trailing space.
    # This prevents compiler flags dynamically appended by HMake (like module defines)
    # from concatenating directly onto the end of the last argument.
    if not base_cmd.endswith(" "):
        base_cmd += " "
        
    return base_cmd

def main():
    parser = argparse.ArgumentParser(description="Generate HMake target configurations from Unreal Build Tool export output.")
    parser.add_argument('--limit', '-l', type=int, help="Limit to first N modules in topological order")
    parser.add_argument('--ue-root', type=str, help="Manually override the current Unreal Engine root path")
    args = parser.parse_args()
    
    # Locate the output.txt file
    output_txt_path = "output.txt"
    if not os.path.exists(output_txt_path):
        # Also check relative to the script file directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        fallback_path = os.path.join(script_dir, "output.txt")
        if os.path.exists(fallback_path):
            output_txt_path = fallback_path
        else:
            print(f"Error: Could not find '{output_txt_path}' in the current directory or the script's directory.")
            sys.exit(1)
            
    print(f"Reading UBT export from: {os.path.abspath(output_txt_path)}")
    
    with open(output_txt_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        
    # Dynamically extract original UE root from output.txt
    original_ue_root = find_original_ue_root(lines)
    print(f"Detected Original UE Root in export file: {original_ue_root}")
    
    # Dynamically extract or manually override current UE root
    if args.ue_root:
        current_ue_root = os.path.abspath(os.path.normpath(args.ue_root))
    else:
        current_ue_root = find_current_ue_root()
    print(f"Detected Current UE Root: {current_ue_root}")
    
    # Dynamically extract Target and Platform
    target, platform = parse_target_and_platform(lines)
    print(f"Detected Build Target: {target}, Platform: {platform}")
    
    # Clean and extend base compilation command
    raw_cmd = parse_compile_command(lines)
    compile_cmd = clean_and_extend_compile_cmd(raw_cmd, original_ue_root, current_ue_root)
    
    if compile_cmd:
        print("Successfully parsed and updated compile command with absolute directories.")
    else:
        print("Warning: Compile command not found in output.txt. Using a placeholder.")
        compile_cmd = "clang++ -std=c++20 "
        
    # State-machine parsing of the UBT export dump
    # Matches Module, Dependencies, Source files, Includes, and Definitions
    modules = []
    current_module = None
    state = None
    
    for line in lines:
        line = line.rstrip('\r\n')
        if not line:
            continue
        # Headers/Footers of UBT export sections
        if line.startswith("====") or line.startswith("Target is") or line.startswith("Result:") or line.startswith("Total execution"):
            if current_module:
                modules.append(current_module)
            state = None
            current_module = None
            continue
        # Start of a new module section
        if line.startswith("Module:"):
            if current_module:
                modules.append(current_module)
            current_module = {
                'name': line.split("Module:")[1].strip(),
                'public_deps': [],
                'private_deps': [],
                # Weak (include-path-only) deps are stored separately so the
                # weighted cycle-breaker can prefer severing them over strong deps.
                'weak_public_deps': [],
                'weak_private_deps': [],
                'circular_deps': [],
                'sources': [],
                'public_defs': [],
                'private_defs': [],
                'public_incs': [],
                'private_incs': [],
                'internal_incs': [],
                'system_incs': [],
                'uht_incs': [],
                'vni_incs': []
            }
            state = None
        # Track transitions between lists of assets/properties.
        # Strong compile/link dependencies:
        elif "Public-dependency-modules" in line:
            state = "PUBLIC_DEPS"
        elif "Private-dependency-modules" in line:
            state = "PRIVATE_DEPS"
        # Weak include-path-only dependencies (no compiled library is linked):
        elif "Public-include-path-modules" in line:
            state = "WEAK_PUBLIC_DEPS"
        elif "Private-include-path-modules" in line:
            state = "WEAK_PRIVATE_DEPS"
        elif "Circular-dependency-modules" in line:
            state = "CIRCULAR_DEPS"
        elif "1) Jumbo-build source-files" in line:
            state = "SOURCES"
        elif "2) Public-definitions" in line:
            state = "PUBLIC_DEFS"
        elif "3) Private-definitions" in line:
            state = "PRIVATE_DEFS"
        elif "4) Public-include-dirs" in line:
            state = "PUBLIC_INCS"
        elif "5) Private-include-dirs" in line:
            state = "PRIVATE_INCS"
        elif "6) Internal-include-dirs" in line:
            state = "INTERNAL_INCS"
        elif "7) System-include-dirs" in line:
            state = "SYSTEM_INCS"
        elif "8) UHT-include-dirs" in line:
            state = "UHT_INCS"
        elif "9) VNI-include-dirs" in line:
            state = "VNI_INCS"
        # Collect lines under the active list state
        elif current_module is not None:
            item = line.strip()
            if item:
                if state == "PUBLIC_DEPS":
                    current_module['public_deps'].append(item)
                elif state == "PRIVATE_DEPS":
                    current_module['private_deps'].append(item)
                elif state == "WEAK_PUBLIC_DEPS":
                    current_module['weak_public_deps'].append(item)
                elif state == "WEAK_PRIVATE_DEPS":
                    current_module['weak_private_deps'].append(item)
                elif state == "CIRCULAR_DEPS":
                    current_module['circular_deps'].append(item)
                    current_module['weak_private_deps'].append(item)
                elif state == "SOURCES":
                    current_module['sources'].append(item)
                elif state == "PUBLIC_DEFS":
                    current_module['public_defs'].append(item)
                elif state == "PRIVATE_DEFS":
                    current_module['private_defs'].append(item)
                elif state == "PUBLIC_INCS":
                    current_module['public_incs'].append(item)
                elif state == "PRIVATE_INCS":
                    current_module['private_incs'].append(item)
                elif state == "INTERNAL_INCS":
                    current_module['internal_incs'].append(item)
                elif state == "SYSTEM_INCS":
                    current_module['system_incs'].append(item)
                elif state == "UHT_INCS":
                    current_module['uht_incs'].append(item)
                elif state == "VNI_INCS":
                    current_module['vni_incs'].append(item)
                    
    if current_module:
        modules.append(current_module)
        
    print(f"Parsed {len(modules)} modules.")

    # Identify and add any missing dependencies as empty modules to prevent sorting errors.
    # This happens when a dependency is listed in deps but has no detail block in output.txt.
    # We scan both strong and weak dep lists.
    all_module_names = set(mod['name'] for mod in modules)
    missing_deps = set()
    for mod in modules:
        all_mod_deps = (
            mod['public_deps'] + mod['private_deps'] +
            mod.get('weak_public_deps', []) + mod.get('weak_private_deps', [])
        )
        for dep in all_mod_deps:
            if dep not in all_module_names:
                missing_deps.add(dep)

    for dep in missing_deps:
        modules.append({
            'name': dep,
            'public_deps': [],
            'private_deps': [],
            'weak_public_deps': [],
            'weak_private_deps': [],
            'circular_deps': [],
            'sources': [],
            'public_defs': [],
            'private_defs': [],
            'public_incs': [],
            'private_incs': [],
            'internal_incs': [],
            'system_incs': []
        })
        
    # Dynamically generate HMakeSharedDefs.h with all empty API macros and resolved VFS paths
    generate_shared_defs(current_ue_root, raw_cmd, modules)
        
    # Sort modules topologically based on their dependencies so dependencies are declared first
    sorted_modules = topological_sort(modules)
    
    # Restrict to a subset limit if specified by command line.
    #
    # IMPORTANT: We do NOT simply take the first N entries of sorted_modules.
    # Many early sorted entries are header-only third-party libs (BLAKE3, ICU, etc.)
    # with no source files.  A naive slice would exhaust the limit before reaching
    # foundational modules like Core.
    #
    # Instead we:
    #   1. Walk sorted_modules in topological order and collect the first N modules
    #      that have actual source files (i.e. modules that need to be compiled).
    #   2. Then expand the selected set with ALL transitive strong dependencies of
    #      those N modules (so that every dep is declared before its dependents).
    #   3. Preserve the original topological order for the final list.


    # Define globally configured include paths to ignore during config generation
    # since they are already passed to the compiler globally via config.cppCompileCommand.
    global_incs_to_ignore = {
        make_path_portable(f"{current_ue_root}/Engine/Source", original_ue_root, current_ue_root),
        make_path_portable(f"{current_ue_root}/Engine/Source/Runtime/AutoRTFM/Public", original_ue_root, current_ue_root),
        make_path_portable(f"{current_ue_root}/Engine/Source/Runtime/ImageCore/Public", original_ue_root, current_ue_root),
        make_path_portable(f"{current_ue_root}/Engine/Shaders/Public", original_ue_root, current_ue_root),
        make_path_portable(f"{current_ue_root}/Engine/Shaders/Shared", original_ue_root, current_ue_root),
        make_path_portable(f"{current_ue_root}/Engine/Source/ThirdParty/OpenGL", original_ue_root, current_ue_root)
    }

    # Detect public/system/internal include paths shared by multiple modules and synthesize a common header-only target for them.
    # This prevents configure-time duplicate include errors when a target depends on multiple modules that
    # export the same path, resolving parallel include duplication.
    from collections import defaultdict
    path_to_mods = defaultdict(list) # portable_path -> list of (module_dict, type_of_inc, original_inc)
    
    for mod in sorted_modules:
        for inc_type in ['public_incs', 'internal_incs', 'system_incs']:
            for inc in mod.get(inc_type, []):
                portable_inc = make_path_portable(inc, original_ue_root, current_ue_root)
                if portable_inc not in global_incs_to_ignore:
                    path_to_mods[portable_inc].append((mod, inc_type, inc))
                    
    synthesized_targets = []
    generated_names = set(mod['name'] for mod in sorted_modules)
    for portable_path, sharing_info in sorted(path_to_mods.items(), key=lambda x: x[0]):
        if len(sharing_info) > 1:
            # First sharing module determines the base name for the synthesized target
            base_mod = sharing_info[0][0]
            base_target_name = f"{base_mod['name'].lower()}_headers"
            new_target_name = base_target_name
            counter = 1
            while new_target_name in generated_names:
                new_target_name = f"{base_target_name}_{counter}"
                counter += 1
            generated_names.add(new_target_name)
            
            # Create the new target
            new_target = {
                'name': new_target_name,
                'public_deps': [],
                'private_deps': [],
                'weak_public_deps': [],
                'weak_private_deps': [],
                'circular_deps': [],
                'sources': [],
                'public_defs': [],
                'private_defs': [],
                'public_incs': [],
                'private_incs': [],
                'internal_incs': [],
                'system_incs': []
            }
            # Put the path in the correct include list based on the first sharing info
            first_type = sharing_info[0][1]
            first_inc = sharing_info[0][2]
            new_target[first_type].append(first_inc)
            synthesized_targets.append((new_target, base_mod['name']))
            
            # For all modules sharing this path, remove the path and add a dependency on the new target
            for mod, inc_type, inc in sharing_info:
                if inc in mod.get(inc_type, []):
                    mod[inc_type].remove(inc)
                if new_target_name not in mod['public_deps']:
                    mod['public_deps'].append(new_target_name)
                    
    # Insert synthesized targets into sorted_modules and modules topologically
    if synthesized_targets:
        new_sorted_modules = []
        base_to_new_targets = defaultdict(list)
        for new_target, base_name in synthesized_targets:
            base_to_new_targets[base_name].append(new_target)
            modules.append(new_target)
            
        for mod in sorted_modules:
            if mod['name'] in base_to_new_targets:
                new_sorted_modules.extend(base_to_new_targets[mod['name']])
            new_sorted_modules.append(mod)
        sorted_modules = new_sorted_modules

    # Build the final module name->module dict (used for weak-dep include injection below).
    mod_map_all = {m['name']: m for m in modules}
    mod_map_final = {m['name']: m for m in sorted_modules}

    # Pre-compute transitive weak include directories for modules.
    _propagated_public_incs_cache = {}

    def _get_all_propagated_public_incs(name, visited=None):
        if visited is None:
            visited = set()
        if name in _propagated_public_incs_cache:
            return _propagated_public_incs_cache[name]
        if name in visited:
            return set()
        visited.add(name)
        
        # If this module is a compiled target (or phantom stub target) whose includes
        # have already been computed, we use its actual computed transitive public includes!
        # This respects private downgrades (like Core's private include of ImageCore).
        if name in transitive_public_incs:
            result = set((path, name) for path in transitive_public_incs[name])
            _propagated_public_incs_cache[name] = result
            return result

        mod = mod_map_all.get(name)
        if mod is None:
            return set()
            
        result = set((inc, name) for inc in mod.get('public_incs', []))
        for inc in mod.get('internal_incs', []):
            result.add((inc, name))
        for inc in mod.get('system_incs', []):
            result.add((inc, name))
        
        # Collect UHT/VNI dirs from UBT metadata
        uht_dirs_to_try = set()
        for raw_uht in mod.get('uht_incs', []) + mod.get('vni_incs', []):
            portable_uht = make_path_portable(raw_uht, original_ue_root, current_ue_root)
            uht_dirs_to_try.add(portable_uht)

        # Fallback to name-based check if metadata didn't have any UHT/VNI dirs
        if not uht_dirs_to_try:
            # Old heuristic fallback
            uht_dir_names_to_try = [name]
            if mod.get('sources'):
                first_src = mod['sources'][0]
                needle = f"/Build/{platform}/x64/{target}/{config_name}/"
                if needle in first_src:
                    try:
                        after_config = first_src.split(needle)[1]
                        abbrev = after_config.split('/')[0]
                        if abbrev and abbrev != name:
                            uht_dir_names_to_try.append(abbrev)
                    except Exception:
                        pass
            for dname in uht_dir_names_to_try:
                uht_dirs_to_try.add(f"{current_ue_root}/Engine/Intermediate/Build/{platform}/{target}/Inc/{dname}/UHT")
                uht_dirs_to_try.add(f"{current_ue_root}/Engine/Intermediate/Build/{platform}/{target}/Inc/{dname}")

        for uht_dir in sorted(list(uht_dirs_to_try)):
            if os.path.isdir(uht_dir):
                result.add((uht_dir, name))
            
        # For non-compiled targets, we traverse public/weak deps transitively
        public_dep_names = mod.get('public_deps', []) + mod.get('weak_public_deps', [])
        for dep_name in public_dep_names:
            result |= _get_all_propagated_public_incs(dep_name, visited)
            
        _propagated_public_incs_cache[name] = result
        return result

    # Pre-compute transitive strong dependencies to detect back-propagation cycles in weak deps.
    _transitive_strong_deps_cache = {}

    def _get_transitive_strong_deps(name):
        if name in _transitive_strong_deps_cache:
            return _transitive_strong_deps_cache[name]
        mod = mod_map_final.get(name)
        if mod is None:
            _transitive_strong_deps_cache[name] = set()
            return set()
        result = set()
        for dep in mod['public_deps'] + mod['private_deps']:
            result.add(dep)
            result |= _get_transitive_strong_deps(dep)
        _transitive_strong_deps_cache[name] = result
        return result

    # Promote weak public dependencies to public dependencies if they are already transitive strong dependencies,
    # to avoid duplicate include paths on the target while ensuring they propagate downstream.
    for mod in sorted_modules:
        transitive_strong = _get_transitive_strong_deps(mod['name'])
        weak_public = mod.get('weak_public_deps', [])
        to_promote = [dep for dep in weak_public if dep in transitive_strong]
        for dep in to_promote:
            if dep in mod['private_deps']:
                mod['private_deps'].remove(dep)
            if dep not in mod['public_deps']:
                mod['public_deps'].append(dep)
    _transitive_strong_deps_cache.clear()


    # Pre-compute target includes using topological ordering.
    # This precisely models HMake's include propagation behavior:
    # - A target's transitive public includes are exposed to its dependents.
    # - If an include directory is already transitively propagated to a target via its
    #   strong dependencies, we do not add it explicitly again, avoiding duplicate include errors.
    transitive_public_incs = {} # name -> set of portable paths
    own_public_hu_includes = {} # name -> set of portable paths
    own_private_hu_includes = {} # name -> set of portable paths

    sorted_names = set(m['name'] for m in sorted_modules)

    # Detect target build configuration (e.g. Debug, Development) from source file paths
    config_name = "Debug"  # default fallback
    for m in sorted_modules:
        for src in m['sources']:
            needle = f"/Build/{platform}/x64/{target}/"
            if needle in src:
                try:
                    parts = src.split(needle)[1].split('/')
                    if parts and parts[0]:
                        config_name = parts[0]
                        break
                except Exception:
                    pass
        if config_name != "Debug":
            break
    print(f"Detected Build Configuration for ISPC intermediate paths: {config_name}")

    for mod in sorted_modules:
        name = mod['name']
               # 1. Compute strong_dep_incs: includes transitively propagated by public/private dependencies
        strong_public_dep_incs = set()
        for dep_name in mod['public_deps']:
            if dep_name in sorted_names and dep_name in transitive_public_incs:
                strong_public_dep_incs |= transitive_public_incs[dep_name]

        strong_private_dep_incs = set()
        for dep_name in mod['private_deps']:
            if dep_name in sorted_names and dep_name in transitive_public_incs:
                strong_private_dep_incs |= transitive_public_incs[dep_name]

        strong_dep_incs = strong_public_dep_incs | strong_private_dep_incs
                
        # 2. Compute what we will add to own_public_hu_includes and own_private_hu_includes
        public_hu = set()
        private_hu = set()
        seen_local = set()
        
        # UHT/VNI directories from UBT metadata export
        # If UBT exports these paths directly, we use them. We also keep a fallback to the old
        # heuristic logic (using mod['name'] and abbreviated name from sources) in case they
        # are not exported or we are running on old exports.
        uht_dirs_to_try = set()
        for raw_uht in mod.get('uht_incs', []) + mod.get('vni_incs', []):
            portable_uht = make_path_portable(raw_uht, original_ue_root, current_ue_root)
            uht_dirs_to_try.add(portable_uht)

        if not uht_dirs_to_try:
            # Fallback heuristic
            uht_dir_names_to_try = [mod['name']]
            if mod['sources']:
                first_src = mod['sources'][0]
                needle = f"/Build/{platform}/x64/{target}/{config_name}/"
                if needle in first_src:
                    try:
                        after_config = first_src.split(needle)[1]
                        abbrev = after_config.split('/')[0]
                        if abbrev and abbrev != mod['name']:
                            uht_dir_names_to_try.append(abbrev)
                    except Exception:
                        pass
            for dir_name in uht_dir_names_to_try:
                uht_dir1 = f"{current_ue_root}/Engine/Intermediate/Build/{platform}/{target}/Inc/{dir_name}/UHT"
                uht_dir2 = f"{current_ue_root}/Engine/Intermediate/Build/{platform}/{target}/Inc/{dir_name}"
                uht_dirs_to_try.add(uht_dir1)
                uht_dirs_to_try.add(uht_dir2)

        for uht_dir in sorted(list(uht_dirs_to_try)):
            if os.path.isdir(uht_dir):
                portable_uht = make_path_portable(uht_dir, original_ue_root, current_ue_root)
                if portable_uht not in global_incs_to_ignore and portable_uht not in seen_local:
                    public_hu.add(portable_uht)
                    seen_local.add(portable_uht)

                
        # public_incs
        for inc in mod['public_incs']:
            portable_inc = make_path_portable(inc, original_ue_root, current_ue_root)
            if portable_inc not in global_incs_to_ignore and portable_inc not in seen_local:
                public_hu.add(portable_inc)
                seen_local.add(portable_inc)
            internal_sibling = get_internal_sibling(portable_inc)
            if internal_sibling and internal_sibling not in global_incs_to_ignore and internal_sibling not in seen_local:
                public_hu.add(internal_sibling)
                seen_local.add(internal_sibling)
                
        # private_incs
        for inc in mod['private_incs']:
            portable_inc = make_path_portable(inc, original_ue_root, current_ue_root)
            if portable_inc not in global_incs_to_ignore and portable_inc not in seen_local:
                if name == "Core":
                    public_hu.add(portable_inc)
                else:
                    private_hu.add(portable_inc)
                seen_local.add(portable_inc)
            internal_sibling = get_internal_sibling(portable_inc)
            if internal_sibling and internal_sibling not in global_incs_to_ignore and internal_sibling not in seen_local:
                # Sibling internal dir of private inc is added as publicHUIncludes
                public_hu.add(internal_sibling)
                seen_local.add(internal_sibling)
                
        # Check if this module has ISPC files
        has_ispc = any(src.endswith('.ispc') or src.endswith('.ispc.o') for src in mod['sources'])
        if has_ispc:
            # Find the correct intermediate folder dynamically from source file path
            ispc_intermediate_dir = None
            if mod['sources']:
                first_src = mod['sources'][0]
                needle = f"/Build/{platform}/x64/{target}/{config_name}/"
                if needle in first_src:
                    base_part = first_src.split(needle)[0]
                    ispc_intermediate_dir = f"{base_part}/Build/{platform}/x64/{target}/{config_name}/{name}"
            
            # Fallback if no source files or needle not match
            if not ispc_intermediate_dir:
                ispc_intermediate_dir = f"{current_ue_root}/Engine/Intermediate/Build/{platform}/x64/{target}/{config_name}/{name}"

            portable_ispc_inc = make_path_portable(ispc_intermediate_dir, original_ue_root, current_ue_root)
            if portable_ispc_inc not in global_incs_to_ignore and portable_ispc_inc not in seen_local:
                os.makedirs(ispc_intermediate_dir, exist_ok=True)
                private_hu.add(portable_ispc_inc)
                seen_local.add(portable_ispc_inc)

        # weak_public_deps (Public-include-path-modules in UBT)
        # UBT injects these includes into the PUBLIC compile command unconditionally —
        # no back-edge or cycle check is performed. We must match that behavior.
        # Previously we had an is_back_edge check here that incorrectly made RHI/Public
        # private on ApplicationCore (because RHI private-depends on ApplicationCore),
        # which prevented RHI/Public from propagating to TypedElementFramework.
        for weak_dep_name in mod.get('weak_public_deps', []):
            propagated = _get_all_propagated_public_incs(weak_dep_name)
            for inc, owner in propagated:
                portable_inc = make_path_portable(inc, original_ue_root, current_ue_root)
                if portable_inc not in global_incs_to_ignore and portable_inc not in strong_public_dep_incs and portable_inc not in seen_local:
                    public_hu.add(portable_inc)
                    seen_local.add(portable_inc)
                    
        # weak_private_deps
        for weak_dep_name in mod.get('weak_private_deps', []):
            propagated = _get_all_propagated_public_incs(weak_dep_name)
            for inc, owner in propagated:
                portable_inc = make_path_portable(inc, original_ue_root, current_ue_root)
                if portable_inc not in global_incs_to_ignore and portable_inc not in strong_dep_incs and portable_inc not in seen_local:
                    private_hu.add(portable_inc)
                    seen_local.add(portable_inc)
                    
        # internal_incs
        for inc in mod.get('internal_incs', []):
            portable_inc = make_path_portable(inc, original_ue_root, current_ue_root)
            if portable_inc not in global_incs_to_ignore and portable_inc not in seen_local:
                public_hu.add(portable_inc)
                seen_local.add(portable_inc)
                
        # system_incs
        for inc in mod.get('system_incs', []):
            portable_inc = make_path_portable(inc, original_ue_root, current_ue_root)
            if portable_inc not in global_incs_to_ignore and portable_inc not in seen_local:
                public_hu.add(portable_inc)
                seen_local.add(portable_inc)
                
        # 3. Store results for the current module
        own_public_hu_includes[name] = public_hu
        own_private_hu_includes[name] = private_hu
        
        # 4. Compute transitive_public_incs[name] (it will be passed to modules depending on this module)
        transitive_pub = set(public_hu)
        for dep_name in mod['public_deps']:
            if dep_name in sorted_names and dep_name in transitive_public_incs:
                transitive_pub |= transitive_public_incs[dep_name]
        transitive_public_incs[name] = transitive_pub
        _propagated_public_incs_cache[name] = set((path, name) for path in transitive_pub)

    # --- SECOND DEDUPLICATION PASS (on computed own_public_hu_includes and own_private_hu_includes) ---
    # Detect include paths (both public and private) shared by multiple modules and synthesize a common header-only target for them.
    # This prevents configure-time duplicate include errors when multiple targets own or depend on the same path.
    from collections import defaultdict
    path_to_sharing_info = defaultdict(list) # path -> list of (module_dict, 'public' or 'private')
    for mod in sorted_modules:
        name = mod['name']
        for path in sorted(list(own_public_hu_includes[name])):
            path_to_sharing_info[path].append((mod, 'public'))
        for path in sorted(list(own_private_hu_includes[name])):
            path_to_sharing_info[path].append((mod, 'private'))

    synthesized_uht_targets = []
    generated_names = set(m['name'] for m in modules)
    
    def get_automated_target_name(portable_path):
        parts = [p for p in portable_path.split('/') if p]
        if not parts:
            return "shared_headers"
        last = parts[-1]
        if len(parts) >= 2 and last.lower() in ('uht', 'public', 'private', 'internal', 'inc', 'include', 'source', 'src', 'lib', 'headers'):
            parent = parts[-2]
            name = f"{parent}_{last}"
        else:
            name = last
        sanitized = "".join(c if c.isalnum() or c == '_' else '_' for c in name.lower())
        base_name = f"{sanitized}_shared"
        candidate = base_name
        counter = 1
        while candidate in generated_names:
            candidate = f"{base_name}_{counter}"
            counter += 1
        generated_names.add(candidate)
        return candidate

    for path, sharing_info in sorted(path_to_sharing_info.items(), key=lambda x: x[0]):
        if len(sharing_info) > 1:
            # Create a synthesized target for this shared include path
            new_target_name = get_automated_target_name(path)
            new_target = {
                'name': new_target_name,
                'public_deps': [],
                'private_deps': [],
                'weak_public_deps': [],
                'weak_private_deps': [],
                'circular_deps': [],
                'sources': [],
                'public_defs': [],
                'private_defs': [],
                'public_incs': [],
                'private_incs': [],
                'internal_incs': [],
                'system_incs': []
            }
            
            # Add synthesized target to lists
            modules.append(new_target)
            synthesized_uht_targets.append(new_target)
            
            # Initialize its public includes with the shared path
            # (the phone target itself always exposes the include path publicly)
            own_public_hu_includes[new_target_name] = {path}
            own_private_hu_includes[new_target_name] = set()
            
            # Remove the path from all sharing modules and add public/private dependency on the new target
            for mod, dep_type in sharing_info:
                name = mod['name']
                if dep_type == 'public':
                    if path in own_public_hu_includes[name]:
                        own_public_hu_includes[name].remove(path)
                    if new_target_name not in mod['public_deps']:
                        mod['public_deps'].append(new_target_name)
                elif dep_type == 'private':
                    if path in own_private_hu_includes[name]:
                        own_private_hu_includes[name].remove(path)
                    if new_target_name not in mod['private_deps']:
                        mod['private_deps'].append(new_target_name)

    # Prepend synthesized targets to sorted_modules so they are declared first
    if synthesized_uht_targets:
        sorted_modules = synthesized_uht_targets + sorted_modules
        print(f"Synthesized {len(synthesized_uht_targets)} header-only targets to deduplicate shared includes.")

    # Generate the C++ target DSC configuration block
    out = []
    out.append("    // Module configurations")
    sorted_names = set(m['name'] for m in sorted_modules)
    module_files_written_count = 0
    for mod in sorted_modules:
        var_name = sanitize_var_name(mod['name'])
        out.append(f'    DSC<CppTarget> &{var_name} = config.getCppStaticDSC("{mod["name"]}");')
        
        # Format public/private dependencies
        public_deps = [dep for dep in mod['public_deps'] if dep in sorted_names]
        private_deps = [dep for dep in mod['private_deps'] if dep in sorted_names]
        
        # Sort dependencies so that the module's own synthesized targets (e.g. core_private_shared)
        # come first, followed by other synthesized targets (header-only), and compiled targets come last.
        def dep_sort_key(dep):
            dep_lower = dep.lower()
            mod_lower = mod['name'].lower()
            if dep_lower.startswith(mod_lower + "_"):
                return (0, dep_lower)
            elif dep_lower.endswith("_shared") or dep_lower.endswith("_headers"):
                return (1, dep_lower)
            else:
                return (2, dep_lower)
        
        public_deps.sort(key=dep_sort_key)
        private_deps.sort(key=dep_sort_key)
        
        if public_deps or private_deps:
            out.append(f"    {var_name}")
            for dep in public_deps:
                dep_var = sanitize_var_name(dep)
                out.append(f"        .publicDeps({dep_var})")
            for dep in private_deps:
                dep_var = sanitize_var_name(dep)
                out.append(f"        .privateDeps({dep_var})")
            out[-1] += ";"
            
        # Format Sources, Includes, Definitions, and headers
        has_config = (
            mod['sources'] or 
            mod['public_defs'] or 
            mod['private_defs'] or 
            own_public_hu_includes[mod['name']] or
            own_private_hu_includes[mod['name']]
        )
        
        if has_config:
            out.append(f"    {var_name}.getSourceTarget()")
            
            # Check if limit is applied and whether we should write source files for this module
            should_write_sources = True
            if args.limit is not None:
                has_cpp_sources = any(not (src.endswith('.ispc') or src.endswith('.ispc.o')) for src in mod['sources'])
                if has_cpp_sources:
                    if module_files_written_count >= args.limit:
                        should_write_sources = False
                    else:
                        module_files_written_count += 1
            
            if should_write_sources:
                for src in mod['sources']:
                    if src.endswith('.ispc') or src.endswith('.ispc.o'):
                        # TEMPORARY CHANGE: Skip ISPC files for now since they cannot be integrated directly as C++ source or pre-built library in HMake
                        continue
                    portable_src = make_path_portable(src, original_ue_root, current_ue_root)
                    out.append(f'        .moduleFiles("{portable_src}")')
                
            for df in mod['public_defs']:
                if '=' in df:
                    name, val = df.split('=', 1)
                    name = name.strip()
                    val = val.strip()
                else:
                    name, val = df.strip(), "1"
                val_str = escape_cpp_string(val)
                out.append(f'        .publicCompileDefines("{name}", {val_str})')
                
            for df in mod['private_defs']:
                if '=' in df:
                    name, val = df.split('=', 1)
                    name = name.strip()
                    val = val.strip()
                else:
                    name, val = df.strip(), "1"
                val_str = escape_cpp_string(val)
                out.append(f'        .privateCompileDefines("{name}", {val_str})')
                
            for path in sorted(own_public_hu_includes[mod['name']]):
                out.append(f'        .publicHUIncludes("{path}")')
                
            for path in sorted(own_private_hu_includes[mod['name']]):
                out.append(f'        .privateHUIncludes("{path}")')
                
            out[-1] += ";"
        out.append("")
        
    modules_code = "\n".join(out)
    
    # Format C++ template and write directly to ./hmake.cpp
    hmake_content = HMAKE_TEMPLATE.format(
        compile_command=compile_cmd,
        module_configurations=modules_code
    )
    
    dest_path = "hmake.cpp"
    with open(dest_path, 'w', encoding='utf-8') as f:
        f.write(hmake_content)
    print(f"Successfully generated {os.path.abspath(dest_path)}")

    # Automatically patch ConsoleManagerTest.cpp to include the private ConsoleManager.h if it exists
    test_file_path = os.path.join(current_ue_root, "Engine", "Source", "Runtime", "Core", "Tests", "HAL", "ConsoleManagerTest.cpp")
    if os.path.exists(test_file_path):
        try:
            with open(test_file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            target_inc = '#include "HAL/ConsoleManager.h"'
            replacement_inc = '#include "../../Private/HAL/ConsoleManager.h"'
            if target_inc in content:
                content = content.replace(target_inc, replacement_inc)
                with open(test_file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                print(f"Automatically patched {test_file_path} to use correct relative private header.")
        except Exception as e:
            print(f"Warning: Failed to automatically patch {test_file_path}: {e}")

    # Automatically rename Core's Public/HAL/ConsoleManager.h to avoid conflict with the Private one
    public_header = os.path.join(current_ue_root, "Engine", "Source", "Runtime", "Core", "Public", "HAL", "ConsoleManager.h")
    if os.path.exists(public_header):
        try:
            backup_header = public_header + ".disabled"
            if os.path.exists(backup_header):
                os.remove(backup_header)
            os.rename(public_header, backup_header)
            print(f"Automatically disabled public forwarding header: {public_header}")
        except Exception as e:
            print(f"Warning: Failed to disable public forwarding header: {e}")

if __name__ == '__main__':
    main()
