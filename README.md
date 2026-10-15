# Raylib Feature Lab

A multi-scene desktop application built with **raylib 6.0** and **C11** that showcases a wide range of raylib's capabilities — 2D rendering, 3D worlds, audio synthesis, interactive painting, and a breakout game — all in a single, tabbed window.

![Language](https://img.shields.io/badge/C11-blue) ![raylib](https://img.shields.io/badge/raylib-6.0-orange) ![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Running](#running)
- [Controls](#controls)
- [Dusk Hopper (2D Platformer)](#dusk-hopper-2d-platformer)
- [Component Library](#component-library)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Adding a New Scene](#adding-a-new-scene)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

**Raylib Feature Lab** is a playground for experimenting with [raylib](https://www.raylib.com/) features. The application is organized around *scenes* — self-contained mini-programs that share a common toolbar and status bar (the "chrome"). Users switch between scenes using on-screen buttons or number keys.

All audio is **procedurally synthesized at runtime** — no external asset files are required beyond the bundled raylib library.

---

## Features

| Scene | What It Demonstrates |
|---|---|
| **1 — Shapes** | `Camera2D`, panning, zooming, splines, polygons, blend modes, animated geometry, user-spawned bouncing blobs |
| **2 — Breakout** | Classic brick-breaker game with collision detection, scoring, lives, and synthesized sound effects |
| **3 — Paint** | `RenderTexture2D`-based canvas, soft-brush stamps with interpolated strokes, HSV hue picker |
| **4 — Piano** | Procedural audio via `Wave`/`Sound`, exponential-decay envelopes, clickable on-screen keyboard, animated glow bars |
| **5 — World** | `Camera3D` (orbital + first-person), GLSL Blinn-Phong shader, heightmap terrain, collectible orbs |

**Global features:** resizable window, F11 borderless fullscreen, F12 screenshot, master volume control, file-drop detection, toast notifications.

---

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| **CMake** | ≥ 3.20 | Build system generator |
| **MSVC** (Visual Studio Build Tools) | 2019+ (v142 / v143) | The bundled `raylib.lib` was compiled with MSVC 16 |
| **Windows 10/11 x64** | — | Currently only a Win64 MSVC build is bundled |

> [!NOTE]
> The raylib 6.0 pre-built library is already included under `third_party/`. No separate raylib installation is needed.

---

## Building

```powershell
# Configure (Release is recommended — matches the bundled library's CRT)
cmake -S . -B build -A x64

# Build
cmake --build build --config Release
```

The executable is produced at `build/Release/feature_lab.exe`.

### Build with a specific Visual Studio generator

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

## Running

```powershell
.\build\Release\feature_lab.exe
```

The window opens at **1280 × 720** (resizable, minimum 960 × 600).

---

## Controls

### Global

| Key | Action |
|---|---|
| `1` – `5` | Switch to scene 1–5 |
| `F11` | Toggle borderless fullscreen |
| `F12` | Save screenshot (`feature_lab.png`) |
| `+` / `-` | Adjust master volume |
| Drag-and-drop | Displays the dropped filename in the status bar |

### Scene 1 — Shapes

| Input | Action |
|---|---|
| Left click (play area) | Spawn a bouncing shape |
| Right drag | Pan the 2D camera |
| Mouse wheel | Zoom in / out |
| `R` | Reset camera and clear all shapes |

### Scene 2 — Breakout

| Input | Action |
|---|---|
| Mouse movement | Move paddle |
| Left click / `Space` | Launch ball |
| `R` | Restart the game |

### Scene 3 — Paint

| Input | Action |
|---|---|
| Left drag (canvas) | Draw with the current brush |
| Mouse wheel / `[` `]` | Decrease / increase brush size |
| `←` `→` arrows | Shift hue left / right |
| Click hue strip | Jump to a specific hue |
| `C` | Clear canvas |

### Scene 4 — Piano

| Input | Action |
|---|---|
| `A S D F G H J K` | White keys (C through C5) |
| `W E T Y U` | Black keys (sharps/flats) |
| Left click on keys | Play notes via mouse |

### Scene 5 — World

| Input | Action |
|---|---|
| `V` | Toggle between orbital and first-person camera |
| `W A S D` + mouse (FP mode) | Walk and look around |
| Mouse wheel (orbit mode) | Zoom |
| Walk into gold orbs (FP mode) | Collect them |

---

## Dusk Hopper (2D Platformer)

A full standalone 2D platformer game (`platformer.exe`) demonstrating tilemap collisions, moving platforms, squash & stretch animations, stompable enemies, and coyote time.

```powershell
.\build\Release\platformer.exe
```

| Input | Action |
|---|---|
| `A` / `D` or `←` / `→` | Run left / right |
| `Space` / `W` / `Up` / `Z` | Jump (hold for higher jump) |
| `R` | Restart level |
| `Esc` | Pause / resume |

---

## Card Press (Business Cards)

A customer-facing designer for a standard US business card (3.5 × 2 inches). Place text and images on the front or back, start from a template, then export print files.

```powershell
.\build\Release\cardpress.exe
```

The executable is `build/Release/cardpress.exe` after the same CMake build as Feature Lab.

| Input | Action |
|---|---|
| Left drag | Move the selected text or image |
| Corner handles | Resize |
| Type | Edit the selected text |
| `Esc` | Leave the text cursor so arrow keys nudge the block |
| Mouse wheel | Zoom the preview |
| Middle drag, or Space drag | Pan |
| `Delete` | Remove the selection |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+S` / `Ctrl+O` | Save / open a `.card` project |
| Drag and drop | Add an image, or open a `.card` file |

**Export card PNG** writes a 300 DPI print-shop file (4.25 × 2.75 in) with 0.125 in bleed and crop marks. **Export letter sheet** writes a 300 DPI letter page of 10 cards on the Avery 8371 grid, with cut ticks in the outer margins. Guides on screen (red bleed, black trim, blue safe zone) are not printed.

---

## Component Library

The project extracts core functionality into modular, reusable components located in `lib/`:

- [`cam2d`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/cam2d.h) — 2D viewport pan/zoom camera controller
- [`canvas`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/canvas.h) — `RenderTexture2D` painting canvas with soft brush & stroke interpolation
- [`lighting`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/lighting.h) — Embedded Blinn-Phong GLSL 330 lighting shader pipeline
- [`particles`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/particles.h) — Bouncing particle/blob pool with boundary physics
- [`platformer2d`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/platformer2d.h) — Tile-based platformer engine with kinematic physics, moving platforms, enemies, and level progression
- [`synth`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/synth.h) — Waveform audio synthesis and frequency calculation
- [`toast`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/toast.h) — Timed notification popups
- [`scene`](file:///c:/Users/Jonathan/Downloads/Raylib_Experiments/lib/scene.h) — Generic scene state machine

For complete component API reference and code examples, see [`docs/COMPONENTS.md`](docs/COMPONENTS.md).

---

## Architecture

### Application Lifecycle

```
main()
  ├── InitWindow / InitAudioDevice
  ├── Init subsystems: AudioBank, Paint, Breakout, World, Shapes
  ├── Main loop
  │     ├── UpdateChrome()       — global input, scene switching, volume, file drops
  │     ├── Update<Scene>()      — per-scene logic
  │     ├── BeginDrawing()
  │     │     ├── Draw<Scene>()  — per-scene rendering
  │     │     └── DrawChrome()   — toolbar, status bar, toast
  │     └── EndDrawing()
  └── ShutdownApp / CloseAudioDevice / CloseWindow
```

### Core Data Structures

| Type | Purpose |
|---|---|
| `App` | Monolithic application state — holds all scene data, audio handles, shaders, textures, and UI state |
| `Blob` | A bouncing circle in the Shapes scene (position, velocity, radius, color) |
| `Brick` | A single brick in the Breakout scene (alive flag + color) |
| `SceneId` | Enum identifying the active scene |

### Key Design Decisions

- **Single-file architecture**: The entire application lives in `src/main.c` (~1050 lines). This is intentional for a learning/experimentation project — everything is visible in one place.
- **No external assets**: All textures are generated with `GenImage*`, all audio with `MakeTone()`, and shaders are compiled from inline strings.
- **Static raylib linkage**: The CMake build imports `raylib.lib` as a `STATIC IMPORTED` target and links the required Windows system libraries.

### Shader Pipeline (World Scene)

The World scene uses a custom **Blinn-Phong** fragment shader (GLSL 330):

- **Vertex shader** passes world-space position, texture coordinates, vertex color, and transformed normals to the fragment stage.
- **Fragment shader** computes diffuse + specular lighting from a single point light that orbits the scene over time.
- The `viewPos` and `lightPos` uniforms are updated each frame from C.

---

## Project Structure

```
Raylib_Experiments/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── CONTRIBUTING.md             # Contribution guidelines
├── .gitignore                  # Git ignore rules
│
├── src/                        # Application source code
│   └── main.c                  # Single-file application (all scenes)
│
├── include/                    # Project headers (for future use)
│   └── .gitkeep
│
├── assets/                     # Runtime assets (for future use)
│   ├── textures/
│   ├── sounds/
│   ├── shaders/
│   │   ├── vertex/
│   │   └── fragment/
│   └── models/
│
├── third_party/                # Vendored dependencies
│   └── raylib-6.0_win64_msvc16/
│       ├── include/            # raylib.h, raymath.h, rlgl.h
│       ├── lib/                # raylib.lib, raylib.dll, raylibdll.lib
│       ├── CHANGELOG
│       ├── LICENSE
│       └── README.md
│
├── docs/                       # Extended documentation (for future use)
│   └── .gitkeep
│
├── build/                      # CMake build output (generated, not committed)
│
└── tests/                      # Test programs (for future use)
    └── .gitkeep
```

---

## Adding a New Scene

1. **Define the scene ID** — add an entry to the `SceneId` enum (before `SCENE_COUNT`).
2. **Name and help text** — add corresponding strings to `SCENE_NAMES[]` and `SCENE_HELP[]`.
3. **Add state** — extend the `App` struct with any data the new scene needs plus a `bool <scene>Ready` flag.
4. **Implement the functions**:
   - `Init<Scene>(App *app)` — called once at startup.
   - `Update<Scene>(App *app, float dt)` — called every frame when the scene is active.
   - `Draw<Scene>(App *app)` — renders the scene inside `PlayArea()`.
5. **Wire it in** — add `case` branches to both `switch` blocks in `main()`.
6. **Clean up** — add teardown logic in `ShutdownApp()`.

> [!TIP]
> Use `PlayArea()` to get the drawable rectangle between the toolbar and status bar. Use `PointInPlay()` to gate mouse input so clicks on the chrome don't bleed into your scene.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for full guidelines. The short version:

1. Fork & branch from `main`
2. Follow the existing code style (C11, 4-space indent, `PascalCase` functions, `camelCase` fields)
3. Keep scenes self-contained — each scene should initialize and clean up its own resources
4. Test on a clean build before opening a PR

---

## License

This project is for personal experimentation. The bundled raylib library is licensed under the [zlib/libpng license](third_party/raylib-6.0_win64_msvc16/LICENSE).
