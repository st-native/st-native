#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# M1a on Linux x64 (results in spike/m1a/results/linux): see run_posix.sh.
#   bash spike/m1a/run_linux.sh [work dir, default ~/m1a] [frames, default 600]
exec bash "$(cd "$(dirname "$0")" && pwd)/run_posix.sh" "${1:-$HOME/m1a}" "${2:-600}" linux
