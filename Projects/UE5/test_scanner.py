#!/usr/bin/env python3

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import scanner  # noqa: E402


class ScannerMetadataTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def write(self, relative_path: str, content: str) -> Path:
        file = self.root / relative_path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(content, encoding="utf-8")
        return file.resolve()

    def test_blank_first_line_uses_filename_defaults(self) -> None:
        file = self.write("Core.hmake.hpp", "\n// Ordinary C++ comment.\nvoid specify() {}\n")

        parsed = scanner.parse_ue_file(file)

        self.assertEqual(parsed.logical_name, "Core")
        self.assertEqual(parsed.kind, "Module")
        self.assertEqual(parsed.configuration, "Default")
        self.assertIsNone(parsed.platform)
        self.assertIsNone(parsed.platform_group)

    def test_parses_typed_metadata_and_block_comments(self) -> None:
        file = self.write(
            "CoreLinux.hmake.hpp",
            "/* Scanner-only explanation.\n"
            " * Ordinary line comments are assignments in this block.\n"
            " */\n"
            "// name = Core\n"
            "// kind = Module\n"
            "// platformGroup = Unix\n"
            "// configuration = RttiExcept\n"
            "\n"
            "void specify() {}\n",
        )

        parsed = scanner.parse_ue_file(file)

        self.assertEqual(parsed.logical_name, "Core")
        self.assertEqual(parsed.kind, "Module")
        self.assertEqual(parsed.configuration, "RttiExcept")
        self.assertEqual(parsed.platform_group, "Unix")

    def test_rejects_invalid_metadata(self) -> None:
        cases = {
            "missing-boundary.hmake.hpp": "// kind = Module\nvoid specify() {}\n",
            "unknown-key.hmake.hpp": "// targetKind = Module\n\nvoid specify() {}\n",
            "duplicate-key.hmake.hpp": "// kind = Module\n// kind = Module\n\nvoid specify() {}\n",
            "line-comment.hmake.hpp": "// Explanation, not an assignment.\n\nvoid specify() {}\n",
            "two-selectors.hmake.hpp": (
                "// platform = Linux\n// platformGroup = Unix\n\nvoid specify() {}\n"
            ),
            "non-module-profile.hmake.hpp": (
                "// kind = Prebuilt\n// configuration = RttiExcept\n\nvoid specify() {}\n"
            ),
            "unterminated-comment.hmake.hpp": "/* explanation\n\nvoid specify() {}\n",
        }
        for name, content in cases.items():
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    scanner.parse_ue_file(self.write(name, content))

    def test_rejects_semantic_filename_suffix(self) -> None:
        file = self.write("Core.module.hmake.hpp", "\nvoid specify() {}\n")

        with self.assertRaisesRegex(ValueError, "semantic filename suffixes"):
            scanner.parse_ue_file(file)

    def test_scan_rejects_duplicate_and_specialization_without_base(self) -> None:
        self.write("one/Core.hmake.hpp", "\nvoid specify() {}\n")
        self.write("two/Core.hmake.hpp", "\nvoid specify() {}\n")
        with self.assertRaisesRegex(ValueError, "Duplicate UE registration"):
            scanner.scan(self.root)

        for file in self.root.rglob("*.hmake.hpp"):
            file.unlink()
        self.write(
            "CoreLinux.hmake.hpp",
            "// name = Core\n// platform = Linux\n\nvoid specify() {}\n",
        )
        with self.assertRaisesRegex(ValueError, "no base registration"):
            scanner.scan(self.root)

    def test_generated_entry_uses_dynamic_producer_configuration(self) -> None:
        files = [
            scanner.UeFile(self.write("Core.hmake.hpp", "\nvoid specify() {}\n"), "Core"),
            scanner.UeFile(
                self.write(
                    "ImageWrapper.hmake.hpp",
                    "// configuration = RttiExcept\n\nvoid specify() {}\n",
                ),
                "ImageWrapper",
                configuration="RttiExcept",
            ),
        ]
        command = {
            "platform": "Linux",
            "architecture": "x64",
            "buildConfiguration": "Debug",
            "targetType": "Server",
            "cppCompileCommand": "clang++ -x c++ -DIS_PROGRAM=0 -DUE_BUILD_DEBUG=1 ",
            "cCompileCommand": "clang++ -x c -DIS_PROGRAM=0 -DUE_BUILD_DEBUG=1 ",
            "linkCommand": "clang++ -o \"",
            "linkDependenciesPrefix": "-Wl,--start-group ",
            "linkCommandSuffix": "-Wl,--end-group ",
            "archiveCommand": "llvm-ar rcs \"",
        }

        generated = scanner.generate(files, [command], self.root)

        self.assertIn("configuration.createProducerConfigurations();", generated)
        self.assertIn("configuration.finalizeProducerConfigurations();", generated)
        self.assertIn('getUeConfiguration("UnrealServerLinuxDebug")', generated)
        self.assertEqual(generated.count("getUeConfiguration("), 1)


if __name__ == "__main__":
    unittest.main()
