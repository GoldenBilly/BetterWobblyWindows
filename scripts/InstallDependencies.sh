#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

RunAsRoot() {
    if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "Need root privileges to install build dependencies, but sudo was not found." >&2
        exit 1
    fi
}

InstallWithApt() {
    RunAsRoot apt-get update
    RunAsRoot apt-get install -y \
        build-essential \
        cmake \
        extra-cmake-modules \
        qt6-base-dev \
        qt6-base-dev-tools \
        libkf6config-dev \
        libkf6coreaddons-dev \
        libkf6kcmutils-dev \
        kwin-dev
}

InstallWithDnf() {
    RunAsRoot dnf install -y \
        gcc-c++ \
        cmake \
        extra-cmake-modules \
        qt6-qtbase-devel \
        kf6-kconfig-devel \
        kf6-kcoreaddons-devel \
        kf6-kcmutils-devel \
        kwin-devel
}

InstallWithPacman() {
    RunAsRoot pacman -Syu --needed --noconfirm \
        base-devel \
        cmake \
        extra-cmake-modules \
        qt6-base \
        kconfig \
        kcoreaddons \
        kcmutils \
        kwin
}

InstallWithZypper() {
    RunAsRoot zypper --non-interactive install \
        gcc-c++ \
        cmake \
        extra-cmake-modules \
        qt6-base-devel \
        kf6-config-devel \
        kf6-coreaddons-devel \
        kf6-kcmutils-devel \
        kwin6-devel
}

InstallDependencies() {
    if command -v apt-get >/dev/null 2>&1; then
        InstallWithApt
    elif command -v dnf >/dev/null 2>&1; then
        InstallWithDnf
    elif command -v pacman >/dev/null 2>&1; then
        InstallWithPacman
    elif command -v zypper >/dev/null 2>&1; then
        InstallWithZypper
    else
        echo "Unsupported package manager. Install CMake, ECM, Qt 6, KF6 Config/CoreAddons/KCMUtils, and KWin development files manually." >&2
        exit 1
    fi
}

InstallDependencies
