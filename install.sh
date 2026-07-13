#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

ProjectRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${ProjectRoot}/build.sh" --install "$@"
