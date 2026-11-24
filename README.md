# gfx-research-clouds

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![OpenGL 4.6](https://img.shields.io/badge/OpenGL-4.6-5586A4.svg)](https://www.opengl.org/)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C.svg)](https://cmake.org/)

Research on high-performance procedural volumetric cloud rendering using adaptive sampling, hierarchical empty-space skipping, and analytical lighting approximations.

[Method](#method) · [Build](#build) · [Controls](#controls) · [Experiments](#experiments) · [References](#references)

## Method

An isolated sky renderer compares a planar baseline with a spherical-shell reconstruction inspired by the 2015 Nubis presentation. The latter provides a common density field and optical model for a factorial comparison of three independent optimization factors.

| ID | Method |
|---|---|
| M0 | Simplified Nubis |
| M1 | Nubis |
| M2 | Nubis + OPT16 |
| M3 | Nubis + FST25 |
| M4 | Nubis + OPT16 + FST25 |
| M5 | Nubis + AAL |
| M6 | Nubis + OPT16 + AAL |
| M7 | Nubis + FST25 + AAL |
| M8 | Nubis + OPT16 + FST25 + AAL |
| REF | High-Precision Reference |
| REF2 | Convergence Reference |

M1 is a research reconstruction, not the original game shader or a timing reproduction. OPT16 is a low-step adaptation of the 2016 approach; exact constant-segment Beer–Lambert integration is already shared by the baseline. FST25 adapts the hierarchical bounding idea to sampled periodic textures, rather than reproducing the paper's analytic derivative-based sphere tracer. AAL is an experimental adaptive analytical lighting approximation, not an established novel method. See [implementation notes](docs/IMPLEMENTATION.md) for assumptions and limitations.

## Build

Requirements: CMake 3.25+, C++20 compiler, Ninja, OpenGL 4.6 Core driver, Python 3 with Jinja2 (base dependency). The application uses `gfx-research-base` v0.3, the same UI style and dependency configuration as the sky experiment.

```powershell
python -m pip install jinja2
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --parallel
ctest --test-dir cmake-build-release --output-on-failure
.\cmake-build-release\gfx-research-clouds.exe
```

For a local base checkout, append `-DGFX_RESEARCH_BASE_LOCAL_PATH=../gfx-research-base` to the configure command (use an absolute path if needed). The initial online configure downloads the pinned base and its dependencies. Linux: omit `.exe` and use `./cmake-build-release/gfx-research-clouds`. The source directory must remain available because shader files are loaded from it.

CPU-only mathematical tests (no external dependencies):

```powershell
cmake -S . -B out/tests -DGFX_RESEARCH_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build out/tests
ctest --test-dir out/tests --output-on-failure
```

## Controls

The Clouds tab selects M0–M8, REF and REF2 and exposes density, coverage, shape, scattering, sampling budgets and AAL tolerance; optional static temporal supersampling accumulates up to 64 samples for frozen views, resetting on any parameter change. Sky selects dawn, morning, noon, sunset, twilight or night, custom solar angles, atmospheric parameters and animated/frozen clouds. Camera exposes elevation, azimuth and FOV; buttons set 0, 30, 60 and 90 degrees. No scene geometry is rendered. Tests provides manual capture and three automated profiles. Ctrl+H toggles the parameter panel; Ctrl+F toggles fullscreen.

## Experiments

Run from the repository root, or use the Tests tab. Output is relative to the working directory unless `--output` is supplied. The CLI exits after completion. Existing timing files are not overwritten.

```powershell
.\cmake-build-release\gfx-research-clouds.exe --smoke
.\cmake-build-release\gfx-research-clouds.exe --quick
.\cmake-build-release\gfx-research-clouds.exe --full
```

| Profile | Resolution | Angles | Lighting | Coverage | Base view budgets | Cases | Samples/case |
|---|---|---|---|---|---|---:|---:|
| Smoke | 640x360 | 30 | noon | .55 | 64 | 11 | 5 |
| Quick | 1280x720 | 0,30,60,90 | dawn,noon,sunset,night | .55 | 64 | 176 | 30 |
| Full | 1920x1080 | 0,30,60,90 | all six | .30,.55,.78 | 64,128 | 1584 | 60 |

Each group uses REF (512 view / 32 light steps) and REF2 (1024 / 64) before M0–M8. OPT16 uses 16 view steps, the base uses 6 light samples, and AAL uses tolerance .10. Time is frozen; no jitter or temporal reconstruction in benchmark profiles. Full is intentionally expensive; start with smoke. The reference is an internal numerical reference for the spherical reconstruction, not ground truth for real clouds. M0 has a different geometry and is not a quality-equivalent comparison to REF. Check REF2 versus REF before interpreting the other errors.

Results: `timing_raw.csv`, `timing_summary.csv`, `quality.csv`, `manifest.txt`, `status.txt`, `captures/*.png`, and reference `*.rgbf` (bottom-up, packed RGB float32, native little-endian on Windows). Timing covers the cloud/sky draw only, including both output attachments, not the UI, presentation, readback, startup generation or analysis. `pass_fps` is reciprocal GPU pass time, not application FPS.

```powershell
python -m pip install -r tools/requirements.txt
python tools/analyze.py results/benchmark-<run-id>
```

The postprocessor writes paired speedups, PNG-domain SSIM and amplified error maps. Full output uses lossless stored-block PNGs and may require over 15 GB of disk space. No performance figures are included before actual hardware measurements.

## References

- Schneider and Vos (2015), [The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn).
- Toft, Bowles and Zimmermann (2016), [Optimisations for Real-Time Volumetric Cloudscapes](https://arxiv.org/abs/1609.05344).
- Moinet and Neyret (2025), [Fast sphere tracing of procedural volumetric noise for very large and detailed scenes](https://doi.org/10.1111/cgf.70072).
- Schneider (2023), [Nubis, Cubed](https://www.guerrilla-games.com/read/nubis-cubed) (context, not implemented).
- Preetham, Shirley and Smits (1999), [A Practical Analytic Model for Daylight](https://doi.org/10.1145/311535.311545).
