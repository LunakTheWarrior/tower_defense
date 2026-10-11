# BASTION // Last Light

A standalone 3D tower defense game written in **C++23**, built with **Bazel**,
and rendered with [raylib 5.5](https://github.com/raysan5/raylib/releases/tag/5.5).
Defend a floating island's energy crystal through ten escalating waves.
All models and effects are generated in code; no external art assets are required.

## Build and play

The current build targets **Linux with X11 (or XWayland) and OpenGL 3.3**.
Use GCC 13 or newer with a C++23 standard library supporting `std::expected`.
Bazel 8.1.1 is pinned in `.bazelversion`; Bazelisk can select it automatically.
The initial build needs network access to fetch pinned dependencies.

On Ubuntu/Debian, install the compiler and graphics development packages:

```sh
sudo apt install g++ libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libgl1-mesa-dev
```

With Bazel or Bazelisk installed:

```sh
bazel build //:tower_defense
bazel run //:tower_defense
bazel test //...
```

No system raylib installation is needed. Bazel downloads its source using a
pinned SHA-256 checksum. The simulation tests do not require a display.

### Build troubleshooting

`.bazelrc` imports the toolchain settings in
[`tools/toolchains/gcc.bazelrc`](tools/toolchains/gcc.bazelrc). They set
`CC=/usr/bin/gcc` and `CXX=/usr/bin/g++` through `--repo_env`, so Bazel's compiler
discovery bypasses Fedora's `/usr/lib64/ccache/gcc` wrapper, regardless of `PATH`
or the shell's `CC` setting. This avoids ccache's read-only sandbox error.
Normal `bazel build`, `bazel run`, and `bazel test` commands apply these settings
automatically; Bazel's own build cache remains enabled.

For a GCC installation at another location, override the repository settings:

```sh
bazel run --repo_env=CC=/opt/gcc/bin/gcc --repo_env=CXX=/opt/gcc/bin/g++ //:tower_defense
```

Warnings that `bazel-bin`, `bazel-out`, or other convenience links already exist
are separate from compiler failures. Copied files or directories can occupy
these names. Leave their contents intact and use a different link prefix:

```sh
bazel run --symlink_prefix=.bazel-links/bazel- //:tower_defense
```

The alternate link directory is ignored by Git. Avoid copying `bazel-*` build
outputs when moving the project between machines.

## Play

Start with 260 credits and 20 core integrity. Select a blueprint and click a
grass tile to build. The sandy route must remain clear. Launch waves when ready;
you may also build, upgrade, and sell during combat or while paused.

| Tower | Cost | Role |
| --- | ---: | --- |
| Pulse | 70 | Rapid single-target damage |
| Mortar | 115 | Splash damage against clustered enemies |
| Frost | 90 | Slows enemies to 45% speed for 2.2 seconds |

Towers target the furthest enemy along the route within range. Each tower has
three upgrade levels; upgrades increase damage, range, and fire rate. Selling
returns 70% of the total investment. Earn credits from kills and wave bonuses.
Titans arrive on waves 5 and 10 and cost five integrity if they reach the core.
Survive wave 10 to win; zero integrity ends the game.

| Control | Action |
| --- | --- |
| `1`, `2`, `3` | Select pulse, mortar, frost |
| Left click | Place a tower or inspect an existing one |
| `U` / `X` | Upgrade / sell selected tower |
| `Space` | Start next wave |
| `P` / `F` | Pause / toggle 2x speed |
| `Q`, `E` / mouse wheel | Orbit / zoom |
| Right click / `Esc` | Cancel placement or selection |
| `H` | Open or close the manual (pauses simulation) |
| `R` | Restart immediately |
| Window close | Quit |

Cover bends with pulse towers, use frost to hold enemies in mortar range,
and invest wave bonuses before launching the next wave.

## Validation

```sh
bazel test //:simulation_test
# Optional software-rendered smoke test (requires xvfb and xauth):
xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 bazel run //:tower_defense -- \
  --smoke-test --screenshot /tmp/bastion.png
```

The smoke test places three towers, starts a wave, renders 300 frames, and exits.
Without `--smoke-test`, the window stays interactive until closed.

## Layout

- `src/game.h`, `src/game.cc`: graphics-independent simulation and economy.
- `src/main.cc`: procedural 3D rendering, picking, camera, and desktop UI.
- `tests/game_test.cc`: headless behavior tests with a small explicit-check harness.
- `third_party/`: Bazel integration for raylib's bundled GLFW/X11 backend.

Audio and save files are not implemented. Windows/macOS build definitions are
not included in this Linux version.
