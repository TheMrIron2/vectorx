# VectorX

A 3D vector rail shooter.

This is a final year project for my TU856 computer science degree.

## Overview

VectorX is a new 3D rail shooter inspired by futuristic 80s aesthetics and feel. Unlike modern 3D games that rely on textured polygons and detailed environments, VectorX embraces an alternative solution, using vector visuals to evoke a sci-fi feel.

The project will be built on a clean and portable OpenGL engine designed specifically for it.

Run `build/debug/vectorx.exe` or `.\run.ps1` to play.

| Control | Action |
| --- | --- |
| WASD / arrow keys | Move |
| Space (hold) | Fire lasers |
| F | Toggle borderless fullscreen |
| R | Recenter the ship and clear projectiles |
| P | Pause / resume |
| G | Toggle glow |
| Escape | Exit |

Glow is controlled by `VxRenderSettings` in `src/core/render_settings.h` and can be adjusted from the command line with eg. `--glow-strength 0.75 --glow-radius 1.2`.

## Build 

```sh
cmake -S . -B build/game -DCMAKE_BUILD_TYPE=Debug
cmake --build build/game --config Debug --parallel 4
ctest --test-dir build/game -C Debug --output-on-failure
```

Run `build/game/vectorx` on Linux/macOS, `build/game/vectorx.exe` with Ninja/MinGW on Windows, or `build/game/Debug/vectorx.exe` with Visual Studio.

## Project structure

```text
src/
  core/       Vector math, projection, clipping, render settings
  game/       Flight, ship geometry, projectile pool, corridor and scene
  platform/   SDL/OpenGL rendering backend and framebuffer capture
  main.c      Window lifecycle, keyboard input, fixed-step loop
tests/        Portable core checks
docs/         Original project brief and prototype notes
```

Game and core code generate a `VxVectorFrame` of 2D line segments with intensities and the platform backend consumes that frame. Another backend can reuse the flight and geometry code.
