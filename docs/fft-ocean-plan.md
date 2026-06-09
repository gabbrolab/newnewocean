# FFT Ocean Implementation Plan

This branch implements the next Gabbro's Lab ocean step: a selectable FFT ocean
mode built next to the Gerstner scene, not on top of it.

## Current renderer constraints

- The Gerstner scene uses a static grid mesh and vertex/fragment shaders.
- The current OpenGL context is 3.3 core.
- The local GL loader only exposes the small function set needed by the Gerstner
  milestone.
- FFT ocean rendering needs float textures, image load/store, memory barriers,
  and compute shader dispatch. The FFT path will therefore raise the context
  target to OpenGL 4.3 core and extend the loader deliberately.

## FFT model

The ocean surface is generated in frequency space, then transformed back to
spatial textures with inverse FFT passes.

For each frequency vector `k`:

```text
omega(k) = sqrt(g * |k|)
H(k, t) = H0(k) * exp(i * omega * t)
        + conj(H0(-k)) * exp(-i * omega * t)
```

The initial spectrum uses Gaussian random values scaled by an ocean spectrum:

```text
H0(k) = 1 / sqrt(2) * (gauss1 + i * gauss2) * sqrt(S(k))
```

The target spectrum is JONSWAP with wind speed, wind direction, fetch, gamma,
directional spreading, amplitude scale, and low/high frequency filters. A
simple Phillips fallback can be kept only as a debug comparison.

## GPU resources

The FFT system will own its resources through an `FftOcean` module:

- `FftOceanConfig`: resolution, patch length, choppiness, quality preset.
- `SpectrumParameters`: wind and JONSWAP controls.
- `FftCascade`: textures and settings for one simulation scale.
- `ComputeShader`: shader helper for `.comp` programs.
- Float textures for `H0`, time-domain spectrum, ping-pong FFT buffers, height,
  slope, horizontal displacement, Jacobian, and foam.

The first implementation starts with one 256 x 256 cascade. Later milestones add
512/1024 presets and multiple cascades.

## Milestones

1. Generate and debug-display the initial spectrum.
2. Validate a simple inverse transform prototype.
3. Replace the prototype with a GPU Stockham FFT.
4. Animate the frequency spectrum with the dispersion relation.
5. Sample the height texture in the ocean mesh.
6. Generate slopes and use them for normals.
7. Add horizontal displacement for choppy waves.
8. Replace the plastic Gerstner material with a more plausible FFT water shader.
9. Detect foam from the displacement Jacobian.
10. Accumulate and decay foam over time.
11. Add multi-scale cascades to reduce visible tiling.
12. Add quality presets and debug timing.
13. Tune a final cinematic FFT ocean scene.

## Verification contract

Every milestone must build, run or capture if possible, and be committed with
the exact milestone message. Debug views should make the current output visible
before the next milestone hides it behind a prettier shader.
