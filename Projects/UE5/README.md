# Unreal Engine configuration generator

This directory contains the reproducible UE-to-HMake configuration workflow used
while bringing up Unreal Engine header-unit builds.

## Files

- `generator.py` is the user-facing command. It runs the configuration in two
  deterministic passes and can learn only the missing include names reported by
  an `hbuild` log.
- `script.py` parses a UBT export, writes `hmake.cpp`, normalizes quoted include
  directives to stable logical names, and writes the inferred-header manifest.

The runtime header resolver remains a single logical-name hash-map lookup. These
scripts do not add requester-relative lookup to HMake.

## Prerequisites

1. A UE checkout containing `Engine/Source`.
2. A UBT export named `output.txt` in the UE checkout root. The export must contain
   the `BASE-COMMAND` and module/include/dependency sections understood by
   `script.py`.
3. HMake commands (`hhelper` and `hbuild`) available on `PATH`.
4. The compiler recorded in `output.txt`, or an explicit compatible compiler via
   `--compiler` or `HMAKE_CXX`.

No username, home directory, Unreal checkout path, SDK version, or compiler path
is hard-coded by the packaged workflow.

## Clean-checkout workflow

From the HMake checkout:

```sh
python3 UE5/generator.py --ue-root /path/to/UnrealEngine
```

The command runs two passes. Pass one canonicalizes sources and produces
`hmake-inferred-headers.json`; pass two consumes that manifest into `hmake.cpp`.
Both passes are idempotent, so rerunning the command should produce no additional
source changes after the first successful run.

Then configure HMake:

```sh
cd /path/to/UnrealEngine/uebuild
hhelper
```

Build the HU configuration explicitly:

```sh
cd hu
hbuild 2>&1 | tee ../hbuild-hu.log
```

Feed actual missing-header diagnostics back into the next deterministic pass:

```sh
python3 /path/to/HMake/UE5/generator.py \
  --ue-root /path/to/UnrealEngine \
  --build-log /path/to/UnrealEngine/uebuild/hbuild-hu.log
```

The log parser accumulates names from HMake's `provides this header` diagnostics
across runs, allowing repeated build/generate cycles to reach a dependency fixed
point without forgetting previously discovered header-only modules.
Only names with exactly one matching physical UE header are inferred. Ambiguous
names are recorded but never guessed. Missing module roots are registered as
header-file providers—not header units—and therefore produce no object files or
bulk header-unit compilation. Headers already owned by an exported target are not
registered a second time.

## Generated files

The scripts write these files in the UE checkout:

- `hmake.cpp` — generated HMake configuration.
- `hmake-missing-includes.txt` — distinct missing names extracted from a supplied
  build log.
- `hmake-inferred-headers.json` — deterministic unique-suffix mappings and the
  ambiguous candidates left unresolved.
- `Engine/Source/HMakeSharedDefs.h` — guarded shared definitions for the
  monolithic configuration.

They may also normalize source include directives and malformed duplicated scalar
values in generated `Definitions.*.h` files. Source rewriting uses canonical public
names such as `HAL/Platform.h` and private names such as
`Private/IO/IoDispatcherChunkDecoder.h`.

## Design constraints

- Public names are relative to a declared public include root.
- Private names use the `Private/` prefix.
- Generated module headers use a module-qualified logical prefix.
- Ambiguous basename/suffix matches are never selected automatically.
- Linux SDK and Clang intrinsic headers remain the responsibility of HMake's
  composing-header/big-standard-HU mechanism.
- `generator.py` never invokes `hhelper` or `hbuild`; those remain explicit steps.

## Verification

For a reproducibility check, run the generator twice and verify that the second
run reports zero patched include directives. In a disposable clean UE worktree,
compare `git status --short` and `hmake-inferred-headers.json` across repeated runs.
Do not use `git reset --hard` in a worktree containing changes you need to keep.
