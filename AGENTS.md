# Repository Guidelines

## Project Structure & Module Organization

BASTION is a Linux desktop tower defense game using C++23, Bazel, and raylib.
Keep simulation rules in `src/game.h` and `src/game.cc`; they must remain
independent of graphics and windowing. `src/main.cc` owns rendering, input,
camera controls, and UI. Behavior tests live in `tests/game_test.cc`.
`third_party/raylib.BUILD.bazel` integrates the pinned raylib source archive.
Models and effects are procedural; there is currently no asset directory.

## Build, Test, and Development Commands

- `bazel build //:tower_defense`: compile the desktop game.
- `bazel run //:tower_defense`: open the interactive game.
- `bazel test //...`: run all headless simulation tests.
- `xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 bazel run //:tower_defense -- --smoke-test --screenshot /tmp/bastion.png`:
  render 300 frames and capture a screenshot without a physical display.

Use Bazel 8.1.1 and GCC 13 or newer. Consult `README.md` for Linux graphics
packages. Keep C++23 enabled in `.bazelrc`; dependency C sources compile as C99.

## Coding Style & Naming Conventions

Use four-space indentation, braces, and explicit ownership. Name types in
`PascalCase`, functions and variables in `snake_case`, and constants with a `k`
prefix. Keep simulation declarations in namespace `bastion`. Prefer standard
containers, ranges, and `std::expected` for fallible placement. Do not introduce
graphics dependencies into simulation code. No formatter or linter is configured;
match the surrounding style and keep `-Wall -Wextra -Wpedantic` builds clean.

## Testing Guidelines

The test executable uses explicit checks that remain active in optimized builds;
no external test framework or coverage threshold is configured. Name behavior
groups `test_<behavior>`. Cover economy invariants, targeting, wave transitions,
and terminal states when changing gameplay. Run `bazel test //...` before handing
off changes. For rendering/input changes, also run the graphical smoke test and
manually check the affected interaction.

## Commit & Pull Request Guidelines

This workspace began without Git history, so no existing commit convention is
established. Use concise imperative subjects such as `Fix mortar splash targeting`.
Describe player-visible behavior, implementation tradeoffs, and validation in
pull requests. Link related issues when available and include screenshots for
visual changes. Avoid committing Bazel output directories or local screenshots.
