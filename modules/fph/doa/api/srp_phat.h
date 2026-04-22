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
#define SRP_PHAT_MAX_LAG        (5)
#define SRP_PHAT_MAX_DIRS       (72)
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
