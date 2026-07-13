#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

EffectId="better_wobblywindows"

printf 'Better Wobbly Windows installation check\n'
printf '=========================================\n'
printf 'Session type: %s\n' "${XDG_SESSION_TYPE:-unknown}"
printf 'KDE session version: %s\n' "${KDE_SESSION_VERSION:-unknown}"
printf 'KWIN_EFFECTS_FORCE_ANIMATIONS: %s\n' "${KWIN_EFFECTS_FORCE_ANIMATIONS:-unset}"
printf 'BETTER_WOBBLY_STRICT_SUPPORT: %s\n' "${BETTER_WOBBLY_STRICT_SUPPORT:-unset}"
printf 'BETTER_WOBBLY_FORCE_SUPPORT: %s\n' "${BETTER_WOBBLY_FORCE_SUPPORT:-unset}"
printf 'BETTER_WOBBLY_BLUR_TRACE: %s\n' "${BETTER_WOBBLY_BLUR_TRACE:-unset}"
printf 'BETTER_WOBBLY_HARDTRACE: %s\n' "${BETTER_WOBBLY_HARDTRACE:-unset}"
printf 'BETTER_WOBBLY_TRACE_FILES: %s\n' "${BETTER_WOBBLY_TRACE_FILES:-unset}"

printf '\nQt plugin roots:\n'
PluginRoots=()
if command -v qtpaths6 >/dev/null 2>&1; then
    while IFS= read -r path; do
        [[ -n "$path" ]] && PluginRoots+=("$path")
        printf '  %s\n' "$path"
    done < <(qtpaths6 --plugin-dir 2>/dev/null || true)
else
    printf '  qtpaths6 not found\n'
fi

printf '\nNative KWin effect libraries found:\n'
SearchRoots=(/usr/lib /usr/lib64 /usr/local/lib "${HOME}/.local/lib")
mapfile -t FoundLibraries < <(
    find "${SearchRoots[@]}" \
        -type f \
        -path '*/qt6/plugins/kwin/effects/plugins/*' \
        \( -name "lib${EffectId}.so" -o -name "*${EffectId}*.so" \) \
        2>/dev/null | sort -u
)

if [[ ${#FoundLibraries[@]} -eq 0 ]]; then
    printf '  none\n'
else
    printf '%s\n' "${FoundLibraries[@]}" | sed 's/^/  /'
fi

printf '\nKCM config modules found:\n'
mapfile -t FoundConfigs < <(
    find "${SearchRoots[@]}" \
        -type f \
        -path '*/qt6/plugins/kwin/effects/configs/*' \
        \( -name "lib${EffectId}_config.so" -o -name "*${EffectId}*_config*.so" \) \
        2>/dev/null | sort -u
)

if [[ ${#FoundConfigs[@]} -eq 0 ]]; then
    printf '  none\n'
else
    printf '%s\n' "${FoundConfigs[@]}" | sed 's/^/  /'
fi

if [[ ${#FoundLibraries[@]} -gt 0 ]]; then
    printf '\nMissing runtime libraries from ldd:\n'
    Missing=0
    for lib in "${FoundLibraries[@]}"; do
        if ldd "$lib" 2>/dev/null | grep -F 'not found'; then
            Missing=1
        fi
    done
    if [[ ${Missing} -eq 0 ]]; then
        printf '  none\n'
    fi

    printf '\nPlugin metadata/IID check:\n'
    PluginInfoCmd=""
    for candidate in qtplugininfo qt6plugininfo; do
        if command -v "$candidate" >/dev/null 2>&1; then
            PluginInfoCmd="$candidate"
            break
        fi
    done
    if [[ -n "$PluginInfoCmd" ]]; then
        for lib in "${FoundLibraries[@]}"; do
            printf '  %s\n' "$lib"
            "$PluginInfoCmd" "$lib" 2>/dev/null | sed 's/^/    /' || true
        done
    else
        printf '  qtplugininfo not found\n'
    fi
fi

printf '\nKWin DBus effect visibility:\n'
QdbusCmd=""
if command -v qdbus6 >/dev/null 2>&1; then
    QdbusCmd="qdbus6"
elif command -v qdbus >/dev/null 2>&1; then
    QdbusCmd="qdbus"
fi
if [[ -n "$QdbusCmd" ]]; then
    if "$QdbusCmd" org.kde.KWin /Effects org.kde.kwin.Effects.loadedEffects 2>/dev/null | grep -i "$EffectId" >/dev/null; then
        printf '  loaded\n'
    else
        printf '  not loaded or DBus method unavailable\n'
    fi
else
    printf '  qdbus6/qdbus not found\n'
fi

printf '\nRecent KWin/plugin log lines:\n'
if command -v journalctl >/dev/null 2>&1; then
    journalctl --user -b --no-pager 2>/dev/null \
        | grep -Ei 'better[_-]wobbly|wobblywindows|kwin.*(plugin|effect)|not matching plugin version|Effect is not supported|KPluginFactory|metadata' \
        | tail -n 100 \
        | sed 's/^/  /' || printf '  none\n'
else
    printf '  journalctl not found\n'
fi

printf '\nRecent dmesg lines from this effect:\n'
if command -v dmesg >/dev/null 2>&1; then
    if ! dmesg --color=never 2>/dev/null \
        | grep -F 'better-wobbly:' \
        | tail -n 100 \
        | sed 's/^/  /'; then
        printf '  unavailable, empty, or permission denied (try sudo dmesg)\n'
    fi
else
    printf '  dmesg not found\n'
fi

printf '\nExpected Wayland native plugin locations:\n'
printf '  .../qt6/plugins/kwin/effects/plugins/libbetter_wobblywindows.so\n'
printf '  .../qt6/plugins/kwin/effects/configs/libbetter_wobblywindows_config.so\n'

if [[ ${#FoundLibraries[@]} -eq 0 ]]; then
    printf '\nDiagnosis: the effect library is not installed in Qt6/KWin plugin paths.\n'
    printf 'Run: ./build.sh --install --prefix /usr\n'
elif [[ ${#FoundConfigs[@]} -eq 0 ]]; then
    printf '\nDiagnosis: the effect exists, but its configuration module is missing.\n'
else
    printf '\nDiagnosis: files are present. If the effect is still absent in System Settings, restart KWin/log out-in and inspect the log lines above for ABI/IID or support errors.\n'
fi
