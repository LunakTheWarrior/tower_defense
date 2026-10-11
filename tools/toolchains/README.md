# Linux x86_64 GCC toolchain

The default is a registered native Bazel `cc_toolchain` with explicit Linux
x86_64 execution and target constraints. It builds against system libc,
libstdc++, OpenGL, and X11, allowing the executable to use the host GPU drivers.
Run it from a normal terminal with Bazelisk; Nix is optional.

## Compiler selection

`repositories.bzl` prefers GCC 16, then 15, 14, 13, and the system GCC (minimum
13). It checks absolute `/usr/bin` paths, resolves symlinks, and rejects ccache
and Nix store paths. It validates the target triple and compiles a C++23
`std::expected` probe before configuring the build. Compiler companions and
binutils are resolved explicitly; shell `CC`, `CXX`, and `PATH` do not select
Fedora's ccache wrappers.

Override the native compiler when needed:

```sh
bazelisk test --repo_env=BASTION_GCC=/usr/bin/gcc-16 //...
```

Install its matching C++ development libraries and compiler companions.

## Components and policy

`MODULE.bazel` registers `@bastion_gcc//:toolchain`. The repository rule probes
C/C++ header roots and generates a manifest. `cc_toolchain_config.bzl` creates
`CcToolchainConfigInfo` using the pinned rules_cc Unix implementation for
archives, dependency files, PIC, response files, and coverage. This is a private
rules_cc API; review compatibility when updating that dependency.

| Setting | Policy |
| --- | --- |
| Architecture | `-march=x86-64 -mtune=generic` |
| Languages | C++23; GNU C99 for dependency C sources |
| Release | `-O2 -DNDEBUG`, fortified libc calls, section GC |
| Debug | `-O0 -g3`, libstdc++ assertions |
| Hardening | Strong stack protection, RELRO, immediate binding |
| Reproducibility | Date/time macros redacted; SHA-1 build IDs |

This is a local SDK toolchain. Remote workers require the same tools and headers
at the same paths; the generated manifest does not package those inputs.

## Verification

```sh
bazelisk test //...
bazelisk test -c dbg //:simulation_test //:toolchain_test
bazelisk aquery 'mnemonic("CppCompile", deps(//:tower_defense))'
```

Compile actions should use a real system GCC, never `/usr/lib64/ccache/gcc`.
`toolchain_test` verifies GCC 13+, C++23, C99, the 64-bit ABI, and mixed-language
linking. `ldd bazel-bin/tower_defense` should show system runtime libraries;
GLFW loads the host OpenGL driver dynamically when opening the window.

## Optional pinned GCC 16 SDK

`flake.nix` and `flake.lock` pin GCC 16.2.0, binutils, libc, graphics development
libraries, and Bazel 8.7.0. The shell exports a validated JSON manifest as
`BASTION_GCC_TOOLCHAIN`; this SDK is selected only by `--config=nix`:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:.
bazelisk test --config=nix //...
```

Nix builds use Nix runtime libraries and require compatible graphics driver
integration to render with a GPU. Use the default native build for the normal
desktop executable. The optional `software` shell is for headless checks.

When updating the SDK, change the Nixpkgs revision, regenerate `flake.lock`,
check compiler validation, and run release/debug tests. Keep `.bazelversion`
aligned with the pinned Bazel package. Nix remote workers need the same store
closure. Native toolchains remain independent of this optional SDK.
