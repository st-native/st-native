# Frame pacing and audio diagnostics

Status: **Verified locally** on Windows x64, 2026-11-17. These are measurements of the tested opening on the
maintainer's machine, not a performance guarantee for other hardware or later scenes.

## Findings and changes

The pre-title single-screen camera uses an extra software rasterization pass. The earlier synchronization used
`PreSavestate`/`PostSavestate` to protect the original geometry; `PostSavestate` restarted the original software
render as well. Patch 0016 makes software-render completion a reusable fence, resets that fence when a new frame
starts, and waits on it for the cinematic pass. It preserves scanline readiness without scheduling that redundant
original-frame render. The regular save-state methods are unchanged. Attribution: pinned melonDS source,
`906e9ebb27da8c6a715cd7abab4abfe8a8d29427`, GPL-3.0-or-later; the patch is our modification.

The frontend also wakes on a newly published frame instead of relying only on a 4 ms event poll. Notifications
are coalesced while an unconsumed frame is pending. The audio stream starts after approximately 50 ms of source
samples are queued, including after pause/resume. Audio queue state changes are serialized against incoming
samples. This provides short-term scheduling headroom; it adds buffering and does not repair sustained slow
execution. The previous 100 ms queue cap remains. No guest cycles, game state, controls or projection are changed.

## Local measurements

Each real-window case uses the same cached module, layout, blank save and opening. Samples are guest frames
180 through 1,749 (1,570 samples), excluding startup and the smoke test's intentional pause. Cases run sequentially
without compilation or hash generation competing for CPU. Normal source cadence is about 59.826 Hz / 16.715 ms.

| Single-screen camera | Mean production interval | p95 production interval | Empty input-queue observations | Samples discarded |
|---|---:|---:|---:|---:|
| Before fixes | 18.03 ms | 23.92 ms | 472 | 0 |
| Fence correction only | 16.86 ms | 19.75 ms | 77 | 0 |
| Fence, readiness notification and audio priming | 16.72 ms | 18.28 ms | 0 | 0 |

An empty input queue before a feed is a starvation-risk proxy, not a measurement of audible clicks or of all
buffers in the OS/audio device. No game audio is recorded. The final original-DS case also has zero empty-feed
observations and zero discarded samples.

**Remaining limitation:** with the single-screen camera, p95 intervals between displayed frames were 21.65 ms
before and 21.09 ms after. Production cadence, presentation cadence and buffering are different measurements.
The extra software camera still has significant cost; this is not a claim of perfectly flat frame times or a
solution for every later scene. Further rasterizer/render scheduling work is still needed.

**Correctness:** the 4,200-frame title replay matches every original guest hash; all five original dual-screen BMP
checkpoints and all three cinematic checkpoints are byte-identical before/after the performance changes. All 70
periodic GX checks report zero geometry mismatches. Local C++ tests, Python tests and real-window pause/resume,
manual layout switching and return-to-launcher smoke runs passed. The overlapping patch-stack regression tests
also check preservation of the real Git index, unrelated edits and future untracked-file collisions.

## Reproducing the measurements

Use `st_native <rom> --module <local module> --no-save --headless --exit-after 3600 --frame-profile build/cpu.csv`
for CPU cost only. Add `--cinematic-aspect 1.3333333333` for the extra camera. Headless runs have no audio device
or real presentation; do not interpret their interval as paced frame time.

For the actual launcher, set `ST_NATIVE_FRAME_PROFILE` to an absolute output CSV path, then launch normally.
The parent directory must exist. Optional developer smoke controls (only used with `--game-smoke <rom>`) are:

- `ST_NATIVE_SMOKE_LAYOUT=focus` (or `classic`, `side-by-side`, `pip`) to select one case.
- `ST_NATIVE_SMOKE_FRAMES=1800` to select the first pause point, in presented frames.
- `ST_NATIVE_SMOKE_ORIGINAL=1` to disable automatic cinematic composition in that case; unset for the default.

The original CLI option or environment variable writes a game-thread CSV and a `.present.csv` sidecar for the
window thread. Fields include numeric timing, camera cost, input-queue depth, empty-feed observations, dropped
sample counts and guest frame IDs; no assets, memory contents, ROM paths or audio samples are recorded.
The presentation sidecar also records `shared_scene` and `cinematic`, indicating whether each displayed frame used
an identified scene and actually supplied the extra camera image. These help distinguish a rendering preview from
a context decision without relying on screenshots.
Raw diagnostic files belong under ignored `build/`. Remove the environment variable after a profiling run.

Run hash/image correctness checks separately with `tests/rom/title_single_screen.txt`, `--hashes`, local `--shots`
and `--gx-check`. Per-frame memory hashing and image writes add overhead and should not be enabled in timing runs.
Never publish the resulting images, modules, saves or hash logs.
