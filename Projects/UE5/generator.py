#!/usr/bin/env python3
"""Reproduce Unreal source normalization and its HMake configuration."""

import argparse
import os
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Generate reproducible HMake standard/HU configuration for Unreal Engine."
    )
    parser.add_argument(
        "--ue-root",
        required=True,
        help="Unreal Engine checkout containing output.txt",
    )
    parser.add_argument(
        "--build-log",
        help="hbuild output used to restrict inferred headers to actual failures",
    )
    parser.add_argument(
        "--compiler",
        default=os.environ.get("HMAKE_CXX"),
        help="Compiler override (default: UBT export; env: HMAKE_CXX)",
    )
    args = parser.parse_args()
    ue_root = os.path.abspath(args.ue_root)
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "script.py")

    if not os.path.isfile(script):
        parser.error(f"packaged configuration generator is missing: {script}")
    if not os.path.isfile(os.path.join(ue_root, "output.txt")):
        parser.error(f"missing UBT export: {os.path.join(ue_root, 'output.txt')}")

    if args.build_log:
        with open(args.build_log, 'r', encoding='utf-8', errors='ignore') as log_file:
            log = log_file.read()
        missing = set(re.findall(
            r'provides this header\s*\n([^\n]+)\n\s*requested in', log
        ))
        missing_path = os.path.join(ue_root, 'hmake-missing-includes.txt')
        try:
            with open(missing_path, 'r', encoding='utf-8') as missing_file:
                missing.update(line.strip() for line in missing_file if line.strip())
        except FileNotFoundError:
            pass
        missing = sorted(name.strip() for name in missing if name.strip())
        with open(missing_path, 'w', encoding='utf-8') as missing_file:
            missing_file.write('\n'.join(name.strip() for name in missing))
            if missing:
                missing_file.write('\n')
        print(
            f"Accumulated {len(missing)} missing include names through {args.build_log}",
            flush=True,
        )

    # The first pass deterministically rewrites source includes and creates the
    # unique-suffix manifest. The second pass consumes that manifest into hmake.cpp.
    command = [sys.executable, script, "--hu", "--ue-root", ue_root]
    if args.compiler:
        command.extend(("--compiler", args.compiler))
    for pass_number in (1, 2):
        print(f"Running UE HMake generator pass {pass_number}/2", flush=True)
        subprocess.run(command, cwd=ue_root, check=True)


if __name__ == "__main__":
    main()
