# Implementation and experimental contracts

## M0 and M1

M0 preserves the planar evaluator, generated RG8 noise, height-dependent erosion, adaptive coarse/fine stepping, cone illumination, powder, multiple-scattering approximation, and default settings from `gfx-research-sky-study.zip` (2026-11-22). The original atmosphere/volume source is reused; camera, solar azimuth and background compositing are new shared experimental controls. M0 is not a binary-exact reproduction of the previous application. M1 uses a curved shell, with a 6371 km nominal planet radius and normalized field units of 3 km, the same generated texture resources, height profile and a configurable cone approximation (six samples by default). It is a reconstruction of selected 2015 concepts, not production code, and does not include the 2015 temporal rendering pipeline. No proprietary assets are needed.

M1–M8 form a 2^3 factorial design with identical base field and camera. OPT16 lowers the view sample count while retaining exact integration of constant source/extinction per segment. The baseline already integrates `T * (1-exp(-sigma*rho*ds)) * source`, so the formula itself is not counted as new. Optional deterministic per-pixel jitter and static temporal supersampling are available interactively. The latter uses Halton pixel offsets, stochastic view steps and up to 64 history samples, resets on camera or parameter changes and is disabled when animated. It is not motion-reprojected TAA. A separate temporal study is required before comparing the full 2016 method.

## FST25 adaptation

Analytic FBM nesting/derivative stepping in Moinet and Neyret requires bounds not available directly from sampled texture noise. This experiment uses a conservative hierarchy over the existing shape texture. At level 0 each cell stores the maximum of the eight periodic trilinear vertices. Parents store maxima of eight children. The texture coordinates are shifted by half a texel to align with interpolation cells. Since height multiplication and erosion cannot increase the base shape signal, a cell with maximum strictly below `1-coverage` is empty for the column's constant coverage. Advance by the distance to the first cell boundary along the ray, conservatively rounded down to the regular sample lattice. This replaces the coarse search and may improve accuracy over M1 as well as change cost. It is a texture-bound adaptation, not the exact 2025 sphere-tracing algorithm; label accordingly in publication. The hierarchy takes about 2.29 MiB plus temporary startup CPU memory. Startup is excluded from pass timing; measure separately for dynamic/noise regeneration studies.

## AAL

The first experimental implementation models density along the light ray as `clamp(rho + slope*t, 0, 1)`. Estimate slope using the current density and one sample at 25% of the capped light path; validate against a sample at 65%. If the discrepancy is below the user-selected tolerance, integrate the clipped linear model analytically and apply Beer–Lambert. Otherwise fall back to the standard cone march. The proxy is not a bound on the true optical depth; it can miss separated occluders and differ in bright rims or thick clouds. Fallback adds two samples, so AAL may be slower in some conditions. The tolerance is a research parameter, not a guaranteed error limit. Compare M1/M5 and M4/M8 to isolate it. Assess novelty only after related-work review and measured evidence.

## Reference and metrics

REF and REF2 use identical spherical field and source, disable empty-space skipping and use more view and light samples. They retain the approximate cone lighting, not physically exact multiple scattering. Detail filtering is fixed relative to the pixel for view sampling, with interval filtering for light samples. Reference convergence is mandatory; high sample counts alone do not prove accuracy. M0 differs geometrically, so its native image error is descriptive only. All methods write linear radiance and a fixed display transform `pow(c/(1+c),1/2.2)`; CPU RMSE and display PSNR use floating-point readback before quantization. SSIM is computed separately on PNG captures. Raw timing samples are exact elapsed queries around the draw. No claim of speed or image equivalence is established by this source release.

## Known limitations

- No temporal reprojection, motion vectors, physically based atmosphere at night, terrain, cloud shadows on objects, or camera flight.
- The full benchmark tests static single-frame quality only; animated visual stability should be evaluated separately.
- FST25 bounds may be ineffective for high coverage; coarse search and hierarchy have different accuracy/overhead tradeoffs.
- AAL may fail on nonlocal occlusion; results are exploratory.
- GPU timing depends on driver, clocks, bandwidth and resolution; no extrapolation from other papers.
