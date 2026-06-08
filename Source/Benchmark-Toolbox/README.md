# Horizon OC Toolbox

A borealis (full-screen, controller-driven) front-end for the Horizon-OC
benchmark / stress engine.

## Features

- **System Info** — live CPU / GPU / MEM (EMC) clocks, memory mode, thread count.
- **Benchmarks** — GPU bandwidth (copy/read/write), CPU bandwidth (3 threads),
  and memory latency (L2 / full RAM), run on a worker thread with a live
  progress readout.
- **GPU Stress** — a combined compute + memory GPU stress kernel with live
  GFLOPS / dispatch / mismatch counters (start / stop). Any mismatch means the
  GPU produced an incorrect result → the overclock is unstable.

The UI is built programmatically with borealis (no XML), so the app is
self-contained.

## Building

Requires devkitPro with the Switch toolchain. Verified building against
devkitA64 GCC 15.2 + current libnx.

borealis is vendored under `lib/borealis` (no submodule to init), and the
runtime resources it loads from `romfs:/` are committed under `resources/`, so
the build is turnkey:

```sh
make
```

Produces `Horizon-OC-Toolbox.nro`.

If you don't already have borealis' dependencies installed:

```sh
sudo dkp-pacman -S switch-glfw switch-mesa switch-glm \
                   switch-libdrm_nouveau switch-glad
```

## Notes

- Both borealis (deko3d) and the GPU-bandwidth test (EGL/GLES) are linked; the
  bandwidth test creates and fully releases its own EGL context around the run.
- The Makefile force-includes `<optional>` for borealis' `dk_renderer.hpp`.
