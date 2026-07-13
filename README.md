# Better Wobbly Windows

Better Wobbly Windows is a KWin effect for KDE Plasma 6 that makes windows move and resize smoothly with a soft wobble instead of behaving like rigid rectangles.

This is still very much experimental, so expect stutters, freezing, etc. Though, it shouldn't crash your kde plasma.

It also keeps the blur attached to the deformed window. When Custom Better Blur DX is available, the effect can use its renderer through API V1. Otherwise, it uses the built-in blur fallback.

See [Custom Better Blur DX implementation for this plugin](MUST-FIX-LINK)

Recommended way of using this effect is to install custom implementation of BBDX.

Otherwise expect freezes/flushes/visual artifacts of fallback mode.

## Minimal Requirements

- KDE Plasma 6 and KWin
- CMake 3.16 or newer
- A C++20 compiler
- Qt 6 development files
- KDE Frameworks 6 development files
- Extra CMake Modules
- KWin development files

## Tested on

- PlasmaShell / KWin 6.7.2 (Wayland)
- NVIDIA GeForce RTX 3050 (driver version 610.43.02) / Ryzen 5600G / 64 Gb RAM
- Arch Linux
- GCC 16.1.1 20260625
- CMake 4.3.4

## Build

The build script configures the project, compiles it, and runs the tests. It does not install anything by default.

```bash
./build.sh
```

To let the script install missing build dependencies on supported distributions:

```bash
./build.sh --install-deps
```

To build only the lightweight tests:

```bash
./build.sh --tests-only
```

## Install

```bash
./install.sh
```

Or build and install in one command:

```bash
./build.sh --install
```

After installation, enable **Better Wobbly Windows** in KDE System Settings under Desktop Effects. Reopen System Settings or restart KWin if the effect does not appear immediately.

## Documentation

Technical documentation is available in [`docs/`](docs/README.md).

## Authors

- va111222666 / goldenbilly / Va126
- Based on KDE KWin Wobbly Windows by Cédric Borgese, Lucas Murray, Vlad Zahorodnii, and other KWin contributors
- Force Blur / Better Blur authors: KWin development team, taj_ny, DaddelZeit, and iGerman00
- Better Blur DX and KWin Blur contributors, including Xarblu

See [`AUTHORS.md`](AUTHORS.md) and [`NOTICE.md`](NOTICE.md) for the full credits and copyright information.

## License

Licensed under **GPL-3.0-or-later**. See [`LICENSE.txt`](LICENSE.txt).


## Known Issues

- Blur cuts off on the edges of the screen
- Some advanced settings may break the effect
- May not work properly on multi-monitor setup (?)
- Huge CPU/GPU usage if Wobbly Windows uses it's own embedded blur
- In Desktop (Workspace) Selector, blur is not visible
