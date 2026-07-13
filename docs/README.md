# Better Wobbly Windows documentation

Better Wobbly Windows is a KDE Plasma 6 effect that makes windows bend and wobble while they are moved or resized. The effect replaces the rigid window rectangle with a deformable mesh and keeps the window blur aligned with that mesh.

This directory contains the technical details that do not belong in the main README.

## Documents

- [Architecture](architecture.md) — the main runtime flow and source components.
- [Blur](blur.md) — Better Blur DX API V1 and the embedded fallback.
- [Configuration](configuration.md) — user-facing and advanced settings.
- [Testing and debugging](testing-and-debugging.md) — tests, logging, and common failures.

## Runtime summary

During a move or resize, the effect tracks the window geometry, updates a small spring simulation, and turns the result into a deformed mesh. KWin then paints the window through that mesh.

For blur, Better Wobbly Windows first gives Better Blur DX API V1 a mesh provider. Better Blur DX can render its own cached blur through the same deformation. When no compatible provider renders the blur, the embedded renderer performs the fallback. A provider policy rejection is respected and does not trigger the fallback.

## Important files

| Path | Purpose |
| --- | --- |
| `src/Effect/` | Window lifecycle, paint flow, and mesh generation |
| `src/Physics/` | Spring simulation |
| `src/EmbeddedBlur/` | Built-in blur renderer and related math |
| `src/Configuration/` | Settings and configuration module |
| `src/Plugin/` | KWin plugin factory and trace utilities |
| `include/BetterWobblyWindows/BetterBlurDxApi.h` | Better Blur DX API V1 contract |
| `resources/shaders/` | Blur shaders |
| `tests/` | Lightweight regression tests |
