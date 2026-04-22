# SRP-PHAT DOA Implementation Design

**Date:** 2026-04-21
**Branch:** feat/xscope-host-injection
**Status:** Approved

## Overview

Add SRP-PHAT (Steered Response Power with Phase Transform) Direction of Arrival estimation as a new DOA algorithm alongside the existing GCC-PHAT implementation. SRP-PHAT supports multi-source detection (top-N peaks) with configurable search resolution, integrated into the batch processing pipeline.

## Requirements

- Return at least 3 DOA candidates per frame via top-N peak search
- Configurable search grid resolution (adjustable at init time for experimentation)
- Per-source output: angle_rad, angle_deg, confidence
- Binary output files with host-side CSV conversion
- Run alongside existing GCC-PHAT in batch mode (both execute per frame)

## New Files

### `modules/fph/doa/api/srp_phat.h`

Public API header containing:

- `srp_phat_config_t` — runtime configuration struct:
  - `unsigned num_directions` — grid size (e.g. 36, 72, 180, 360)
  - `unsigned max_sources` — maximum number of peaks to return (e.g. 3)
  - `unsigned min_peak_sep_deg` — minimum angular separation between peaks
  - `float array_radius_m` — array radius in meters (0.0355f)
  - `float sample_rate_hz` — sample rate (16000.0f)
  - `float speed_of_sound` — speed of sound (343.0f)

- `srp_source_t` — per-source result:
  - `float angle_rad`
  - `float angle_deg`
  - `float confidence` — relative power (peak / mean of power map)

- `srp_phat_state_t` — estimator state (opaque, sized by config)

- API functions:
  - `srp_phat_init(state, config)` — allocate and initialize state including precomputed steering delays
  - `srp_phat_process_frame(state, input, in_exp)` — process one frame, return number of sources found
  - `srp_phat_get_sources(state)` — get pointer to source results array
  - `srp_phat_get_power_map(state)` — get pointer to power map (for diagnostics)

- Sizing macro: `SRP_PHAT_STATE_SIZE(config)` — returns bytes needed for state

### `modules/fph/doa/src/srp_phat.c`

Implementation of the SRP-PHAT algorithm.

## Algorithm

### 1. FFT each microphone (overlap-save)

Same as current GCC-PHAT:
- Build 256-sample frame: tail[16] + new[240]
- Forward FFT (mono real, unpacked to 129 bins)
- Store spectra for all 4 mics

### 2. Compute PHAT-weighted cross-spectra for all 6 mic pairs

For each of the 6 unique pairs (0,1), (0,2), (0,3), (1,2), (1,3), (2,3):
- Compute `Z = A * conj(B)` using `vect_complex_s32_conj_mul`
- Compute magnitude `|Z|`
- Apply PHAT weighting: `Z_phat = Z / |Z|` (normalize by magnitude)
- Store the 6 PHAT-weighted cross-spectra for the steering step

### 3. Build steered response power map

For each steering direction `θ_d` (d = 0..num_directions-1):
- For each mic pair (i,j), retrieve precomputed delay `τ_ij(θ_d)` in fractional samples
- For each frequency bin k, compute the steering phase: `φ_k = 2π · k · τ_ij(θ_d) / N_fft`
- Accumulate the steered power:
  ```
  P(θ_d) += Σ_k Re{ Z_phat_pair[k] · exp(j·φ_k) }
  ```
  summed across all 6 pairs and all 129 bins.

This is equivalent to delay-and-sum beamforming in the frequency domain using the GCC-PHAT cross-spectrum.

### 4. Precomputed steering delays

At init time, for each direction and mic pair, compute:
```
τ_ij(θ) = (fs / c) · (cos(θ)·(x_j - x_i) + sin(θ)·(y_j - y_i))
```

Mic positions on the circle (radius r = 0.0355m):
- mic0: (r, 0) at 0°
- mic1: (0, r) at 90°
- mic2: (-r, 0) at 180°
- mic3: (0, -r) at 270°

Directions are uniformly spaced: `θ_d = 2π · d / num_directions`.

The steering delays are stored as floats in `steering_delays[num_directions][6]`.

### 5. Peak search

- Find the highest value in `power_map[]`
- Record as peak, then zero out a window of ±`min_peak_sep_deg` around it
- Repeat for up to `max_sources` peaks
- Each peak's confidence = `peak_power / mean(power_map)`

### Input layout

Same as GCC-PHAT: `[240 mic0][240 mic1][240 mic2][240 mic3]` = 960 int32 samples.

## Modified Files

### `modules/fph/doa/CMakeLists.txt`

Add `src/srp_phat.c` to `target_sources`.

### `satellite-xmos-firmware/src/batch_processor.c`

Add after existing Stage 5 (GCC-PHAT DOA):

```c
/* Stage 5b: SRP-PHAT DOA */
// Copy mic data into srp_input buffer
// Call srp_phat_process_frame() on raw mic
// Write binary output: per source (angle_rad, angle_deg, confidence)
// Call srp_phat_process_frame() on AEC output
// Write binary output: per source (angle_rad, angle_deg, confidence)
```

New output files:
- `output_srp_doa.bin` — SRP-PHAT on AEC output
- `output_srp_doa_raw.bin` — SRP-PHAT on raw mic

Binary format per frame: `max_sources × sizeof(srp_source_t)` = 3 × 12 = 36 bytes, written unconditionally (zero-filled confidence for unused sources).

New static state and buffers added alongside existing GCC-PHAT state.

### `scripts/run_batch_test.py`

Add host-side conversion for `output_srp_doa.bin` and `output_srp_doa_raw.bin`:
- Read binary (36 bytes per frame = 3 sources × 12 bytes)
- Write CSV with columns: `frame,source,confidence,angle_rad,angle_deg`

### `satellite-xmos-firmware/xk-voice-sq66-fileio-batch.cmake`

No changes needed — `fph::lib_doa` already linked.

## Memory Estimate (72 directions, 3 max sources)

| Component | Size |
|-----------|------|
| Steering delays (72 × 6 × 4B) | 1.7 KB |
| Power map (72 × 4B) | 288 B |
| Cross-spectra storage (6 × 129 × 8B) | 6.2 KB |
| FFT state (shared with GCC-PHAT layout) | ~6 KB |
| Source results (3 × 12B) | 36 B |
| **Total additional** | **~14 KB** |

This fits within the XMOS XS3 SRAM budget alongside the existing pipeline.

## API Example

```c
// Configure
srp_phat_config_t cfg = {
    .num_directions = 72,
    .max_sources = 3,
    .min_peak_sep_deg = 15,
    .array_radius_m = 0.0355f,
    .sample_rate_hz = 16000.0f,
    .speed_of_sound = 343.0f,
};

// Allocate state
uint8_t state_buf[SRP_PHAT_STATE_SIZE(&cfg)];
srp_phat_state_t *state = (srp_phat_state_t *)state_buf;
srp_phat_init(state, &cfg);

// Per frame
int n = srp_phat_process_frame(state, doa_input, -31);
const srp_source_t *src = srp_phat_get_sources(state);
for (int i = 0; i < n; i++) {
    printf("Source %d: %.1f deg (conf %.2f)\n",
           i, src[i].angle_deg, src[i].confidence);
}
```

## Out of Scope

- Integration with real-time pipeline (main.c) — batch mode only for now
- LED ring visualization for multi-source
- DOA servicer (SPI) multi-source support
- Elevation estimation (azimuth only)
