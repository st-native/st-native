#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# M1a on Android arm64 (V1's Android criterion: NDK compile + link). Cross-compiles, from a Linux x64 host, the
# engine's runtime, melonDS core and recompiler plus our headless host and the V2 fiber test for arm64-v8a. Running
# them needs an arm64 device or emulator (not done here).
#   bash spike/m1a/run_android.sh [work dir, default ~/m1a] [API level, default 28]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${1:-$HOME/m1a}"
API="${2:-28}"
OUT="$HERE/results/android-arm64"
ENGINE="$WORK/ffcc"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_LATEST_HOME:-}}"
mkdir -p "$WORK" "$OUT"
: > "$OUT/summary.txt"
summary() { echo "$*" | tee -a "$OUT/summary.txt"; }
FAILED=0
[ -d "$NDK" ] || { summary "no Android NDK (set ANDROID_NDK_HOME)"; exit 1; }
summary "NDK: $NDK ($(grep -m1 Pkg.Revision "$NDK/source.properties" 2>/dev/null)), API $API, arm64-v8a"

python3 "$HERE/fetch_engine.py" --dest "$ENGINE" > "$OUT/fetch.log" 2>&1 || { summary "fetch FAILED"; exit 1; }
summary "fetch: $(tr '\n' ' ' < "$OUT/fetch.log")"

cd "$ENGINE"
BUILD=build/android-arm64
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM="android-$API" \
  -DFFCC_BUILD_TESTS=OFF -DCMAKE_PROJECT_FFCCEOTRecomp_INCLUDE="$HERE/host/m1a_targets.cmake" > "$OUT/v1_configure.log" 2>&1 \
  || { summary "V1 configure FAILED: $(grep -m3 -E 'Error|error' "$OUT/v1_configure.log" | cut -c1-300 | tr '\n' ' ')"; exit 2; }
summary "V1 configure ok"
for T in melonds_core ntr_runtime ntr-recomp ntr_run; do
  if cmake --build "$BUILD" --target "$T" > "$OUT/v1_build_$T.log" 2>&1; then
    summary "V1 $T: compiled and linked"
  else
    summary "V1 $T: FAILED: $(grep -m3 -E 'error:' "$OUT/v1_build_$T.log" | sed -E 's#.*/ffcc/##' | cut -c1-250 | tr '\n' ' ')"
    FAILED=1
  fi
done
CLANGXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android$API-clang++"
if "$CLANGXX" -std=c++17 -O2 -ffp-contract=off -static-libstdc++ -I "$ENGINE/runtime/src" "$HERE/fiber_conformance.cpp" \
     "$ENGINE/runtime/src/fiber.cpp" -o "$OUT/fiber_conformance_arm64" > "$OUT/v2_build.log" 2>&1; then
  summary "V2 fiber test: compiled and linked for arm64 (running it needs a device or emulator)"
else
  summary "V2 fiber test: FAILED: $(grep -m3 -E 'error' "$OUT/v2_build.log" | sed -E 's#.*/ffcc/##' | cut -c1-250 | tr '\n' ' ')"
  FAILED=1
fi
exit $FAILED
