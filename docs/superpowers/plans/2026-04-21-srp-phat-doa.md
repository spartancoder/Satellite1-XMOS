# SRP-PHAT DOA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add multi-source SRP-PHAT DOA estimation alongside the existing GCC-PHAT in the batch processing pipeline.

**Architecture:** Time-domain SRP-PHAT — compute GCC-PHAT cross-correlations for all 6 mic pairs via IFFT, then evaluate the steered response at precomputed theoretical TDOAs for each candidate direction. Peak search returns top-N sources with confidence scores. Configurable grid resolution set at init time.

**Tech Stack:** C (XMOS XS3), lib_xcore_math BFP/FFT APIs, xscope_fileio batch I/O, Python host scripts.

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `modules/fph/doa/api/srp_phat.h` | Create | Types, config, state struct, API |
| `modules/fph/doa/src/srp_phat.c` | Create | SRP-PHAT algorithm (init + process) |
| `modules/fph/doa/CMakeLists.txt` | Modify | Add `src/srp_phat.c` to build |
| `satellite-xmos-firmware/src/batch_processor.c` | Modify | Add SRP-PHAT stage alongside GCC-PHAT |
| `scripts/run_batch_test.py` | Modify | Add host-side CSV conversion for SRP output |

---

### Task 1: Create `srp_phat.h`

**Files:**
- Create: `modules/fph/doa/api/srp_phat.h`

- [ ] **Step 1: Write the header file**

```c
// SRP-PHAT DOA estimator for 4-mic circular array
// Multi-source direction of arrival using Steered Response Power
// with Phase Transform (SRP-PHAT)
//
// Designed for:
//   - 4 microphones in circular array
//   - 16 kHz sample rate
//   - 240 sample frame advance
//   - 256 FFT length
//
// Input layout: [240 mic0][240 mic1][240 mic2][240 mic3]

#ifndef SRP_PHAT_H_
#define SRP_PHAT_H_

#include <stdint.h>
#include "xcore_math.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------
// Configuration constants
// ------------------------------------------------------------

#define SRP_PHAT_NUM_MICS       (4)
#define SRP_PHAT_NUM_PAIRS      (6)
#define SRP_PHAT_FRAME_ADVANCE  (240)
#define SRP_PHAT_FFT_LENGTH     (256)
#define SRP_PHAT_TAIL_SAMPLES   (SRP_PHAT_FFT_LENGTH - SRP_PHAT_FRAME_ADVANCE)
#define SRP_PHAT_SPEC_BINS      ((SRP_PHAT_FFT_LENGTH / 2) + 1)
#define SRP_PHAT_MAX_LAG        (5)   // interpolation window half-width
#define SRP_PHAT_MAX_DIRS       (360)
#define SRP_PHAT_MAX_SOURCES    (8)

// ------------------------------------------------------------
// Configuration struct (set at init, resolution is adjustable)
// ------------------------------------------------------------

typedef struct {
    unsigned num_directions;     // Search grid size (e.g. 36, 72, 180, 360)
    unsigned max_sources;        // Max peaks to return (e.g. 3)
    unsigned min_peak_sep_deg;   // Min angular separation between peaks
    float    array_radius_m;     // Array radius in meters (0.0355)
    float    sample_rate_hz;     // Sample rate (16000.0)
    float    speed_of_sound;     // Speed of sound (343.0)
} srp_phat_config_t;

// ------------------------------------------------------------
// Per-source result
// ------------------------------------------------------------

typedef struct {
    float angle_rad;
    float angle_deg;
    float confidence;   // peak / mean of power map
} srp_source_t;

// ------------------------------------------------------------
// Estimator state
// ------------------------------------------------------------

typedef struct {
    // Config (set at init)
    srp_phat_config_t cfg;

    // Mic pair indices [6][2]: (i,j) for each unique pair
    int pairs[SRP_PHAT_NUM_PAIRS][2];

    // Precomputed steering delays [num_directions][num_pairs] in fractional samples
    float steering_delays[SRP_PHAT_MAX_DIRS][SRP_PHAT_NUM_PAIRS];

    // FFT overlap-save
    int32_t tail[SRP_PHAT_NUM_MICS][SRP_PHAT_TAIL_SAMPLES];
    int32_t td[SRP_PHAT_NUM_MICS][SRP_PHAT_FFT_LENGTH];
    complex_s32_t spec_mem[SRP_PHAT_NUM_MICS][SRP_PHAT_SPEC_BINS];
    bfp_complex_s32_t Spec[SRP_PHAT_NUM_MICS];

    // GCC-PHAT scratch (reused per pair)
    complex_s32_t z_mem[SRP_PHAT_SPEC_BINS];
    bfp_complex_s32_t Z;
    int32_t mag_mem[SRP_PHAT_SPEC_BINS];
    bfp_s32_t Mag;
    int32_t corr_buf[SRP_PHAT_FFT_LENGTH];
    bfp_s32_t CorrBuf;

    // Power map
    float power_map[SRP_PHAT_MAX_DIRS];

    // Results
    srp_source_t sources[SRP_PHAT_MAX_SOURCES];
    unsigned num_sources_found;

} srp_phat_state_t;

// ------------------------------------------------------------
// API
// ------------------------------------------------------------

/**
 * Initialise SRP-PHAT estimator.
 * Precomputes steering delays for the configured grid resolution.
 * Must be called once before processing frames.
 */
void srp_phat_init(
    srp_phat_state_t *state,
    const srp_phat_config_t *cfg);

/**
 * Process one frame of 4-mic audio.
 *
 * Input layout:
 *   input[0..239]     = mic0
 *   input[240..479]   = mic1
 *   input[480..719]   = mic2
 *   input[720..959]   = mic3
 *
 * Results stored in state->sources[0..num_sources_found-1].
 */
void srp_phat_process_frame(
    srp_phat_state_t *state,
    const int32_t *input,
    exponent_t in_exp);

/**
 * Get source results after processing a frame.
 */
static inline const srp_source_t *srp_phat_get_sources(
    const srp_phat_state_t *state)
{
    return state->sources;
}

#ifdef __cplusplus
}
#endif

#endif // SRP_PHAT_H_
```

- [ ] **Step 2: Commit**

```bash
git add modules/fph/doa/api/srp_phat.h
git commit -m "feat(doa): add SRP-PHAT header with types, config, and API"
```

---

### Task 2: Create `srp_phat.c`

**Files:**
- Create: `modules/fph/doa/src/srp_phat.c`

- [ ] **Step 1: Write the implementation file**

```c
// SRP-PHAT DOA estimator for 4-mic circular array
//
// Algorithm:
//   1. FFT each mic (overlap-save, same as GCC-PHAT)
//   2. For each of 6 mic pairs, compute PHAT-weighted cross-correlation (IFFT)
//   3. For each steering direction, interpolate cross-correlations at theoretical
//      TDOAs and sum to get steered response power
//   4. Peak search for top-N sources with minimum angular separation
//
// Mic geometry:
//   mic0 at 0°, mic1 at 90°, mic2 at 180°, mic3 at 270° on circle of radius r

#include "srp_phat.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef GCC_PHAT_EPS_MANT
#define GCC_PHAT_EPS_MANT (1)
#endif

// 6 unique pairs from 4 mics
static const int PAIRS[SRP_PHAT_NUM_PAIRS][2] = {
    {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}
};

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------

void srp_phat_init(
    srp_phat_state_t *state,
    const srp_phat_config_t *cfg)
{
    memset(state, 0, sizeof(*state));
    state->cfg = *cfg;

    memcpy(state->pairs, PAIRS, sizeof(PAIRS));

    // Precompute mic positions on circle
    const float r = cfg->array_radius_m;
    float mic_xy[SRP_PHAT_NUM_MICS][2];
    const float mic_angles[4] = {
        0.0f, (float)M_PI / 2.0f, (float)M_PI, 3.0f * (float)M_PI / 2.0f
    };
    for (int m = 0; m < SRP_PHAT_NUM_MICS; m++) {
        mic_xy[m][0] = r * cosf(mic_angles[m]);
        mic_xy[m][1] = r * sinf(mic_angles[m]);
    }

    // Precompute steering delays: τ_ij(θ) = (fs/c) · (Δx cosθ + Δy sinθ)
    const float scale = cfg->sample_rate_hz / cfg->speed_of_sound;

    for (unsigned d = 0; d < cfg->num_directions; d++) {
        float theta = 2.0f * (float)M_PI * (float)d / (float)cfg->num_directions;
        float cos_t = cosf(theta);
        float sin_t = sinf(theta);

        for (int p = 0; p < SRP_PHAT_NUM_PAIRS; p++) {
            int i = PAIRS[p][0];
            int j = PAIRS[p][1];
            float dx = mic_xy[j][0] - mic_xy[i][0];
            float dy = mic_xy[j][1] - mic_xy[i][1];
            state->steering_delays[d][p] = scale * (dx * cos_t + dy * sin_t);
        }
    }

    // Init BFP wrappers
    for (unsigned m = 0; m < SRP_PHAT_NUM_MICS; m++) {
        bfp_complex_s32_init(&state->Spec[m], state->spec_mem[m],
                             0, SRP_PHAT_SPEC_BINS, 1);
    }

    bfp_complex_s32_init(&state->Z, state->z_mem, 0, SRP_PHAT_SPEC_BINS, 1);
    bfp_s32_init(&state->Mag, state->mag_mem, 0, SRP_PHAT_SPEC_BINS, 1);
    bfp_s32_init(&state->CorrBuf, state->corr_buf, 0, SRP_PHAT_FFT_LENGTH, 1);

    state->num_sources_found = 0;
}

// ------------------------------------------------------------
// GCC-PHAT cross-correlation for one pair (no peak search)
// ------------------------------------------------------------

static void compute_gcc_phat_corr(
    srp_phat_state_t *state,
    const bfp_complex_s32_t *A,
    const bfp_complex_s32_t *B)
{
    const unsigned K = SRP_PHAT_SPEC_BINS;
    bfp_complex_s32_t *Z = &state->Z;
    bfp_s32_t *mag = &state->Mag;
    bfp_s32_t *corr = &state->CorrBuf;

    // Z = A * conj(B)
    exponent_t z_exp;
    right_shift_t a_shr, b_shr;
    vect_complex_s32_conj_mul_prepare(&z_exp, &a_shr, &b_shr,
                                       A->exp, B->exp, A->hr, B->hr);
    vect_complex_s32_conj_mul(Z->data, A->data, B->data, K, a_shr, b_shr);
    Z->exp = z_exp;
    Z->hr = bfp_complex_s32_headroom(Z);

    // Zero DC & Nyquist to avoid division sensitivity
    Z->data[0].re = 0; Z->data[0].im = 0;
    Z->data[K-1].re = 0; Z->data[K-1].im = 0;

    // mag = |Z|
    bfp_complex_s32_mag(mag, Z);

    // Clamp magnitude to epsilon
    for (unsigned k = 0; k < K; k++) {
        if (mag->data[k] == 0) mag->data[k] = GCC_PHAT_EPS_MANT;
    }
    mag->hr = bfp_s32_headroom(mag);

    // PHAT: Z /= |Z|
    bfp_s32_t w = *mag;
    bfp_s32_inverse(&w, mag);
    bfp_complex_s32_real_mul(Z, Z, &w);

    // IFFT: packed -> inverse -> correlation in CorrBuf
    const uint32_t zlen = Z->length;
    bfp_fft_pack_mono(Z);
    bfp_s32_t *tmp = bfp_fft_inverse_mono(Z);
    memcpy(corr, tmp, sizeof(*corr));
    Z->length = zlen;
}

// ------------------------------------------------------------
// Convert BFP correlation to float window around lag 0
// ------------------------------------------------------------

// window[0..2*MAX_LAG] represents lags [-MAX_LAG .. +MAX_LAG]
static void corr_to_float_window(
    const bfp_s32_t *corr,
    float *window)
{
    const int N = SRP_PHAT_FFT_LENGTH;
    exponent_t exp = corr->exp;
    for (int lag = -SRP_PHAT_MAX_LAG; lag <= SRP_PHAT_MAX_LAG; lag++) {
        int idx = (lag >= 0) ? lag : (N + lag);
        window[lag + SRP_PHAT_MAX_LAG] = ldexpf((float)corr->data[idx], exp);
    }
}

// ------------------------------------------------------------
// Linear interpolation at fractional lag
// ------------------------------------------------------------

static float interp_at_lag(const float *window, float tau)
{
    float idx_f = tau + (float)SRP_PHAT_MAX_LAG;

    // Clamp to window bounds
    if (idx_f < 0.0f) idx_f = 0.0f;
    if (idx_f > (float)(2 * SRP_PHAT_MAX_LAG)) idx_f = (float)(2 * SRP_PHAT_MAX_LAG);

    int i0 = (int)floorf(idx_f);
    int i1 = i0 + 1;
    if (i1 > 2 * SRP_PHAT_MAX_LAG) i1 = 2 * SRP_PHAT_MAX_LAG;

    float frac = idx_f - (float)i0;
    return window[i0] * (1.0f - frac) + window[i1] * frac;
}

// ------------------------------------------------------------
// Process one frame
// ------------------------------------------------------------

void srp_phat_process_frame(
    srp_phat_state_t *state,
    const int32_t *input,
    exponent_t in_exp)
{
    const unsigned N = SRP_PHAT_FFT_LENGTH;
    const unsigned num_dir = state->cfg.num_directions;

    // 1) Build FFT frames: tail[16] + new[240]
    for (unsigned m = 0; m < SRP_PHAT_NUM_MICS; m++) {
        memcpy(&state->td[m][0], &state->tail[m][0],
               SRP_PHAT_TAIL_SAMPLES * sizeof(int32_t));
        memcpy(&state->td[m][SRP_PHAT_TAIL_SAMPLES],
               &input[m * SRP_PHAT_FRAME_ADVANCE],
               SRP_PHAT_FRAME_ADVANCE * sizeof(int32_t));
        memcpy(&state->tail[m][0],
               &state->td[m][N - SRP_PHAT_TAIL_SAMPLES],
               SRP_PHAT_TAIL_SAMPLES * sizeof(int32_t));
    }

    // 2) FFT each mic (real mono forward + unpack)
    for (unsigned m = 0; m < SRP_PHAT_NUM_MICS; m++) {
        bfp_s32_t td_bfp;
        bfp_s32_init(&td_bfp, state->td[m], in_exp, N, 1);
        const uint32_t len = td_bfp.length;
        bfp_complex_s32_t *tmp = bfp_fft_forward_mono(&td_bfp);
        tmp->hr = bfp_complex_s32_headroom(tmp);
        memcpy(&state->Spec[m], tmp, sizeof(bfp_complex_s32_t));
        bfp_fft_unpack_mono(&state->Spec[m]);
        td_bfp.length = len;
    }

    // 3) Zero power map
    for (unsigned d = 0; d < num_dir; d++) {
        state->power_map[d] = 0.0f;
    }

    // 4) For each mic pair: compute GCC-PHAT correlation,
    //    interpolate at steering delays, accumulate into power map
    float window[2 * SRP_PHAT_MAX_LAG + 1];

    for (int p = 0; p < SRP_PHAT_NUM_PAIRS; p++) {
        int i = state->pairs[p][0];
        int j = state->pairs[p][1];

        compute_gcc_phat_corr(state, &state->Spec[i], &state->Spec[j]);
        corr_to_float_window(&state->CorrBuf, window);

        for (unsigned d = 0; d < num_dir; d++) {
            float tau = state->steering_delays[d][p];
            state->power_map[d] += interp_at_lag(window, tau);
        }
    }

    // 5) Compute mean power for confidence normalization
    float mean_power = 0.0f;
    for (unsigned d = 0; d < num_dir; d++) {
        mean_power += state->power_map[d];
    }
    mean_power /= (float)num_dir;

    // 6) Peak search: find top-N with minimum angular separation
    unsigned sep_bins = (unsigned)((float)state->cfg.min_peak_sep_deg
                                   * (float)num_dir / 360.0f) + 1;
    unsigned found = 0;

    for (unsigned s = 0; s < state->cfg.max_sources; s++) {
        // Find global max
        float best = -1e30f;
        unsigned best_d = 0;
        for (unsigned d = 0; d < num_dir; d++) {
            if (state->power_map[d] > best) {
                best = state->power_map[d];
                best_d = d;
            }
        }

        if (best < -1e29f) break;  // all masked

        float angle = 2.0f * (float)M_PI * (float)best_d / (float)num_dir;
        state->sources[s].angle_rad = angle;
        state->sources[s].angle_deg = angle * (180.0f / (float)M_PI);
        state->sources[s].confidence = (mean_power > 1e-12f)
                                        ? (best / mean_power) : 0.0f;
        found++;

        // Mask neighborhood to enforce minimum separation
        for (unsigned dd = 0; dd <= 2 * sep_bins; dd++) {
            int idx = (int)best_d - (int)sep_bins + (int)dd;
            idx = idx % (int)num_dir;
            if (idx < 0) idx += (int)num_dir;
            state->power_map[idx] = -1e30f;
        }
    }

    // Zero-fill remaining source slots
    for (unsigned s = found; s < state->cfg.max_sources; s++) {
        state->sources[s].angle_rad = 0.0f;
        state->sources[s].angle_deg = 0.0f;
        state->sources[s].confidence = 0.0f;
    }

    state->num_sources_found = found;
}
```

- [ ] **Step 2: Commit**

```bash
git add modules/fph/doa/src/srp_phat.c
git commit -m "feat(doa): add SRP-PHAT implementation (init + process_frame)"
```

---

### Task 3: Update `CMakeLists.txt`

**Files:**
- Modify: `modules/fph/doa/CMakeLists.txt`

- [ ] **Step 1: Add `src/srp_phat.c` to the library sources**

In `modules/fph/doa/CMakeLists.txt`, add `src/srp_phat.c` to the `target_sources` list. The current file lists `src/gcc_phat.c` and `src/doa_led.c`. Add the new source on the line after `src/doa_led.c`:

Change:
```cmake
    target_sources(lib_doa
        INTERFACE
            src/gcc_phat.c
            src/doa_led.c
    )
```

To:
```cmake
    target_sources(lib_doa
        INTERFACE
            src/gcc_phat.c
            src/doa_led.c
            src/srp_phat.c
    )
```

- [ ] **Step 2: Commit**

```bash
git add modules/fph/doa/CMakeLists.txt
git commit -m "build(doa): add srp_phat.c to lib_doa sources"
```

---

### Task 4: Update `batch_processor.c`

**Files:**
- Modify: `satellite-xmos-firmware/src/batch_processor.c`

- [ ] **Step 1: Add SRP-PHAT include and state declarations**

After the existing `#include "gcc_phat.h"` at line 20, add:

```c
#include "srp_phat.h"
```

After the existing DOA state declarations (lines 57-59), add the SRP-PHAT state and buffers:

```c
/* SRP-PHAT DOA */
static srp_phat_state_t srp_state;
static srp_phat_state_t srp_raw_state;
static int32_t DWORD_ALIGNED srp_input[4 * FRAME_ADVANCE];
```

- [ ] **Step 2: Initialize SRP-PHAT in `init_dsp()`**

At the end of `init_dsp()`, after `doa4_init(&doa_raw_state);` (line 123), add:

```c
    /* SRP-PHAT DOA — 72 directions, 3 sources, 15° min separation */
    srp_phat_config_t srp_cfg = {
        .num_directions   = 72,
        .max_sources      = 3,
        .min_peak_sep_deg = 15,
        .array_radius_m   = 0.0355f,
        .sample_rate_hz   = 16000.0f,
        .speed_of_sound   = 343.0f,
    };
    srp_phat_init(&srp_state, &srp_cfg);
    srp_phat_init(&srp_raw_state, &srp_cfg);
```

- [ ] **Step 3: Add SRP-PHAT output file opens**

After the existing DOA output file opens (line 188), add:

```c
    xscope_file_t out_srp_doa     = xscope_open_file("output_srp_doa.bin", "wb");
    xscope_file_t out_srp_doa_raw = xscope_open_file("output_srp_doa_raw.bin", "wb");
```

- [ ] **Step 4: Add SRP-PHAT processing stage after existing Stage 5**

After the existing Stage 5 DOA processing block (after line 293), add:

```c
        /* ---- Stage 5b: SRP-PHAT DOA ---- */
        /* SRP-PHAT on raw mic input */
        for (int ch = 0; ch < 4; ch++) {
            memcpy(&srp_input[ch * FRAME_ADVANCE], mic[ch],
                   FRAME_ADVANCE * sizeof(int32_t));
        }
        srp_phat_process_frame(&srp_raw_state, srp_input, -31);
        xscope_fwrite(&out_srp_doa_raw, (uint8_t *)srp_raw_state.sources,
                      srp_raw_state.cfg.max_sources * sizeof(srp_source_t));

        /* SRP-PHAT on AEC output */
        for (int ch = 0; ch < 4; ch++) {
            memcpy(&srp_input[ch * FRAME_ADVANCE], aec_out[ch],
                   FRAME_ADVANCE * sizeof(int32_t));
        }
        srp_phat_process_frame(&srp_state, srp_input, -31);
        xscope_fwrite(&out_srp_doa, (uint8_t *)srp_state.sources,
                      srp_state.cfg.max_sources * sizeof(srp_source_t));
```

Also update the diagnostic print block (lines 296-300). After the existing `if (f < 5)` block for GCC-PHAT, add SRP-PHAT diagnostics inside the same `if (f < 5)` guard:

```c
        if (f < 5) {
            printf("    SRP sources (raw): %u\n", srp_raw_state.num_sources_found);
            for (unsigned s = 0; s < srp_raw_state.num_sources_found; s++) {
                printf("      src%u: %.1f deg (conf %.2f)\n",
                       s, srp_raw_state.sources[s].angle_deg,
                       srp_raw_state.sources[s].confidence);
            }
            printf("    SRP sources (aec): %u\n", srp_state.num_sources_found);
            for (unsigned s = 0; s < srp_state.num_sources_found; s++) {
                printf("      src%u: %.1f deg (conf %.2f)\n",
                       s, srp_state.sources[s].angle_deg,
                       srp_state.sources[s].confidence);
            }
        }
```

- [ ] **Step 5: Commit**

```bash
git add satellite-xmos-firmware/src/batch_processor.c
git commit -m "feat(batch): add SRP-PHAT DOA stage alongside GCC-PHAT"
```

---

### Task 5: Update `run_batch_test.py`

**Files:**
- Modify: `scripts/run_batch_test.py`

- [ ] **Step 1: Add SRP-PHAT CSV conversion**

After the existing DOA binary-to-CSV conversion block (lines 212-225), add:

```python
    # Convert SRP-PHAT DOA binary to CSV
    srp_max_sources = 3
    srp_source_size = struct.calcsize('<fff')  # angle_rad, angle_deg, confidence = 12 bytes
    srp_frame_size = srp_max_sources * srp_source_size

    for srp_bin, srp_csv, label in [
        ('output_srp_doa.bin', 'output_srp_doa.csv', 'SRP-PHAT DOA (AEC)'),
        ('output_srp_doa_raw.bin', 'output_srp_doa_raw.csv', 'SRP-PHAT DOA (raw mic)'),
    ]:
        if os.path.isfile(srp_bin):
            data = open(srp_bin, 'rb').read()
            n_frames = len(data) // srp_frame_size
            with open(srp_csv, 'w') as f:
                f.write('frame,source,confidence,angle_rad,angle_deg\n')
                for frame in range(n_frames):
                    for src in range(srp_max_sources):
                        offset = frame * srp_frame_size + src * srp_source_size
                        angle_rad, angle_deg, confidence = struct.unpack(
                            '<fff', data[offset:offset + srp_source_size])
                        f.write(f'{frame},{src},{confidence:.4f},{angle_rad:.6f},{angle_deg:.2f}\n')
            print(f"  {srp_csv} ({label}, {n_frames} frames)")
        else:
            print(f"  {srp_bin} NOT FOUND")
```

- [ ] **Step 2: Commit**

```bash
git add scripts/run_batch_test.py
git commit -m "feat(scripts): add SRP-PHAT DOA binary to CSV conversion"
```

---

### Task 6: Build and verify

- [ ] **Step 1: Build the batch target**

```bash
cd /workspace && make -C build sq66_fileio_batch -j16 2>&1
```

Expected: Build succeeds with no errors. `srp_phat.c` compiles and links into the firmware.

- [ ] **Step 2: Fix any compilation errors**

If the build fails, read the error output carefully. Common issues:
- Missing `#include <math.h>` for `cosf`, `sinf`, `atan2f`, `floorf`, `ldexpf`, `sqrtf`, `fabsf`
- Missing `#include <string.h>` for `memcpy`, `memset`
- Type mismatches in BFP API calls (check `const` qualifiers)
- `DWORD_ALIGNED` attribute on `srp_phat_state_t` if the struct size is odd

- [ ] **Step 3: Commit any build fixes**

```bash
git add -u
git commit -m "fix(doa): build fixes for SRP-PHAT integration"
```

---

## Self-Review

**1. Spec coverage:**
- Multi-source (top-N peak search): Task 2 `srp_phat.c` peak search section
- Configurable resolution: Task 2 `srp_phat_init` precomputes steering delays from `cfg->num_directions`
- Per-source output (angle_rad, angle_deg, confidence): Task 2 `srp_source_t`, Task 5 CSV conversion
- Binary output + host CSV: Task 4 batch output, Task 5 Python conversion
- Runs alongside GCC-PHAT: Task 4 adds Stage 5b after Stage 5

**2. Placeholder scan:** No TBDs, TODOs, or placeholder steps. All code blocks contain complete implementation.

**3. Type consistency:**
- `srp_source_t` defined in `srp_phat.h` (Task 1) used consistently in `srp_phat.c` (Task 2), `batch_processor.c` (Task 4), and `run_batch_test.py` (Task 5)
- `srp_phat_config_t` fields match between init call in Task 4 and struct definition in Task 1
- `srp_phat_state_t` field names consistent between header (Task 1) and implementation (Task 2)
- BFP types (`bfp_complex_s32_t`, `bfp_s32_t`, `complex_s32_t`) match lib_xcore_math signatures
