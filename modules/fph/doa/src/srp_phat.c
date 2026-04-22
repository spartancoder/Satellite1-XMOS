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

    // Validate config bounds
    if (cfg->num_directions == 0 || cfg->num_directions > SRP_PHAT_MAX_DIRS) return;
    if (cfg->max_sources == 0 || cfg->max_sources > SRP_PHAT_MAX_SOURCES) return;

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

    // Precompute steering delays: tau_ij(theta) = (fs/c) * (dx*cos_t + dy*sin_t)
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

        compute_gcc_phat_corr(state, &state->Spec[j], &state->Spec[i]);
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
