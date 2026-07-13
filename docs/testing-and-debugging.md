# Testing and debugging

## Run all tests

```bash
./build.sh
```

The script creates `cmake-build-release`, builds the plugin and tests, then runs CTest.

To run only the lightweight tests without the KWin and KF6 development packages:

```bash
./build.sh --tests-only
```

To rerun CTest after a build:

```bash
ctest --test-dir cmake-build-release --output-on-failure
```

## Test coverage

The lightweight suite covers:

- embedded blur math and color transforms;
- blur geometry and transition limits;
- default wobble parameter sets;
- Better Blur DX API V1 request and result validation;
- trace helpers and trace environment handling;
- constant ranges and relationships.

These tests do not replace testing inside KWin. Final validation should include moving and resizing windows, screen-edge behavior, fractional scaling, multiple monitors, provider handoff, policy exclusions, and the embedded fallback.

## Trace output

Trace helpers are implemented in `src/Plugin/TraceUtilities.cpp`. Enable the project trace environment variable before starting KWin when detailed effect messages are needed. Because KWin owns the process, restart KWin after changing the variable.

## Installation checks

After installation, run:

```bash
./scripts/CheckInstallation.sh
```

If the effect does not appear:

1. confirm that the install command completed successfully;
2. reopen System Settings;
3. run `kbuildsycoca6 --noincremental`;
4. restart KWin or log out and back in;
5. check that the plugin was installed under the selected CMake prefix.

## Build failures

A normal plugin build needs Qt 6, Extra CMake Modules, KF6 Config, KF6 CoreAddons, KF6 KCMUtils, and KWin development files. The helper can install dependencies for supported package managers:

```bash
./build.sh --install-deps
```

If only the regression tests are needed, use `--tests-only` to avoid the full Plasma SDK.
