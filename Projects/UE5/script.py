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

Deviations from UBT (Unreal Build Tool):
--------------------------------------
1. Exception/RTTI Targets:
   Targets using exception handling or RTTI (such as 'OpenExrWrapper') are omitted from active C++ compilation (configured as header-only / skipped) to prevent compile errors, because the standard compiler command disables exception handling and RTTI.
2. ISPC Compilation:
   Intel ISPC (Implicit SPMD Program Compiler) source files (.ispc) are not configured for compilation.
3. MiMalloc:
   'MiMalloc.c' is excluded from active compilation in the 'Core' target to prevent compilation warnings/errors treated as hard errors when the C-file is processed under C++ compilation rules.
"""

import os
import sys
import shlex
import argparse
import collections
import json

HMAKE_TEMPLATE = """#include "Configure.hpp"

void addUeDirectory(DSC<CppTarget> &target, const string &directory, const string &prefix,
                    const string &huRegex, const string &incRegex, const bool isPublic)
{{
    CppTarget &ueCpp = target.getSourceTarget();

    string parentDir = directory;
    string cleanPrefix = prefix;
    if (cleanPrefix.ends_with("/"))
    {{
        cleanPrefix.pop_back();
    }}
    if (!cleanPrefix.empty() && parentDir.ends_with(cleanPrefix))
    {{
        parentDir = parentDir.substr(0, parentDir.size() - cleanPrefix.size());
        if (parentDir.ends_with("/"))
        {{
            parentDir.pop_back();
        }}
    }}

    Node *parentDirNode = NodeOrStr(parentDir).resolve(false);
    if (isPublic)
    {{
        if (!huRegex.empty()) ueCpp.publicHUDirsRE(directory, prefix, huRegex);
        if (!incRegex.empty()) ueCpp.publicIncDirsRE(directory, prefix, incRegex);
        ueCpp.publicIncDirsRE(directory, prefix, ".*\\\\.inc")
            .publicIncDirsRE(directory, prefix, ".*\\\\.def")
            .publicIncDirsRE(directory, prefix, ".*\\\\.inl");
        
        bool alreadyAdded = false;
        for (const auto &inc : ueCpp.reqIncls)
        {{
            if (inc.node == parentDirNode)
            {{
                alreadyAdded = true;
                break;
            }}
        }}
        if (!alreadyAdded)
        {{
            for (const auto &inc : ueCpp.useReqIncls)
            {{
                if (inc.node == parentDirNode)
                {{
                    alreadyAdded = true;
                    break;
                }}
            }}
        }}
        if (!alreadyAdded)
        {{
            ueCpp.publicIncludesSource(parentDirNode);
        }}
    }}
    else
    {{
        if (!huRegex.empty()) ueCpp.privateHUDirsRE(directory, prefix, huRegex);
        if (!incRegex.empty()) ueCpp.privateIncDirsRE(directory, prefix, incRegex);
        ueCpp.privateIncDirsRE(directory, prefix, ".*\\\\.inc")
            .privateIncDirsRE(directory, prefix, ".*\\\\.def")
            .privateIncDirsRE(directory, prefix, ".*\\\\.inl");
        
        bool alreadyAdded = false;
        for (const auto &inc : ueCpp.reqIncls)
        {{
            if (inc.node == parentDirNode)
            {{
                alreadyAdded = true;
                break;
            }}
        }}
        if (!alreadyAdded)
        {{
            ueCpp.privateIncludesSource(parentDirNode);
        }}
    }}
}}

void configurationSpecification(Configuration &config)
{{
    config.stdCppTarget->getSourceTarget().useReqIncls.clear();
    if (config.name == "standard")
    {{
        config.cppCompileCommand =
            R"({compile_command})";
    }}
    else if (config.name == "hu")
    {{
        config.cppCompileCommand =
            R"({cleaned_compile_command})";
        config.stdCppTarget->getSourceTarget()
{std_hu_includes};
    }}

{module_configurations}
}}

void buildSpecification()
{{
{config_specifications}
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

def generate_add_ue_dir_calls(target_var, root_path, is_public, current_ue_root, other_targets_inc_paths,
                              emitted_inc_dirs=None, all_known_inc_paths=None, target_include_roots=None,
                              as_header_files=False):
    calls = []
    header_regex = r'.*\\.(h|hh|hpp|hxx)'

    def is_structurally_non_standalone(header_path, content):
        normalized = header_path.replace('\\', '/')
        lower_path = normalized.lower()
        filename = os.path.basename(lower_path)

        # These trees contain implementation fragments, platform-selected
        # headers, generated shader/C++ dual-use files, or third-party headers
        # whose contract requires an umbrella header or special compile mode.
        non_standalone_components = (
            '/thirdparty/', '/shaders/', '/private/', '/internal/', '/syms/', '/resource/',
            '/windows/', '/microsoft/', '/apple/', '/mac/', '/ios/', '/tvos/', '/visionos/',
            '/android/', '/hololens/',
        )
        if any(component in lower_path for component in non_standalone_components):
            return True

        if ('/impl/' in lower_path or filename.endswith(('.impl.h', '_impl.h')) or
                filename in {'sdl_begin_code.h', 'sdl_close_code.h'}):
            return True

        if filename.startswith(('preapplesystemheaders', 'postapplesystemheaders')):
            return True

        if 'static_assert(false' in content:
            return True
        if 'size_t' in content and not re.search(r'#\s*include\s*[<"]c?stddef(?:\.h)?[>"]', content):
            return True
        return 'HEADER_UNIT_SKIP' in content
    
    def make_relative(p):
        try:
            rel_p = os.path.relpath(p, current_ue_root)
            return rel_p.replace('\\\\', '/').replace('\\', '/')
        except Exception:
            return p.replace('\\\\', '/').replace('\\', '/')

    rel_root_path = make_relative(root_path)
    normalized_root_path = root_path.replace('\\', '/')
    generated_prefix = (os.path.basename(root_path.rstrip('/')) + '/'
                        if '/Intermediate/Build/' in normalized_root_path and '/Inc/' not in normalized_root_path
                        else '')

    if not os.path.isdir(root_path):
        try:
            rel_root_ue = os.path.relpath(root_path, current_ue_root).replace('\\', '/')
            path_parts = [p.lower() for p in rel_root_ue.split('/')]
            is_private_dir = 'private' in path_parts
        except Exception:
            is_private_dir = False
        logical_is_public = (is_public or '/Intermediate/Build/' in normalized_root_path) and not is_private_dir
        prefix = generated_prefix if generated_prefix else ("" if logical_is_public else "Private/")
        calls.append(f'    addUeDirectory({target_var}, "{rel_root_path}", "{prefix}", "{header_regex}", "", {str(is_public).lower()});')
        return calls

    subdirs = []
    # Prune walk to not recurse into directories owned by other targets
    # or directories that look like sub-modules not in the build
    for dirpath, dirs, files in os.walk(root_path):
        for d in list(dirs):
            full_d = os.path.abspath(os.path.join(dirpath, d))
            if target_include_roots and full_d in target_include_roots:
                dirs.remove(d)
                continue
            if full_d in other_targets_inc_paths:
                dirs.remove(d)
                continue
        
        # Scan files in dirpath for HEADER_UNIT_SKIP
        skipped = []
        for f in files:
            if f.lower().endswith(('.h', '.hh', '.hpp', '.hxx')):
                f_path = os.path.join(dirpath, f)
                try:
                    with open(f_path, 'r', encoding='utf-8', errors='ignore') as fh:
                        content = fh.read(8000)
                        if is_structurally_non_standalone(f_path, content):
                            skipped.append(f)
                except Exception:
                    pass
        subdirs.append((dirpath, skipped))
    
    subdirs.sort(key=lambda x: x[0])
    
    for subdir, skipped in subdirs:
        rel = os.path.relpath(subdir, root_path)
        try:
            rel_subdir_ue = os.path.relpath(subdir, current_ue_root).replace('\\', '/')
            path_parts = [p.lower() for p in rel_subdir_ue.split('/')]
            is_private_dir = 'private' in path_parts
        except Exception:
            is_private_dir = False
        logical_is_public = (is_public or '/Intermediate/Build/' in normalized_root_path) and not is_private_dir
        if rel == '.':
            prefix = generated_prefix if generated_prefix else ("" if logical_is_public else "Private/")
        else:
            relative_prefix = rel.replace('\\\\', '/').replace('\\', '/') + "/"
            prefix = ((generated_prefix + relative_prefix) if generated_prefix else
                      (relative_prefix if logical_is_public else "Private/" + relative_prefix))
        
        rel_subdir = make_relative(subdir)
        
        if as_header_files:
            # Header-only providers expose ordinary textual includes through
            # HMake's name map. They intentionally do not create header-unit
            # compile jobs for every header in an unexported UE module.
            calls.append(
                f'    addUeDirectory({target_var}, "{rel_subdir}", "{prefix}", "", '
                f'"{header_regex}", {str(is_public).lower()});'
            )
        elif skipped:
            escaped = [f.replace('.', '\\\\.') for f in skipped]
            hu_regex = f"^(?!{'|'.join(escaped)}$){header_regex}"
            inc_regex = f"^({'|'.join(escaped)})$"
            calls.append(f'    addUeDirectory({target_var}, "{rel_subdir}", "{prefix}", "{hu_regex}", "{inc_regex}", {str(is_public).lower()});')
        else:
            calls.append(f'    addUeDirectory({target_var}, "{rel_subdir}", "{prefix}", "{header_regex}", "", {str(is_public).lower()});')

    return calls

def generate_target_includes_code(target_var, target_name, public_paths, private_paths, current_ue_root, own_public_hu_includes, own_private_hu_includes, all_known_inc_paths=None, configured_roots=None, sorted_names=None):
    def make_relative(p):
        try:
            rel_p = os.path.relpath(p, current_ue_root)
            return rel_p.replace('\\\\', '/').replace('\\', '/')
        except Exception:
            return p.replace('\\\\', '/').replace('\\', '/')

    def split_module_roots(paths, default_public):
        """Turn UBT's broad Source/<Module> roots into UE include roots.

        UBT occasionally exports a module directory itself. Treating that as a
        public root gives headers names such as Module/Private/Foo.h, while UE
        source uses Foo.h or Private/Foo.h. Split it before emitting mappings.
        """
        public_expanded = set()
        private_expanded = set()
        for path in paths:
            root_name = os.path.basename(os.path.normpath(path))
            if root_name in {'Public', 'Classes'}:
                public_expanded.add(path)
                continue
            if root_name == 'Private':
                private_expanded.add(path)
                continue
            module_dirs = [path]
            if os.path.isdir(path) and os.path.basename(path) == 'Source':
                module_dirs = [
                    entry.path for entry in os.scandir(path)
                    if entry.is_dir() and (sorted_names is None or entry.name in sorted_names)
                ]

            split_children = []
            for module_dir in module_dirs:
                if not os.path.isdir(module_dir) or '/Source/' not in module_dir.replace('\\', '/'):
                    continue
                for child, is_public_child in (
                    ('Public', True), ('Classes', True), ('Private', False),
                ):
                    child_path = os.path.join(module_dir, child)
                    if os.path.isdir(child_path):
                        split_children.append((child_path, is_public_child))
            if split_children:
                for child_path, is_public_child in split_children:
                    if is_public_child:
                        public_expanded.add(child_path)
                    else:
                        private_expanded.add(child_path)
            else:
                (public_expanded if default_public else private_expanded).add(path)
        return public_expanded, private_expanded

    # A broad root can appear in either UBT list. Preserve the UE directory's
    # own visibility rather than the broad-root list's accidental classification.
    public_from_public, private_from_public = split_module_roots(public_paths, True)
    public_from_private, private_from_private = split_module_roots(private_paths, False)
    public_paths = public_from_public | public_from_private
    private_paths = private_from_public | private_from_private

    if target_name.endswith("_shared") or target_name.endswith("_headers"):
        public_paths = set(public_paths) | set(private_paths)
        private_paths = set()

    public_paths = sorted(public_paths)
    private_paths = sorted(private_paths)

    if configured_roots is not None:
        def was_already_configured(path):
            return any(path == root or path.startswith(root + os.sep) for root in configured_roots)

        public_paths = [path for path in public_paths if not was_already_configured(path)]
        private_paths = [path for path in private_paths if not was_already_configured(path)]
        configured_roots.update(public_paths)
        configured_roots.update(private_paths)
    
    # We sort all parent directories (public and private combined) so that shorter paths come first
    sorted_all_parents = sorted(public_paths + private_paths, key=len)
    
    other_targets_inc_paths = set()
    for other_name, paths in own_public_hu_includes.items():
        if other_name != target_name:
            for p in paths:
                other_targets_inc_paths.add(os.path.abspath(p))
    for other_name, paths in own_private_hu_includes.items():
        if other_name != target_name:
            for p in paths:
                other_targets_inc_paths.add(os.path.abspath(p))

    def get_shadowing_parent(path, parent_set):
        for parent in parent_set:
            if len(parent) >= len(path):
                break
            if path.startswith(parent + "/") or path.startswith(parent + "\\"):
                return parent
        return None

    calls = []
    emitted_inc_dirs = set()
    target_include_roots = {os.path.abspath(path) for path in public_paths + private_paths}
    
    # Pre-populate emitted_inc_dirs with root paths that addUeDirectory already adds as includesSource
    for path in public_paths + private_paths:
        emitted_inc_dirs.add(os.path.abspath(path))
    
    for path in public_paths:
        parent = get_shadowing_parent(path, sorted_all_parents)
        rel_path = make_relative(path)
        if parent is not None:
            # Walk the include root with an empty prefix so headers are also
            # registered under the names used relative to this include root.
            # addUeDirectory itself adds includesSource when the prefix is empty;
            # emitting it separately here registers the same directory twice.
            calls.extend(generate_add_ue_dir_calls(target_var, path, True, current_ue_root,
                                                   other_targets_inc_paths, emitted_inc_dirs,
                                                   all_known_inc_paths, target_include_roots))
        else:
            calls.extend(generate_add_ue_dir_calls(target_var, path, True, current_ue_root,
                                                   other_targets_inc_paths, emitted_inc_dirs,
                                                   all_known_inc_paths, target_include_roots))
            
    for path in private_paths:
        parent = get_shadowing_parent(path, sorted_all_parents)
        rel_path = make_relative(path)
        if parent is not None:
            calls.extend(generate_add_ue_dir_calls(target_var, path, False, current_ue_root,
                                                   other_targets_inc_paths, emitted_inc_dirs,
                                                   all_known_inc_paths, target_include_roots))
        else:
            calls.extend(generate_add_ue_dir_calls(target_var, path, False, current_ue_root,
                                                   other_targets_inc_paths, emitted_inc_dirs,
                                                   all_known_inc_paths, target_include_roots))

    return calls



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
        
    return None

DIRECTORY_OVERRIDES = {
    "Engine/Source/Runtime/Experimental/Chaos/Private": "Engine/Source/Runtime/Experimental/Chaos",
    "Engine/Source/Runtime/OodleDataCompression/Sdks/2.9.14/include": "Engine/Source/Runtime/OodleDataCompression/Sdks/2.9.14",
}

# Directories where headers use relative includes to sibling subdirectories.
# These must be added as extra include directories so the compiler can resolve them.
EXTRA_INCLUDE_DIRS = {
    "Engine/Source/Runtime/TraceLog/Public/Trace",
}

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
    
    # Apply path overrides if matched
    try:
        rel_p = os.path.relpath(path, current_ue_root).replace('\\', '/')
        if rel_p in DIRECTORY_OVERRIDES:
            overridden_rel = DIRECTORY_OVERRIDES[rel_p]
            path = os.path.abspath(os.path.join(current_ue_root, overridden_rel))
    except Exception:
        pass

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

def clean_and_extend_compile_cmd(base_cmd, original_ue_root, current_ue_root, compiler=None):
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
        
    # Keep the compiler exported by UBT unless the caller explicitly overrides it.
    try:
        cmd_args = shlex.split(base_cmd)
        if cmd_args:
            if compiler:
                cmd_args[0] = compiler
            cmd_args = [arg for arg in cmd_args if arg != "-c"]
            base_cmd = shlex.join(cmd_args)
    except Exception:
        pass
        
    # Resolve relative SDK references inside BASE-COMMAND to absolute normalized paths
    base_cmd = base_cmd.replace('../Extras/', f'{current_ue_root}/Engine/Extras/')
    base_cmd = base_cmd.replace('../Extras"', f'{current_ue_root}/Engine/Extras"')
    
    # Define absolute include paths for Source, core subsystems, shaders, target platforms, and image core.
    # HMake runs compiler processes from its build directory (uebuild), so relative paths like
    # -I"." or -I"../" won't find headers in the Unreal Engine source tree. Adding absolute paths solves this.
    extra_inc = f'-I"{current_ue_root}/Engine/Source"'
    
    # Include HMakeSharedDefs.h to provide empty macro definitions for module export/import macros.
    # (e.g. CORE_API, BUILDSETTINGS_API) so the monolithic compilation of static libraries builds correctly.
    extra_hdr = f'-include"{current_ue_root}/Engine/Source/HMakeSharedDefs.h"'
    
    # Remove -I. and -I"." from the base command
    base_cmd = base_cmd.replace('-I"."', '')
    base_cmd = base_cmd.replace('-I.', '')
    
    # Inject include paths and header include into compile command
    if extra_inc not in base_cmd:
        base_cmd += f" {extra_inc}"
            
    if extra_hdr not in base_cmd:
        base_cmd += f" {extra_hdr}"
        
    if " -Wno-missing-braces" not in base_cmd:
        base_cmd += " -Wno-missing-braces"
    if " -Wno-character-conversion" not in base_cmd:
        base_cmd += " -Wno-character-conversion"
    if " -Wno-error=unknown-warning-option" not in base_cmd:
        base_cmd += " -Wno-error=unknown-warning-option"
    if " -Wno-pragma-system-header-outside-header" not in base_cmd:
        base_cmd += " -Wno-pragma-system-header-outside-header"

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
    parser.add_argument(
        '--compiler',
        default=os.environ.get('HMAKE_CXX'),
        help="Compiler override (default: compiler in UBT export; env: HMAKE_CXX)",
    )
    parser.add_argument('--hu', action='store_true', help="Enable C++20 header units ('hu') configuration")
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
    if not current_ue_root or not os.path.isdir(os.path.join(current_ue_root, 'Engine', 'Source')):
        parser.error(
            "could not locate an Unreal Engine checkout; pass --ue-root or set "
            "UNREAL_ENGINE_ROOT"
        )
    print(f"Detected Current UE Root: {current_ue_root}")
    
    # Dynamically extract Target and Platform
    target, platform = parse_target_and_platform(lines)
    print(f"Detected Build Target: {target}, Platform: {platform}")
    
    # Clean and extend base compilation command
    raw_cmd = parse_compile_command(lines)
    compile_cmd = clean_and_extend_compile_cmd(
        raw_cmd, original_ue_root, current_ue_root, args.compiler
    )
    
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

        # UBT emits Definitions.* and SharedDefinitions.* beside each unity or
        # generated module source. They are included by basename and must be
        # visible configuration-wide just like UHT output.
        for source in mod['sources']:
            portable_source = make_path_portable(source, original_ue_root, current_ue_root)
            normalized_source = portable_source.replace('\\', '/')
            if '/Intermediate/Build/' not in normalized_source:
                continue
            generated_root = os.path.dirname(portable_source)
            if (os.path.isdir(generated_root) and generated_root not in global_incs_to_ignore and
                    generated_root not in seen_local):
                public_hu.add(generated_root)
                seen_local.add(generated_root)

                
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
        try:
            rel_path = os.path.relpath(portable_path, current_ue_root).replace('\\', '/')
            parts = [p for p in rel_path.split('/') if p]
        except Exception:
            parts = [p for p in portable_path.replace('\\', '/').split('/') if p]
        if not parts:
            return "shared_headers"
        last = parts[-1]
        if len(parts) >= 2 and last.lower() in ('uht', 'public', 'private', 'internal', 'inc', 'include', 'source', 'src', 'lib', 'headers'):
            parent = parts[-2]
            name = f"{parent}_{last}"
        else:
            name = last
        # Find owning module
        owner_name = None
        parts_lower = [p.lower() for p in parts]
        for m in sorted_modules:
            m_name_lower = m['name'].lower()
            if m_name_lower in parts_lower:
                owner_name = m['name']
                break
        
        if owner_name:
            name = owner_name
            sanitized = "".join(c if c.isalnum() or c == '_' else '_' for c in name.lower())
            return f"{sanitized}_shared"
            
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

    # Find all paths that are shared by more than one module
    shared_paths = [path for path, info in path_to_sharing_info.items() if len(info) > 1]
    sorted_shared_paths = sorted(shared_paths, key=len)

    nested_path_to_parent_target = {}
    target_additional_paths = collections.defaultdict(list)
    path_to_target_name = {}

    for path in sorted_shared_paths:
        # Check if this path is shadowed by any shorter shared path
        parent = None
        for p in sorted_shared_paths:
            if len(p) >= len(path):
                break
            if path.startswith(p + "/") or path.startswith(p + "\\"):
                parent = p
                break
        
        if parent is not None:
            parent_target = path_to_target_name[parent]
            nested_path_to_parent_target[path] = parent_target
            target_additional_paths[parent_target].append(path)
        else:
            new_target_name = get_automated_target_name(path)
            path_to_target_name[path] = new_target_name

    created_targets = {}
    for path in sorted_shared_paths:
        sharing_info = path_to_sharing_info[path]
        
        if path in nested_path_to_parent_target:
            parent_target_name = nested_path_to_parent_target[path]
            for mod, dep_type in sharing_info:
                name = mod['name']
                if dep_type == 'public':
                    if path in own_public_hu_includes[name]:
                        own_public_hu_includes[name].remove(path)
                    if parent_target_name not in mod['public_deps']:
                        mod['public_deps'].append(parent_target_name)
                elif dep_type == 'private':
                    if path in own_private_hu_includes[name]:
                        own_private_hu_includes[name].remove(path)
                    if parent_target_name not in mod['private_deps']:
                        mod['private_deps'].append(parent_target_name)
        else:
            new_target_name = path_to_target_name[path]
            if new_target_name not in created_targets:
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
                modules.append(new_target)
                synthesized_uht_targets.append(new_target)
                own_public_hu_includes[new_target_name] = {path}
                own_private_hu_includes[new_target_name] = set()
                created_targets[new_target_name] = new_target
            else:
                own_public_hu_includes[new_target_name].add(path)
                
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

    # Add merged additional paths to their respective parent targets
    for target_name, paths in target_additional_paths.items():
        for p in paths:
            own_public_hu_includes[target_name].add(p)

    # Assign dependencies to synthesized targets based on their owning modules
    sorted_names = set(m['name'] for m in modules)
    for target in synthesized_uht_targets:
        # Find owning module
        owner_name = None
        for m in sorted_modules:
            if m['name'] == target['name'] or target['name'].startswith(m['name'].lower() + "_"):
                owner_name = m['name']
                break
        if owner_name:
            owner_mod = next(m for m in sorted_modules if m['name'] == owner_name)
            for dep in owner_mod['public_deps'] + owner_mod['private_deps']:
                dep_shared = f"{dep.lower()}_shared"
                if dep_shared in sorted_names and dep_shared != target['name']:
                    if dep_shared not in target['public_deps']:
                        target['public_deps'].append(dep_shared)

    # Prepend synthesized targets to sorted_modules so they are declared first
    if synthesized_uht_targets:
        sorted_modules = synthesized_uht_targets + sorted_modules
        print(f"Synthesized {len(synthesized_uht_targets)} header-only targets to deduplicate shared includes.")

    # Topological sort the final combined list of modules to avoid use-before-declaration errors
    def real_topological_sort(modules_list):
        adj = {}
        mod_map = {m['name']: m for m in modules_list}
        for m in modules_list:
            adj[m['name']] = m['public_deps'] + m['private_deps']
            
        visited = set()
        temp_visited = set()
        result = []
        cycles_broken = []
        
        def visit(name):
            if name in temp_visited:
                return True
            if name not in visited:
                temp_visited.add(name)
                if name in adj:
                    pruned_deps = []
                    for dep in adj[name]:
                        if visit(dep):
                            cycles_broken.append((name, dep))
                        else:
                            pruned_deps.append(dep)
                    adj[name] = pruned_deps
                temp_visited.remove(name)
                visited.add(name)
                result.append(name)
            return False
            
        for m in modules_list:
            visit(m['name'])
            
        for m in modules_list:
            m['public_deps'] = [d for d in m['public_deps'] if d in adj[m['name']]]
            m['private_deps'] = [d for d in m['private_deps'] if d in adj[m['name']]]
            
        if cycles_broken:
            print(f"  Real topo-sort pruned {len(cycles_broken)} back-edge(s) to avoid cycles.")
            
        return [mod_map[name] for name in result]

    sorted_modules = real_topological_sort(sorted_modules)

    # Build set of all known module include paths (absolute) for sub-module pruning
    all_known_inc_paths = set()
    for mod in sorted_modules:
        for inc in mod['public_incs'] + mod['private_incs'] + mod.get('internal_incs', []) + mod.get('system_incs', []):
            portable = make_path_portable(inc, original_ue_root, current_ue_root)
            all_known_inc_paths.add(os.path.abspath(portable))
    # Also add all own_public/private_hu_includes paths
    for paths in own_public_hu_includes.values():
        for p in paths:
            all_known_inc_paths.add(os.path.abspath(p))
    for paths in own_private_hu_includes.values():
        for p in paths:
            all_known_inc_paths.add(os.path.abspath(p))
    print(f"Built index of {len(all_known_inc_paths)} known include paths for sub-module pruning.")

    # Unity files often include implementation files as "Private/Foo.cpp".
    # That spelling is ambiguous configuration-wide, unlike headers. Resolve it
    # from the unity file's owning module and store the Engine/Source-relative
    # spelling used by the source-file registration below.
    import re
    module_source_dirs = collections.defaultdict(list)
    source_tree = os.path.join(current_ue_root, 'Engine', 'Source')
    for dirpath, dirs, _ in os.walk(source_tree):
        if os.path.basename(dirpath) == 'Source':
            for directory in dirs:
                candidate = os.path.join(dirpath, directory)
                if os.path.isdir(os.path.join(candidate, 'Private')):
                    module_source_dirs[directory].append(candidate)

    unity_include_re = re.compile(r'(^\s*#\s*include\s*")((?:Private|Public)/[^"\n]+\.(?:c|cc|cpp|cxx))(".*$)', re.MULTILINE)
    unity_rewrites = 0
    for mod in sorted_modules:
        for src in mod['sources']:
            unity_path = make_path_portable(src, original_ue_root, current_ue_root)
            if '/Intermediate/Build/' not in unity_path.replace('\\', '/') or not os.path.isfile(unity_path):
                continue
            candidates_roots = module_source_dirs.get(mod['name'], [])
            if not candidates_roots:
                continue
            try:
                with open(unity_path, 'r', encoding='utf-8', errors='ignore') as unity_file:
                    unity_content = unity_file.read()

                def rewrite_unity_include(match):
                    nonlocal unity_rewrites
                    include_name = match.group(2)
                    candidates = [
                        os.path.join(root, include_name) for root in candidates_roots
                        if os.path.isfile(os.path.join(root, include_name))
                    ]
                    if len(candidates) != 1:
                        return match.group(0)
                    unity_rewrites += 1
                    logical = os.path.relpath(candidates[0], source_tree).replace('\\', '/')
                    return match.group(1) + logical + match.group(3)

                rewritten = unity_include_re.sub(rewrite_unity_include, unity_content)
                if rewritten != unity_content:
                    with open(unity_path, 'w', encoding='utf-8') as unity_file:
                        unity_file.write(rewritten)
            except OSError as error:
                print(f"Warning: could not normalize unity file {unity_path}: {error}")
    if unity_rewrites:
        print(f"Canonicalized {unity_rewrites} unity implementation includes.")

    # Generate the C++ target DSC configuration block
    out = []
    out.append("    // Module configurations")
    inferred_manifest_path = os.path.join(current_ue_root, "hmake-inferred-headers.json")
    inferred_headers = {}
    try:
        with open(inferred_manifest_path, 'r', encoding='utf-8') as manifest_file:
            manifest_data = json.load(manifest_file)
        inferred_headers = manifest_data.get('headers', {})
    except FileNotFoundError:
        pass
    except (OSError, ValueError, TypeError) as error:
        print(f"Warning: could not read {inferred_manifest_path}: {error}")

    if inferred_headers:
        declared_roots = [
            os.path.abspath(root)
            for paths in list(own_public_hu_includes.values()) + list(own_private_hu_includes.values())
            for root in paths
        ]
        inferred_headers = {
            logical: physical for logical, physical in inferred_headers.items()
            if not any(
                os.path.commonpath((os.path.abspath(physical), root)) == root
                for root in declared_roots
            )
        }
        # Configure whole include roots for modules absent from the UBT target,
        # but deliberately emit no moduleFiles(). This gives their consumers a
        # normal header-only target instead of a bag of one-off aliases.
        inferred_roots = {}
        loose_headers = {}
        for logical, physical in sorted(inferred_headers.items()):
            physical = os.path.abspath(physical)
            normalized = physical.replace('\\', '/')
            root = None
            is_public = True
            if '/Intermediate/Build/' in normalized and '/Inc/' in normalized:
                uht_marker = normalized.find('/UHT/')
                if uht_marker != -1:
                    root = normalized[:uht_marker + len('/UHT')]
            if root is None:
                logical_parts = logical.split('/')
                has_global_prefix = (logical_parts[0] in {'Runtime', 'Developer', 'Editor', 'ThirdParty', 'Programs', 'Plugins', 'Engine'} or
                                     any(p in {'Public', 'Private', 'Classes'} for p in logical_parts))
                if not has_global_prefix:
                    parts = physical.split(os.sep)
                    candidates = [
                        index for index, part in enumerate(parts)
                        if part in {'Public', 'Private', 'Classes'}
                    ]
                    if candidates:
                        marker_index = candidates[-1]
                        root = os.sep.join(parts[:marker_index + 1]) or os.sep
                        is_public = parts[marker_index] != 'Private'
            if root and os.path.isdir(root):
                inferred_roots[os.path.abspath(root)] = is_public
            else:
                loose_headers[logical] = physical

        out.append('    DSC<CppTarget> &hmakeInferredHeaders = config.getCppStaticDSC("HMakeInferredHeaders");')
        for root, is_public in sorted(inferred_roots.items()):
            out.extend(generate_add_ue_dir_calls(
                'hmakeInferredHeaders', root, is_public, current_ue_root,
                set(), target_include_roots=set(inferred_roots), as_header_files=True
            ))
        for logical, physical in sorted(loose_headers.items()):
            if os.path.isfile(physical):
                out.append(
                    f'    hmakeInferredHeaders.getSourceTarget().publicHeaderFiles('
                    f'{escape_cpp_string(logical)}, {escape_cpp_string(physical)});')
        out.append("")
        print(
            f"Configured {len(inferred_roots)} inferred header-only module roots "
            f"and {len(loose_headers)} loose headers from the previous pass."
        )
    sorted_names = set(m['name'] for m in sorted_modules)
    module_files_written_count = 0
    configured_header_roots = set()
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
        should_write_sources = True
        if args.limit is not None:
            has_cpp_sources = any(not (src.endswith('.ispc') or src.endswith('.ispc.o')) for src in mod['sources'])
            if has_cpp_sources:
                if module_files_written_count >= args.limit:
                    should_write_sources = False
                else:
                    module_files_written_count += 1
            
        # Modules that require exceptions/RTTI or are manually skipped
        skipped_compilation_modules = {"OpenExrWrapper"}
        
        has_sources = should_write_sources and mod['name'] not in skipped_compilation_modules and any(
            not (src.endswith('.ispc') or src.endswith('.ispc.o') or src.endswith('MiMalloc.c')) for src in mod['sources']
        )
        
        has_config_chain = (
            has_sources or 
            mod['public_defs'] or 
            mod['private_defs']
        )
        
        if has_config_chain:
            out.append(f"    {var_name}.getSourceTarget()")
            
            if has_sources:
                for src in mod['sources']:
                    if src.endswith('.ispc') or src.endswith('.ispc.o'):
                        # TEMPORARY CHANGE: Skip ISPC files for now since they cannot be integrated directly as C++ source or pre-built library in HMake
                        continue
                    if src.endswith('MiMalloc.c'):
                        # Skip MiMalloc.c as instructed by the user
                        continue
                    portable_src = make_path_portable(src, original_ue_root, current_ue_root)
                    out.append(f'        .moduleFiles("{portable_src}")')
                
            for df in mod['public_defs']:
                df_stripped = df.strip()
                if '=' in df_stripped:
                    name, val = df_stripped.split('=', 1)
                    name = name.strip()
                    val = val.strip()
                elif ' ' in df_stripped:
                    parts = df_stripped.split(None, 1)
                    name = parts[0].strip()
                    val = parts[1].strip()
                else:
                    name, val = df_stripped, "1"
                val_str = escape_cpp_string(val)
                out.append(f'        .publicCompileDefines("{name}", {val_str})')
                
            for df in mod['private_defs']:
                df_stripped = df.strip()
                if '=' in df_stripped:
                    name, val = df_stripped.split('=', 1)
                    name = name.strip()
                    val = val.strip()
                elif ' ' in df_stripped:
                    parts = df_stripped.split(None, 1)
                    name = parts[0].strip()
                    val = parts[1].strip()
                else:
                    name, val = df_stripped, "1"
                val_str = escape_cpp_string(val)
                out.append(f'        .privateCompileDefines("{name}", {val_str})')
                
            # Terminate the getSourceTarget chain with a semicolon
            out[-1] += ";"
            
        public_paths = own_public_hu_includes[mod['name']]
        private_paths = own_private_hu_includes[mod['name']]
        inc_calls = generate_target_includes_code(
            var_name, mod['name'], public_paths, private_paths, current_ue_root,
            own_public_hu_includes, own_private_hu_includes, all_known_inc_paths,
            configured_header_roots, sorted_names=sorted_names,
        )
        if mod['name'] == 'Paper2D':
            print("PAPER2D GENERATED CALLS:")
            for call in inc_calls:
                if "addUeDirectory" in call and "Classes" in call:
                    print("  ", call)
        out.extend(inc_calls)
        
        # Register C/C++ source files as include files to support unity builds without conflicts
        source_root = os.path.abspath(os.path.join(current_ue_root, "Engine", "Source")).replace('\\', '/')
        registered_files = set()
        for src in mod['sources']:
            if src.endswith('.cpp') or src.endswith('.c') or src.endswith('.cc') or src.endswith('.cxx'):
                if src.endswith('MiMalloc.c'):
                    continue
                portable_src = make_path_portable(src, original_ue_root, current_ue_root)
                if "Intermediate/Build" in portable_src and os.path.exists(portable_src):
                    try:
                        import re
                        with open(portable_src, 'r', encoding='utf-8', errors='ignore') as f:
                            for line in f:
                                m = re.search(r'#\s*include\s*["<]([^">]+)[">]', line)
                                if m:
                                    inc_name = m.group(1).replace('\\', '/')
                                    if inc_name.endswith('.cpp') or inc_name.endswith('.c') or inc_name.endswith('.cc') or inc_name.endswith('.cxx'):
                                        portable_phys = None
                                        inc_suffix = "/" + inc_name
                                        for s_file in mod['sources']:
                                            portable_s = make_path_portable(s_file, original_ue_root, current_ue_root)
                                            if portable_s.endswith(inc_suffix):
                                                portable_phys = portable_s
                                                break
                                        if not portable_phys:
                                            candidate_roots = set()
                                            for inc_path in mod.get('public_incs', []) + mod.get('private_incs', []) + mod.get('internal_incs', []):
                                                candidate_roots.add(inc_path)
                                                candidate_roots.add(os.path.dirname(inc_path))
                                                candidate_roots.add(os.path.dirname(os.path.dirname(inc_path)))
                                                candidate_roots.add(os.path.dirname(os.path.dirname(os.path.dirname(inc_path))))
                                            for root in sorted(list(candidate_roots), key=len, reverse=True):
                                                candidate = os.path.normpath(os.path.join(root, inc_name))
                                                if os.path.exists(candidate):
                                                    portable_phys = make_path_portable(candidate, original_ue_root, current_ue_root)
                                                    break
                                        if not portable_phys:
                                            for root in module_source_dirs.get(mod['name'], []):
                                                candidate = os.path.normpath(os.path.join(root, inc_name))
                                                if os.path.exists(candidate):
                                                    portable_phys = make_path_portable(candidate, original_ue_root, current_ue_root)
                                                    break
                                        if not portable_phys:
                                            # Fallback to old behavior
                                            phys_path = os.path.normpath(os.path.join(current_ue_root, "Engine", "Source", inc_name))
                                            if os.path.exists(phys_path):
                                                portable_phys = make_path_portable(phys_path, original_ue_root, current_ue_root)
                                        
                                        if portable_phys and os.path.exists(portable_phys):
                                            if portable_phys not in registered_files:
                                                out.append(f'    {var_name}.getSourceTarget().privateHeaderFiles("{inc_name}", "{portable_phys}");')
                                                registered_files.add(portable_phys)
                    except Exception:
                        pass
                if portable_src.startswith(source_root):
                    logical_name = os.path.relpath(portable_src, source_root).replace('\\', '/')
                    if portable_src not in registered_files:
                        out.append(f'    {var_name}.getSourceTarget().privateHeaderFiles("{logical_name}", "{portable_src}");')
                        registered_files.add(portable_src)
        out.append("")
        
    modules_code = "\n".join(out)
    
    # Parse -I and -isystem paths for C++20 header units in 'hu' configuration
    full_args = shlex.split(compile_cmd)
    cleaned_args = []
    inc_paths = []
    
    i = 0
    while i < len(full_args):
        arg = full_args[i]
        if arg.startswith('-I'):
            path = arg[2:]
            if not path:
                i += 1
                path = full_args[i]
            inc_paths.append(path.strip('"'))
        elif arg.startswith('-isystem'):
            path = arg[8:]
            if not path:
                i += 1
                path = full_args[i]
            inc_paths.append(path.strip('"'))
        else:
            cleaned_args.append(arg)
        i += 1
        
    cleaned_compile_cmd = shlex.join(cleaned_args)
    if not cleaned_compile_cmd.endswith(" "):
        cleaned_compile_cmd += " "
    
    all_module_inc_paths = set()
    for paths in own_public_hu_includes.values():
        for p in paths:
            all_module_inc_paths.add(os.path.abspath(p).replace('\\', '/'))
    for paths in own_private_hu_includes.values():
        for p in paths:
            all_module_inc_paths.add(os.path.abspath(p).replace('\\', '/'))

    seen = set()
    unique_inc_paths = []
    engine_source_root = os.path.abspath(
        os.path.join(current_ue_root, 'Engine', 'Source')
    ).replace('\\', '/')
    for x in inc_paths:
        abs_x = os.path.abspath(x).replace('\\', '/')
        # Engine/Source is a compiler lookup root, not a standard-library HU
        # tree. Recursing it would attempt to compile C#, target rules, and all
        # UE module headers as part of std-cpp.
        if abs_x == engine_source_root:
            continue
        if abs_x in all_module_inc_paths:
            continue
        if abs_x not in seen:
            seen.add(abs_x)
            unique_inc_paths.append(x)
    
    shared_defs = os.path.join(
        current_ue_root, 'Engine', 'Source', 'HMakeSharedDefs.h'
    ).replace('\\', '/')
    std_hu_includes_lines = [
        f'            .publicHeaderUnits("{shared_defs}", "{shared_defs}")'
    ]
    for path in unique_inc_paths:
        std_hu_includes_lines.append(f'            .publicHUIncludes("{path}")')
    std_hu_includes = "\n".join(std_hu_includes_lines)

    # Format C++ template and write directly to ./hmake.cpp
    if args.hu:
        config_specifications = (
            '    getConfiguration("standard").assign(IsCppMod::NO, BigHeaderUnit::NO, UseConfigurationScope::YES, AssignStandardCppTarget::NO);\n'
            '    getConfiguration("hu").assign(IsCppMod::YES, BigHeaderUnit::NO, UseConfigurationScope::YES, AssignStandardCppTarget::NO);'
        )
    else:
        config_specifications = (
            '    getConfiguration("standard").assign(IsCppMod::NO, BigHeaderUnit::NO, UseConfigurationScope::YES, AssignStandardCppTarget::NO);'
        )

    hmake_content = HMAKE_TEMPLATE.format(
        compile_command=compile_cmd,
        cleaned_compile_command=cleaned_compile_cmd,
        std_hu_includes=std_hu_includes,
        config_specifications=config_specifications,
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

    # Automatically patch Trace.h to include Trace/Detail/Trace.h instead of Detail/Trace.h
    trace_header_path = os.path.join(current_ue_root, "Engine", "Source", "Runtime", "TraceLog", "Public", "Trace", "Trace.h")
    if os.path.exists(trace_header_path):
        try:
            with open(trace_header_path, 'r', encoding='utf-8') as f:
                content = f.read()
            target_inc = '#include "Detail/Trace.h"'
            replacement_inc = '#include "Trace/Detail/Trace.h"'
            if target_inc in content:
                content = content.replace(target_inc, replacement_inc)
                with open(trace_header_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                print(f"Automatically patched {trace_header_path} to use correct relative path.")
        except Exception as e:
            print(f"Warning: Failed to automatically patch {trace_header_path}: {e}")

    # Automatically patch Fortnite season branch custom object version headers to include UObject/DevObjectVersion.h instead of DevObjectVersion.h
    uobject_dir = os.path.join(current_ue_root, "Engine", "Source", "Runtime", "Core", "Public", "UObject")
    if os.path.isdir(uobject_dir):
        for f_name in os.listdir(uobject_dir):
            if f_name.endswith('.h'):
                file_path = os.path.join(uobject_dir, f_name)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    target_inc = '#include "DevObjectVersion.h"'
                    replacement_inc = '#include "UObject/DevObjectVersion.h"'
                    if target_inc in content:
                        content = content.replace(target_inc, replacement_inc)
                        with open(file_path, 'w', encoding='utf-8') as f:
                            f.write(content)
                        print(f"Automatically patched {file_path} to use correct relative path.")
                except Exception as e:
                    print(f"Warning: Failed to automatically patch {file_path}: {e}")

    # Canonicalize quoted relative includes so build-time resolution remains a
    # single logical-name hash-map lookup. Public names are relative to their
    # include root; private names are prefixed with Private/.
    import re
    root_kinds = {}

    def add_canonical_root(root, fallback_kind):
        root = os.path.abspath(root)
        normalized = root.replace('\\', '/')
        if '/Intermediate/Build/' in normalized:
            root_kinds[root] = 'generated' if '/Inc/' not in normalized else 'public'
            return

        root_name = os.path.basename(root)
        if root_name in {'Public', 'Classes'}:
            root_kinds[root] = 'public'
            return
        if root_name == 'Private':
            root_kinds[root] = 'private'
            return

        candidates = [root]
        if os.path.isdir(root) and os.path.basename(root) == 'Source':
            candidates = [entry.path for entry in os.scandir(root) if entry.is_dir() and entry.name in sorted_names]
        split = False
        for candidate in candidates:
            for child, kind in (('Public', 'public'), ('Classes', 'public'), ('Private', 'private')):
                child_path = os.path.join(candidate, child)
                if os.path.isdir(child_path):
                    root_kinds[os.path.abspath(child_path)] = kind
                    split = True
        if not split:
            root_kinds[root] = fallback_kind

    for paths in own_public_hu_includes.values():
        for root in paths:
            add_canonical_root(root, 'public')
    for paths in own_private_hu_includes.values():
        for root in paths:
            add_canonical_root(root, 'private')

    canonical_names = {}
    include_alias_owners = collections.defaultdict(set)
    for root, kind in sorted(root_kinds.items(), key=lambda item: len(item[0]), reverse=True):
        if not os.path.isdir(root):
            continue
        for dirpath, dirs, files in os.walk(root):
            # A more-specific declared include root owns its subtree.
            dirs[:] = [directory for directory in dirs
                       if os.path.abspath(os.path.join(dirpath, directory)) not in root_kinds]
            for filename in files:
                physical = os.path.abspath(os.path.join(dirpath, filename))
                relative = os.path.relpath(physical, root).replace('\\', '/')
                if kind == 'generated':
                    logical = os.path.basename(root.rstrip('/')) + '/' + relative
                else:
                    logical = relative if kind == 'public' else 'Private/' + relative
                canonical_names.setdefault(physical, logical)
                include_alias_owners[relative].add(physical)
                include_alias_owners[logical].add(physical)
                # Older generated configurations exposed an entire module
                # directory and rewrote includes as Module/Public/Foo.h (or
                # Module/Private/Foo.h). Keep that spelling only as a rewrite
                # alias so sources converge to the root-relative canonical
                # logical name emitted by the split-root configuration.
                root_name = os.path.basename(root)
                if root_name in {'Public', 'Private', 'Classes'}:
                    module_name = os.path.basename(os.path.dirname(root))
                    include_alias_owners[
                        f'{module_name}/{root_name}/{relative}'
                    ].add(physical)
                    source_marker = '/Engine/Source/'
                    normalized_root = root.replace('\\', '/')
                    if source_marker in normalized_root:
                        source_relative = normalized_root.split(source_marker, 1)[1]
                        include_alias_owners[
                            f'{source_relative}/{relative}'
                        ].add(physical)

    logical_owners = collections.defaultdict(set)
    for physical, logical in canonical_names.items():
        logical_owners[logical].add(physical)
    unique_logical_names = {
        logical: next(iter(owners)) for logical, owners in logical_owners.items() if len(owners) == 1
    }
    print("Canonicalizing quoted includes against declared include roots...")
    patch_count = 0
    rewritten_include_count = 0
    unresolved_includes = collections.Counter()
    ambiguous_includes = collections.Counter()
    include_re = re.compile(r'(^\s*#\s*include\s*")([^\"]+)(")', re.MULTILINE)
    engine_root = os.path.join(current_ue_root, "Engine")
    for dirpath, dirs, files in os.walk(engine_root):
        # Skip output trees that never participate in compilation. Intermediate
        # is intentionally scanned because UHT and unity inputs are compiled.
        dirs[:] = [directory for directory in dirs
                   if directory not in {'Binaries', 'DerivedDataCache', 'Saved'}]
        for filename in files:
            if not filename.lower().endswith(('.h', '.hpp', '.inl', '.inc', '.c', '.cc', '.cpp', '.cxx')):
                continue
            file_path = os.path.join(dirpath, filename)
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as source_file:
                    content = source_file.read()

                def canonicalize(match):
                    nonlocal rewritten_include_count
                    include_name = match.group(2).replace('\\', '/')

                    # Skip definitions files from being canonicalized/rewritten globally, and restore them if already rewritten.
                    include_basename = os.path.basename(include_name)
                    if include_basename == "Definitions.h" or (include_basename.startswith("Definitions.") and include_basename.endswith(".h")):
                        if include_name != include_basename:
                            rewritten_include_count += 1
                            return match.group(1) + include_basename + match.group(3)
                        return match.group(0)

                    # It is already the unique canonical mapping HMake stores.
                    if include_name in unique_logical_names:
                        return match.group(0)

                    candidates = set()
                    requester_relative = os.path.abspath(os.path.normpath(os.path.join(dirpath, include_name)))
                    if requester_relative in canonical_names:
                        candidates.add(requester_relative)

                    candidates.update(include_alias_owners.get(include_name, ()))
                    if '/Intermediate/Build/' in dirpath.replace('\\', '/') and include_name.startswith('Private/'):
                        candidates.update(include_alias_owners.get(include_name[len('Private/'):], ()))

                    if len(candidates) == 0:
                        unresolved_includes[include_name] += 1
                        return match.group(0)
                    if len(candidates) > 1:
                        ambiguous_includes[include_name] += 1
                        return match.group(0)

                    physical = next(iter(candidates))
                    logical = canonical_names[physical]
                    if physical.lower().endswith(('.c', '.cc', '.cpp', '.cxx')):
                        # Unity files are registered by the source-file block
                        # below using Engine/Source-relative names. They are
                        # implementation files, not public/private headers.
                        logical = os.path.relpath(
                            physical, os.path.join(current_ue_root, 'Engine', 'Source')
                        ).replace('\\', '/')
                    if (logical == include_name or
                            (not physical.lower().endswith(('.c', '.cc', '.cpp', '.cxx')) and
                             len(logical_owners[logical]) != 1)):
                        return match.group(0)

                    rewritten_include_count += 1
                    return match.group(1) + logical + match.group(3)

                new_content = include_re.sub(canonicalize, content)
                if ('/Intermediate/Build/' in file_path.replace('\\', '/') and
                        os.path.basename(file_path).startswith(('Definitions.', 'SharedDefinitions.'))):
                    # Normalize accidentally duplicated scalar values such as
                    # "#define WITH_AITESTSUITE 1 1" in generated definition headers.
                    new_content = re.sub(
                        r'^(\s*#\s*define\s+[A-Za-z_]\w*\s+)(\S+)\s+\2\s*$',
                        r'\1\2', new_content, flags=re.MULTILINE)
                if new_content != content:
                    with open(file_path, 'w', encoding='utf-8') as source_file:
                        source_file.write(new_content)
                    patch_count += 1
            except OSError as error:
                print(f"Warning: could not canonicalize includes in {file_path}: {error}")

    print(f"Patched {patch_count} files ({rewritten_include_count} include directives).")
    print(f"Unresolved quoted include names: {len(unresolved_includes)}")
    print(f"Ambiguous quoted include names: {len(ambiguous_includes)}")
    if unresolved_includes:
        print("Most frequent unresolved includes:")
        for include_name, count in unresolved_includes.most_common(20):
            print(f"  {count:6}  {include_name}")
    if ambiguous_includes:
        print("Most frequent ambiguous includes:")
        for include_name, count in ambiguous_includes.most_common(20):
            print(f"  {count:6}  {include_name}")

    # Some headers referenced by exported modules live in conventional UE module
    # trees that UBT did not export as include roots for this target. Register only
    # unresolved names with one exact suffix match. This avoids globally importing
    # every editor/plugin tree and never guesses when a name is ambiguous.
    suffix_candidates = collections.defaultdict(set)
    missing_names_path = os.path.join(current_ue_root, 'hmake-missing-includes.txt')
    requested_missing_names = set()
    try:
        with open(missing_names_path, 'r', encoding='utf-8') as missing_file:
            requested_missing_names = {
                line.strip().replace('\\', '/') for line in missing_file if line.strip()
            }
    except FileNotFoundError:
        pass
    unresolved_names = set(unresolved_includes).intersection(requested_missing_names)
    if unresolved_names:
        unresolved_by_basename = collections.defaultdict(set)
        for include_name in unresolved_names:
            unresolved_by_basename[os.path.basename(include_name)].add(include_name)
        for dirpath, dirs, files in os.walk(engine_root):
            dirs[:] = [directory for directory in dirs
                       if directory not in {'Binaries', 'DerivedDataCache', 'Saved'}]
            for filename in files:
                if not filename.lower().endswith(('.h', '.hh', '.hpp', '.hxx', '.inc', '.inl')):
                    continue
                physical = os.path.abspath(os.path.join(dirpath, filename))
                normalized = physical.replace('\\', '/')
                for include_name in unresolved_by_basename.get(filename, ()):
                    if normalized.endswith('/' + include_name):
                        suffix_candidates[include_name].add(physical)

    inferred_headers = {
        include_name: next(iter(candidates))
        for include_name, candidates in suffix_candidates.items()
        if len(candidates) == 1
    }
    manifest = {
        'format': 1,
        'headers': dict(sorted(inferred_headers.items())),
        'ambiguous': {
            include_name: sorted(candidates)
            for include_name, candidates in sorted(suffix_candidates.items())
            if len(candidates) > 1
        },
    }
    try:
        with open(os.path.join(current_ue_root, 'hmake-inferred-headers.json'), 'w', encoding='utf-8') as manifest_file:
            json.dump(manifest, manifest_file, indent=2, sort_keys=True)
            manifest_file.write('\n')
        print(f"Recorded {len(inferred_headers)} uniquely inferred build-failing header names.")
        print(f"Left {len(manifest['ambiguous'])} suffix-matched names ambiguous.")
    except OSError as error:
        print(f"Warning: could not write inferred-header manifest: {error}")

if __name__ == '__main__':
    main()
