#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# M1a on Linux (x64, arm64) and macOS (arm64): V1 (engine build + ctest), V2 (fiber test), V3 (recompiler difftest)
# and the synthetic cartridge, at the revisions in spike/m1a/deps.lock.json, with our patches. The cartridge's
# per-frame state hashes are compared with the Windows x64 reference (fixtures/).
#   bash spike/m1a/run_posix.sh [work dir, default ~/m1a] [frames, default 600] [results name, default: os-arch]
# Needs: clang, CMake >= 3.28, Ninja, Python 3, git; on Linux the X11/XKB/ALSA/Pulse development files.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="${1:-$HOME/m1a}"
FRAMES="${2:-600}"
OS="$(uname -s)"
NAME="${3:-$(echo "$OS" | tr 'A-Z' 'a-z')-$(uname -m)}"
OUT="$HERE/results/$NAME"
ENGINE="$WORK/ffcc"
mkdir -p "$WORK" "$OUT"
: > "$OUT/summary.txt"
summary() { echo "$*" | tee -a "$OUT/summary.txt"; }
FAILED=0
fail() { summary "$*"; FAILED=1; }
if [ "$OS" = Darwin ]; then
  # Apple's linker requires every dylib to link libSystem, so no -nostdlib here
  MODULE="$OUT/zpoc/libzpoc_game.dylib"; MODFLAGS=(-dynamiclib -undefined dynamic_lookup)
else
  export CC="${CC:-clang}" CXX="${CXX:-clang++}"
  MODULE="$OUT/zpoc/libzpoc_game.so"; MODFLAGS=(-shared -fPIC -nostdlib)
fi
# run with a time limit: coreutils timeout where present (Linux), else perl's alarm (macOS has no timeout)
with_limit() { local s="$1"; shift
  if command -v timeout > /dev/null; then timeout "$s" "$@"; else perl -e 'alarm shift @ARGV; exec @ARGV or die' "$s" "$@"; fi; }
CC_="${CC:-clang}"; CXX_="${CXX:-clang++}"

{ uname -srm; "$CC_" --version | head -1; cmake --version | head -1; echo "ninja $(ninja --version)"; python3 --version; } \
  > "$OUT/versions.txt" 2>&1
summary "platform: $(uname -srm), $("$CC_" --version | head -1)"

python3 "$HERE/fetch_engine.py" --dest "$ENGINE" > "$OUT/fetch.log" 2>&1 || { summary "fetch FAILED"; exit 1; }
summary "fetch: $(tr '\n' ' ' < "$OUT/fetch.log")"

# ---- V1: build (the engine has a Linux preset; macOS has none: the same settings by hand) + our headless host
cd "$ENGINE"
INCLUDE="-DCMAKE_PROJECT_FFCCEOTRecomp_INCLUDE=$HERE/host/m1a_targets.cmake"
if [ "${ST_NATIVE_BUILD_APP:-0}" = 1 ]; then
  INCLUDE="-DCMAKE_PROJECT_FFCCEOTRecomp_INCLUDE=$ROOT/app/st_targets.cmake"
fi
if [ "$OS" = Linux ]; then
  BUILD=build/linux-release
  CONFIGURE=(cmake --preset linux-release "$INCLUDE")
else
  BUILD=build/macos-release
  CONFIGURE=(cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "$INCLUDE")
fi
start=$(date +%s)
if "${CONFIGURE[@]}" > "$OUT/v1_configure.log" 2>&1 \
   && cmake --build "$BUILD" -- -k 0 > "$OUT/v1_build.log" 2>&1; then
  summary "V1 build ok in $(( $(date +%s) - start )) s"
  (cd "$BUILD" && ctest --output-on-failure > "$OUT/v1_ctest.log" 2>&1) || fail "V1 ctest FAILED"
  summary "V1 ctest: $(grep -E 'tests passed|tests failed' "$OUT/v1_ctest.log")"
  # ui_smoke has a fixed 120 s limit; on a slow software renderer, time the same run without it (slow or hung?)
  if grep -q "ui_smoke (Failed)" "$OUT/v1_ctest.log"; then
    W="$WORK/ui_smoke_diag"; rm -rf "$W"; mkdir -p "$W"
    t0=$(date +%s)
    with_limit 900 "$BUILD/game/ffcc" --prelaunch --first-run --no-settings --data-dir "$W/data" \
      --ui-script tests/ui_smoke.txt --ui-shots "$W/shots" > "$OUT/ui_smoke_diag.log" 2>&1
    rc=$?
    summary "V1 ui_smoke without the 120 s limit (900 s instead): exit $rc after $(( $(date +%s) - t0 )) s," \
      "$(ls "$W/shots" 2>/dev/null | wc -l | tr -d ' ') screenshots of the script's $(grep -c '^shot:' tests/ui_smoke.txt)"
  fi
else
  fail "V1 configure/build FAILED: $(grep -m3 -E 'error:|Error' "$OUT/v1_configure.log" "$OUT/v1_build.log" | cut -c1-300 | tr '\n' ' ')"
fi

# ---- V2: fiber conformance (the engine's fiber.cpp); macOS needs _XOPEN_SOURCE for the deprecated ucontext API.
# -ffp-contract=off: the transcript prints FP results, and AArch64 compilers otherwise fuse a*b+c (other rounding).
XOPEN=(); [ "$OS" = Darwin ] && XOPEN=(-D_XOPEN_SOURCE=600)
for O in O0 O2; do
  if "$CXX_" -std=c++17 -$O -ffp-contract=off "${XOPEN[@]}" -I "$ENGINE/runtime/src" "$HERE/fiber_conformance.cpp" \
       "$ENGINE/runtime/src/fiber.cpp" -o "$WORK/fiber_$O" > "$OUT/v2_build_$O.log" 2>&1; then
    if "$WORK/fiber_$O" > "$OUT/v2_fiber_$O.txt" 2>&1; then
      summary "V2 -$O exit 0: $(tail -1 "$OUT/v2_fiber_$O.txt")"
    else
      fail "V2 -$O FAILED: $(tail -1 "$OUT/v2_fiber_$O.txt")"
    fi
  else
    fail "V2 -$O build FAILED: $(grep -m2 'error' "$OUT/v2_build_$O.log" | cut -c1-200 | tr '\n' ' ')"
  fi
done

# The Android path (patch 0003: an AArch64 context switch, since bionic has no ucontext) runs on any AArch64 CPU:
# on arm64 Linux, also run V2 with that branch enabled.
if [ "$OS" = Linux ] && [ "$(uname -m)" = aarch64 ]; then
  sed 's/defined(__ANDROID__) && defined(__aarch64__)/defined(__aarch64__)/' "$ENGINE/runtime/src/fiber.cpp" \
    > "$WORK/fiber_android_path.cpp"
  if "$CXX_" -std=c++17 -O2 -ffp-contract=off -I "$ENGINE/runtime/src" "$HERE/fiber_conformance.cpp" \
       "$WORK/fiber_android_path.cpp" -o "$WORK/fiber_android_path" > "$OUT/v2_build_android_path.log" 2>&1; then
    if "$WORK/fiber_android_path" > "$OUT/v2_fiber_android_path.txt" 2>&1; then
      summary "V2 Android context switch (on arm64 Linux) exit 0: $(tail -1 "$OUT/v2_fiber_android_path.txt")"
    else
      fail "V2 Android context switch FAILED: $(tail -1 "$OUT/v2_fiber_android_path.txt")"
    fi
  else
    fail "V2 Android context switch build FAILED"
  fi
fi

# ---- V3: the engine's recompiler difftest (unicorn in a venv)
RECOMP="$ENGINE/$BUILD/recompiler/ntr-recomp"
if [ -x "$RECOMP" ]; then
  [ -x "$WORK/venv/bin/python" ] || { python3 -m venv "$WORK/venv" && "$WORK/venv/bin/pip" install -q unicorn==2.1.4; }
  for SEED in 1 2; do
    "$WORK/venv/bin/python" "$ENGINE/tests/difftest/difftest.py" --recomp "$RECOMP" --count 300 --seed "$SEED" \
      > "$OUT/v3_difftest_seed$SEED.txt" 2>&1
    rc=$?
    summary "V3 seed $SEED exit $rc: $(grep -E '^ *[a-z_0-9]+ +[0-9]+/[0-9]+ ok' "$OUT/v3_difftest_seed$SEED.txt" | awk '{split($2,a,"/"); n+=a[1]; t+=a[2]} END {print n "/" t " instructions ok"}')"
    [ $rc -eq 0 ] || FAILED=1
  done
else
  fail "V3 skipped: no ntr-recomp"
fi

# ---- the synthetic cartridge (fixture: our own code, with the logo checksum) against the Windows x64 reference
Z="$OUT/zpoc"; rm -rf "$Z"; mkdir -p "$Z"
ROM="$HERE/fixtures/zpoc_synthetic.rom"
RUN="$ENGINE/$BUILD/ntr_run"
if [ -x "$RUN" ] && "$RECOMP" --rom "$ROM" --config "$HERE/zpoc_config" --out "$Z/gen" --allow-unknown-rom > "$Z/recomp.log" 2>&1 \
   && "$CC_" "${MODFLAGS[@]}" -w -ffreestanding -fno-stack-protector -O1 -DNTR_GAME_MODULE=1 \
        -I "$ENGINE/runtime/include" -I "$Z/gen" "$Z"/gen/*.c -o "$MODULE" > "$Z/module_build.log" 2>&1; then
  "$RUN" "$ROM" "$MODULE" "$FRAMES" "$Z/hashes_recomp_1.txt" > "$Z/run_1.log" 2>&1 || fail "cartridge: first run FAILED"
  "$RUN" "$ROM" "$MODULE" "$FRAMES" "$Z/hashes_recomp_2.txt" > "$Z/run_2.log" 2>&1 || fail "cartridge: second run FAILED"
  summary "cartridge: $(tail -1 "$Z/run_1.log")"
  cmp -s "$Z/hashes_recomp_1.txt" "$Z/hashes_recomp_2.txt" && summary "cartridge: identical frame hashes twice" \
    || fail "cartridge: frame hashes DIFFER between two runs"
  REF="$HERE/fixtures/zpoc_hashes_windows_x64.txt"
  if cmp -s "$REF" "$Z/hashes_recomp_1.txt"; then
    summary "cartridge: frame hashes identical to Windows x64"
  else
    fail "cartridge: frame hashes differ from Windows x64 from frame $(paste -d' ' "$REF" "$Z/hashes_recomp_1.txt" | awk '$2 != $5 {print $1; exit}')"
  fi
else
  fail "cartridge FAILED: $(cat "$Z/recomp.log" "$Z/module_build.log" 2>/dev/null | grep -m3 -iE 'error' | cut -c1-200 | tr '\n' ' ')"
fi
exit $FAILED
