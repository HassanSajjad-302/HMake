# Example 11: ball_pit modules

This example builds [cdacamar/ball_pit](https://github.com/cdacamar/ball_pit), the project used in
[Moving a Project to C++ Named Modules](https://devblogs.microsoft.com/cppblog/moving-a-project-to-cpp-named-modules/).
The external sources are intentionally not vendored.

From this `Examples/Example11` directory, clone the dependency beside `hmake.cpp`:

```bash
git clone https://github.com/cdacamar/ball_pit --recursive --shallow-submodules
```

Then configure and build it in one command:

```bash
hbuild -S . -B Build
```

This specification enables `IsCppMod::YES`, so select a named toolchain compatible with
HMake's module support.
