# Example 11: ball_pit modules

This example builds [cdacamar/ball_pit](https://github.com/cdacamar/ball_pit), the project used in
[Moving a Project to C++ Named Modules](https://devblogs.microsoft.com/cppblog/moving-a-project-to-cpp-named-modules/).
The external sources are intentionally not vendored.

From this `Examples/Example11` directory, clone the dependency beside `hmake.cpp`:

```bash
git clone https://github.com/cdacamar/ball_pit --recursive --shallow-submodules
```

Then configure and build it in an out-of-source directory:

```bash
mkdir Build
cd Build
hhelper
hhelper
hbuild
```

The repeated `hhelper` command is expected: the first run creates `cache.json`, while the
second compiles and runs HMake's configure program. This specification enables
`IsCppMod::YES`, so use a compiler/tool cache compatible with HMake's module support.
