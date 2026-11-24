# Validation status, v0.1.0

- CMake CPU-only configuration and C++20 build: passed.
- `cloud-reference-tests`: passed (clipped linear optical-depth integral, stable shell intersection, trilinear upper-bound property, Beer–Lambert segment integration).
- Mesa EGL software OpenGL with version override: all four shader stages compile; render, accumulation and compute programs link. All M0–M8/REF/REF2 render modes produce finite outputs in the synthetic 64x36 smoke harness, including horizon, 30/60/90 degree views and day/night lighting. No GL error reported. The harness uses synthetic textures and reduced reference budgets, not a production GPU benchmark; `python tools/validate_gl_linux.py` reproduces this limited check under Linux with EGL and NumPy.
- Full desktop executable build and interactive UI run: not verified in this environment (network access to the pinned gfx-research-base dependency is unavailable). The CMake dependency setup and API calls follow the prior sky project. Start with the Windows smoke test and inspect output before the full run.
- Temporal accumulation shader compiled and linked; its integrated interactive history plumbing has not been run here.
- No real hardware timings, comparative quality claims, or publication-ready results yet. AAL and FST25 are experimental adaptations.
