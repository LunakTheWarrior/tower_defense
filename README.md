# BASTION // Last Light

A standalone 3D tower defense game written in **C++23**, built with **Bazel**,
and rendered with [raylib 5.5](https://github.com/raysan5/raylib/releases/tag/5.5).
Defend a floating island's energy crystal through ten escalating waves.
All models and effects are generated in code; no external art assets are required.

## Build and play

The build targets **Linux x86_64 with X11 (or XWayland) and OpenGL 3.3**.
Install **Bazelisk**, native **GCC/G++ 13 or newer**, binutils, and the system's
OpenGL/X11 development packages. GCC 16 is preferred when installed; the
registered toolchain selects the newest supported version in `/usr/bin` and
resolves real compiler paths, bypassing Fedora's ccache wrappers.

Ubuntu prerequisites:

```sh
sudo apt install g++ binutils libgl-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev
```

Fedora prerequisites:

```sh
sudo dnf install gcc gcc-c++ binutils libglvnd-devel libX11-devel \
  libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

Build, test, and play directly from your usual terminal:

```sh
bazelisk build //:tower_defense
bazelisk test //...
bazelisk run //:tower_defense
```

Bazelisk downloads the version in `.bazelversion`. The first build also fetches
checksum-pinned dependencies. The native executable links the host's OpenGL
libraries, which load its installed GPU drivers on Ubuntu, Fedora, and other
Linux desktops. No Nix shell or software-rendering override is required. You can
also launch `./bazel-bin/tower_defense` directly after building.

### Toolchain options and troubleshooting

To select a particular native GCC installation:

```sh
bazelisk build --repo_env=BASTION_GCC=/usr/bin/gcc-16 //:tower_defense
```

The optional Nix flake pins a GCC 16.2 SDK for explicit `--config=nix` builds;
see [the toolchain guide](tools/toolchains/README.md). It is separate from the
normal native build and has its own graphics runtime requirements.

Warnings that `bazel-bin`, `bazel-out`, or other convenience links already exist
are separate from compiler failures. Leave existing contents intact and choose
an alternate link prefix:

```sh
bazelisk run --symlink_prefix=.bazel-links/bazel- //:tower_defense
```

Avoid copying `bazel-*` build outputs when moving the project between machines.

## Play

Start with 260 credits and 20 core integrity. Select a blueprint and click a
grass tile to build. The sandy route must remain clear. Launch waves when ready;
you may also build, upgrade, and sell during combat or while paused.

| Tower | Cost | Role |
| --- | ---: | --- |
| Pulse | 70 | Rapid single-target damage |
| Mortar | 115 | Splash damage against clustered enemies |
| Frost | 90 | Slows enemies to 45% speed for 2.2 seconds |
| Rail | 150 | Long-range, heavy shots that bypass Sentinel armor |

Towers target the furthest enemy along the route within range. Each tower has
three upgrade levels; upgrades increase damage, range, and fire rate. Selling
returns 70% of the total investment. Earn credits from kills and wave bonuses.
Titans arrive on waves 5 and 10 and cost five integrity if they reach the core.
Sentinels appear from wave 4, take half damage from other towers, and award 18
credits. Their violet armor rings distinguish them on the battlefield.
Survive wave 10 to win; zero integrity ends the game.

| Control | Action |
| --- | --- |
| `1`, `2`, `3`, `4` | Select pulse, mortar, frost, rail |
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
bazelisk test //:simulation_test
# Verifies native GCC, C++23/C99, the x86_64 ABI, and mixed-language linking:
bazelisk test //:toolchain_test
# Optional headless check; install Xvfb and xauth first:
xvfb-run -a bazelisk run //:tower_defense -- \
  --smoke-test --screenshot /tmp/bastion.png
```

The smoke test places four towers and a Sentinel, starts a wave, renders 300 frames, and exits.
Without `--smoke-test`, the window stays interactive until closed.

## Layout

- `src/game.h`, `src/game.cc`: graphics-independent simulation and economy.
- `src/main.cc`: procedural 3D rendering, picking, camera, and desktop UI.
- `tests/game_test.cc`: headless behavior tests with a small explicit-check harness.
- `third_party/`: Bazel integration for raylib's bundled GLFW/X11 backend.
- `tools/toolchains/`: registered Linux x86_64 GCC toolchain and SDK validation.
- `flake.nix`, `flake.lock`: optional pinned Nix SDK and development shells.

Audio and save files are not implemented. Windows/macOS build definitions are
not included in this Linux version.
