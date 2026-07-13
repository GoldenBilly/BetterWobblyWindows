#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

ProjectRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BuildDir="${ProjectRoot}/cmake-build-release"
InstallPrefix="${CMAKE_INSTALL_PREFIX:-/usr}"
BuildType="${CMAKE_BUILD_TYPE:-Release}"
AutoInstallDependencies="${BETTER_WOBBLY_AUTO_INSTALL_DEPS:-0}"
InstallProject=0
RunTests=1
CleanBuildDir=1
BuildPlugin=1

Usage() {
    cat <<USAGE
Usage: ./build.sh [options]

Default behavior:
  ./build.sh                 configure, build, and run tests only; it does not install anything

Install explicitly:
  ./build.sh --install       configure, build, test, then install
  ./install.sh               same as ./build.sh --install

Options:
  --install, install         install after a successful build/test run
  --install-deps            try to install missing build dependencies if configure fails
  --no-tests                 skip ctest
  --tests-only               build only lightweight tests (no KWin/KF6 SDK needed)
  --keep-build-dir           reuse the existing cmake-build-release directory
  --prefix PATH              set CMAKE_INSTALL_PREFIX for this run
  --build-type TYPE          set CMAKE_BUILD_TYPE for this run, for example Release or Debug
  -h, --help                 show this help

Environment:
  CMAKE_INSTALL_PREFIX       default install prefix when --prefix is not used; default /usr
  CMAKE_BUILD_TYPE           default build type when --build-type is not used; default Release
  BETTER_WOBBLY_AUTO_INSTALL_DEPS=1 enables automatic build dependency installation
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --install|install)
            InstallProject=1
            shift
            ;;
        --install-deps)
            AutoInstallDependencies=1
            shift
            ;;
        --no-tests)
            RunTests=0
            shift
            ;;
        --tests-only)
            BuildPlugin=0
            RunTests=1
            shift
            ;;
        --keep-build-dir)
            CleanBuildDir=0
            shift
            ;;
        --prefix)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --prefix." >&2
                exit 2
            fi
            InstallPrefix="$2"
            shift 2
            ;;
        --prefix=*)
            InstallPrefix="${1#--prefix=}"
            shift
            ;;
        --build-type)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --build-type." >&2
                exit 2
            fi
            BuildType="$2"
            shift 2
            ;;
        --build-type=*)
            BuildType="${1#--build-type=}"
            shift
            ;;
        -h|--help)
            Usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            Usage >&2
            exit 2
            ;;
    esac
done

RunAsRoot() {
    if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "Need root privileges for cmake --install because the install prefix is ${InstallPrefix}." >&2
        exit 1
    fi
}

ConfigureProject() {
    local BuildTesting="ON"
    if [[ "${RunTests}" == "0" ]]; then
        BuildTesting="OFF"
    fi

    cmake -S "${ProjectRoot}" -B "${BuildDir}" \
        -DCMAKE_BUILD_TYPE="${BuildType}" \
        -DCMAKE_INSTALL_PREFIX="${InstallPrefix}" \
        -DBUILD_TESTING="${BuildTesting}" \
        -DBETTER_WOBBLY_BUILD_PLUGIN="${BuildPlugin}"
}

if [[ "${CleanBuildDir}" == "1" ]]; then
    rm -rf "${BuildDir}"
elif [[ -f "${BuildDir}/CMakeCache.txt" ]]; then
    CachedSourceDir="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "${BuildDir}/CMakeCache.txt" | head -n 1)"
    if [[ -n "${CachedSourceDir}" && "$(realpath -m "${CachedSourceDir}")" != "$(realpath -m "${ProjectRoot}")" ]]; then
        echo "Discarding non-portable CMake cache from ${CachedSourceDir}."
        rm -rf "${BuildDir}"
    fi
fi

if ! ConfigureProject; then
    if [[ "${AutoInstallDependencies}" == "1" ]]; then
        "${ProjectRoot}/scripts/InstallDependencies.sh"
        rm -rf "${BuildDir}"
        ConfigureProject
    else
        exit 1
    fi
fi

cmake --build "${BuildDir}" -j"$(nproc)"

if [[ "${RunTests}" == "1" ]]; then
    ctest --test-dir "${BuildDir}" --output-on-failure
fi

RefreshKServiceCache() {
    if command -v kbuildsycoca6 >/dev/null 2>&1; then
        kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
    fi
}

AskKWinToReconfigure() {
    local qdbus_cmd=""
    if command -v qdbus6 >/dev/null 2>&1; then
        qdbus_cmd="qdbus6"
    elif command -v qdbus >/dev/null 2>&1; then
        qdbus_cmd="qdbus"
    fi

    if [[ -n "${qdbus_cmd}" ]]; then
        "${qdbus_cmd}" org.kde.KWin /Effects org.kde.kwin.Effects.reconfigure >/dev/null 2>&1 || true
    fi
}

PrintInstallHint() {
    cat <<HINT
Installed Better Wobbly Windows to ${InstallPrefix}.

If System Settings was already open, close and reopen it. If the effect is still
missing, log out/in or restart KWin, then run:
  ./scripts/CheckInstallation.sh
HINT
}

if [[ "${InstallProject}" == "1" ]]; then
    RunAsRoot cmake --install "${BuildDir}"
    RefreshKServiceCache
    AskKWinToReconfigure
    PrintInstallHint
else
    echo "Build completed. Nothing was installed. Use './build.sh --install' or './install.sh' to install."
fi
