# ADR-0007: Graphics backends — GL family first, backend-neutral presentation, Software fallback everywhere

- Status: **Proposed**
- Date: 2026-11-12

## Context

melonDS's renderers are the only DS rasterisers available: Software (CPU, exact, native resolution), OpenGL (GLSL 1.40,
upscaling) and Compute (GLSL 4.30 compute, upscaling, more accurate). There is no Vulkan or Metal DS renderer. Targets
differ: Windows/Linux have desktop GL 4.6; macOS stops at a deprecated GL 4.1 core profile (no compute); Android has
GLES 3.x (PH converted melonDS's shaders to ES 3.20). SDL3's GPU API covers Vulkan, D3D12 and Metal but not OpenGL.
RmlUi 6.3 has GL3 (with an Android GLES path), Vulkan, DX12 and SDL_GPU renderers.

## Options

1. One API everywhere (e.g. GLES through ANGLE, or Vulkan with MoltenVK) — no DS rasteriser exists for Vulkan; ANGLE's
   Metal backend ES 3.1 status is unclear.
2. Write our own DS rasteriser on SDL_GPU now — large, duplicates melonDS's accuracy work before we have a baseline.
3. **Per-platform GL family for DS rasterisation + a backend-neutral presentation layer + Software fallback**, with a
   later SDL_GPU presentation backend.

## Decision

Option 3.
- DS rasterisation: Software everywhere; OpenGL on Windows, Linux, macOS (4.1) and Android (GLES); Compute on Windows
  and Linux, experimental on Android, unavailable on macOS.
- `app/present` defines a `PresentBackend` interface; layouts, PiP, HUD placement decisions, settings and UI never call
  a graphics API directly. First backend: GL/GLES. Second (when needed): SDL_GPU, fed by the Software renderer's CPU
  images, giving a no-OpenGL path on every platform.
- Features implemented inside melonDS's GL shaders (widescreen HUD moves, texture smoothing, texture replacement) are
  marked GL-family-only, optional and experimental until a backend-neutral form exists.
- The runtime's frame output stays a tagged image (CPU pixels or a GL texture handle); new backends add tags, not
  special cases in layout code.

## Consequences

- macOS gets upscaling through the OpenGL renderer but not the Compute renderer; if Apple removes OpenGL, macOS falls
  back to Software + SDL_GPU (no upscaling) until a Metal/Vulkan rasteriser exists (research item, see RISKS).
- Low-end Android phones need GLES (Software is too slow there per PH's measurements).
- Two shader dialects (desktop GLSL, GLSL ES) are generated from one source at configure time (PH's approach) and
  checked in CI with `glslc`.
