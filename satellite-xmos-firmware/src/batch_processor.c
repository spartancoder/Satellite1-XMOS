// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xcore/chanend.h>

/* xscope_fileio */
#include "xscope_io_device.h"

/* DSP libraries — audio_pipeline_dsp.h includes aec_api.h, aec_memory_pool.h,
 * ic_api.h, ns_api.h, agc_api.h, vnr headers, and type definitions. */
#include "audio_pipeline_dsp.h"
#include "agc_profiles.h"

/* DOA */
#include "gcc_phat.h"
#include "srp_phat.h"

/* Local */
#include "batch_processor.h"
#include "wav_utils.h"

/* ---------- Constants ---------- */

#define FRAME_ADVANCE     240
#define SAMPLE_RATE       16000
#define BIT_DEPTH         32
#define MAX_INPUT_CHANS   6   /* ref_0, ref_1, mic_0..mic_3 */
#define MAX_AEC_Y_CHANS   4
#define MAX_AEC_X_CHANS   2
#define AEC_MAIN_PHASES   7
#define VNR_AGC_THRESHOLD (0.5f)

/* ---------- DSP State (static, DWORD_ALIGNED) ---------- */

/* AEC — shadow filter kept at 0 phases to satisfy aec_process_frame_1thread API */
static aec_state_t DWORD_ALIGNED aec_main_state;
static aec_state_t DWORD_ALIGNED aec_shadow_state;
static aec_shared_state_t DWORD_ALIGNED aec_shared_state;
static uint8_t DWORD_ALIGNED aec_main_pool[sizeof(aec_memory_pool_t)];
static uint8_t DWORD_ALIGNED aec_shadow_pool[sizeof(aec_shadow_filt_memory_pool_t)];

/* IC + VNR */
static ic_state_t DWORD_ALIGNED ic_state;
static vnr_pred_state_t vnr_pred_state;

/* NS */
static ns_state_t DWORD_ALIGNED ns_state;

/* AGC */
static agc_state_t DWORD_ALIGNED agc_state;

/* DOA */
static doa4_state_t doa_state;
static doa4_state_t doa_raw_state;
static int32_t DWORD_ALIGNED doa_input[4 * FRAME_ADVANCE];

/* SRP-PHAT DOA */
static srp_phat_state_t srp_state;
static srp_phat_state_t srp_raw_state;
static int32_t DWORD_ALIGNED srp_input[4 * FRAME_ADVANCE];

/* Working buffers (static to avoid stack overflow — ~26KB total) */
static int32_t DWORD_ALIGNED read_buf[MAX_INPUT_CHANS * FRAME_ADVANCE];
static int32_t DWORD_ALIGNED deinterleaved[MAX_INPUT_CHANS][FRAME_ADVANCE];
static int32_t ref[MAX_AEC_X_CHANS][FRAME_ADVANCE];
static int32_t mic[4][FRAME_ADVANCE];
static int32_t DWORD_ALIGNED aec_out[MAX_AEC_Y_CHANS][FRAME_ADVANCE];
static int32_t DWORD_ALIGNED aec_shadow_out[MAX_AEC_Y_CHANS][FRAME_ADVANCE];
static int32_t DWORD_ALIGNED ic_out_buf[FRAME_ADVANCE];
static int32_t DWORD_ALIGNED ns_out_buf[FRAME_ADVANCE];
static int32_t DWORD_ALIGNED agc_out_buf[FRAME_ADVANCE];
static int32_t DWORD_ALIGNED write_buf_4ch[4 * FRAME_ADVANCE];
static int32_t DWORD_ALIGNED write_buf_2ch[2 * FRAME_ADVANCE];

/* ---------- Helpers ---------- */

/* Deinterleave interleaved PCM data into separate channel buffers.
 * interleaved: [s0_ch0, s0_ch1, ..., s0_chN, s1_ch0, ...]
 * outputs:     channels[0..num_ch-1], each with num_samples elements */
static void deinterleave(const int32_t *interleaved,
                         int32_t outputs[][FRAME_ADVANCE],
                         int num_ch, int num_samples)
{
    for (int s = 0; s < num_samples; s++) {
        for (int ch = 0; ch < num_ch; ch++) {
            outputs[ch][s] = interleaved[s * num_ch + ch];
        }
    }
}

/* Interleave separate channel buffers into a flat PCM buffer. */
static void interleave(const int32_t channels[][FRAME_ADVANCE],
                       int32_t *interleaved,
                       int num_ch, int num_samples)
{
    for (int s = 0; s < num_samples; s++) {
        for (int ch = 0; ch < num_ch; ch++) {
            interleaved[s * num_ch + ch] = channels[ch][s];
        }
    }
}

/* ---------- DSP Init ---------- */

static void init_dsp(void)
{
    /* AEC — shadow filter at 0 phases to reduce memory for 4-channel mode */
    aec_init(&aec_main_state, &aec_shadow_state, &aec_shared_state,
             &aec_main_pool[0], &aec_shadow_pool[0],
             MAX_AEC_Y_CHANS, MAX_AEC_X_CHANS,
             AEC_MAIN_PHASES, 0 /* shadow phases */);

    /* IC + VNR */
    ic_init(&ic_state);

    /* NS */
    ns_init(&ns_state);

    /* AGC */
    agc_init(&agc_state, &AGC_PROFILE_ASR);

    /* DOA */
    doa4_init(&doa_state);
    doa4_init(&doa_raw_state);

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
}

/* ---------- Main ---------- */

void batch_process(chanend_t c_xscope)
{
    printf("Batch processor starting...\n");

    /* Initialize xscope file I/O */
    printf("Before xscope_io_init...\n");
    xscope_io_init(c_xscope);
    printf("xscope_io_init done\n");

    /* Initialize DSP */
    printf("Before init_dsp...\n");
    init_dsp();
    printf("init_dsp done\n");

    /* Open input file */
    xscope_file_t input_file = xscope_open_file("input.wav", "rb");

    /* Parse WAV header */
    wav_header_t input_hdr;
    unsigned header_size = 0;
    if (wav_read_header(&input_file, &input_hdr, &header_size) != 0) {
        printf("Error: failed to parse input WAV header\n");
        xscope_close_all_files();
        exit(1);
    }

    int num_input_ch = input_hdr.num_channels;
    int total_frames = wav_get_num_frames(&input_hdr);
    int num_batch_frames = total_frames / FRAME_ADVANCE;

    printf("Input: %d channels, %d Hz, %d-bit, %d total frames (%d batch frames)\n",
           num_input_ch, input_hdr.sample_rate, input_hdr.bit_depth,
           total_frames, num_batch_frames);

    if (num_input_ch < 4 || num_input_ch > 6) {
        printf("Error: expected 4-6 channels, got %d\n", num_input_ch);
        xscope_close_all_files();
        exit(1);
    }

    /* Map input channels to ref/mic.
     * Layout: [ref_0, ref_1, mic_0, mic_1, mic_2, mic_3]
     * 4ch -> mic only (ref = zero)
     * 5ch -> ref_0 + 4 mic (ref_1 = zero)
     * 6ch -> 2 ref + 4 mic */
    int num_ref = (num_input_ch > 4) ? (num_input_ch - 4) : 0;

    /* Open output files for each DSP stage */
    wav_header_t out_hdr_aec, out_hdr_ic, out_hdr_ns, out_hdr_agc;

    wav_form_header(&out_hdr_aec, MAX_AEC_Y_CHANS, SAMPLE_RATE, BIT_DEPTH, num_batch_frames * FRAME_ADVANCE);
    wav_form_header(&out_hdr_ic,  1, SAMPLE_RATE, BIT_DEPTH, num_batch_frames * FRAME_ADVANCE);
    wav_form_header(&out_hdr_ns,  1, SAMPLE_RATE, BIT_DEPTH, num_batch_frames * FRAME_ADVANCE);
    wav_form_header(&out_hdr_agc, 1, SAMPLE_RATE, BIT_DEPTH, num_batch_frames * FRAME_ADVANCE);

    xscope_file_t out_aec = xscope_open_file("output_aec.wav", "wb");
    xscope_file_t out_ic  = xscope_open_file("output_ic.wav",  "wb");
    xscope_file_t out_ns  = xscope_open_file("output_ns.wav",  "wb");
    xscope_file_t out_agc = xscope_open_file("output_agc.wav", "wb");
    xscope_file_t out_doa = xscope_open_file("output_doa.bin", "wb");
    xscope_file_t out_doa_raw = xscope_open_file("output_doa_raw.bin", "wb");
    xscope_file_t out_srp_doa     = xscope_open_file("output_srp_doa.bin", "wb");
    xscope_file_t out_srp_doa_raw = xscope_open_file("output_srp_doa_raw.bin", "wb");

    /* Write placeholder headers (will update data sizes at the end) */
    wav_write_header(&out_aec, &out_hdr_aec);
    wav_write_header(&out_ic,  &out_hdr_ic);
    wav_write_header(&out_ns,  &out_hdr_ns);
    wav_write_header(&out_agc, &out_hdr_agc);

    printf("Processing %d frames...\n", num_batch_frames);

    for (int f = 0; f < num_batch_frames; f++) {
        /* Read one batch frame (FRAME_ADVANCE samples * num_input_ch channels) */
        size_t bytes_to_read = (size_t)FRAME_ADVANCE * num_input_ch * sizeof(int32_t);
        size_t bytes_read = xscope_fread(&input_file, (uint8_t *)read_buf, bytes_to_read);
        if (bytes_read != bytes_to_read) {
            printf("Warning: short read at frame %d (%zu < %zu)\n",
                   f, bytes_read, bytes_to_read);
            break;
        }

        /* Deinterleave into per-channel arrays */
        deinterleave(read_buf, deinterleaved, num_input_ch, FRAME_ADVANCE);

        /* Split into ref and mic channels */
        memset(ref, 0, sizeof(ref));
        memset(mic, 0, sizeof(mic));

        /* Ref channels: first num_ref channels from input */
        for (int ch = 0; ch < num_ref && ch < MAX_AEC_X_CHANS; ch++) {
            memcpy(ref[ch], deinterleaved[ch], FRAME_ADVANCE * sizeof(int32_t));
        }

        /* Mic channels: remaining channels from input */
        for (int ch = 0; ch < 4; ch++) {
            int src_ch = num_ref + ch;
            if (src_ch < num_input_ch) {
                memcpy(mic[ch], deinterleaved[src_ch], FRAME_ADVANCE * sizeof(int32_t));
            }
        }

        /* ---- Stage 1: AEC (4 mic channels) ---- */
        /* Diagnostic: print first few mic samples to verify channel data */
        if (f == 0) {
            printf("  Mic samples [0..3] at frame 0:\n");
            for (int ch = 0; ch < 4; ch++) {
                printf("    mic[%d]: %d %d %d %d %d\n", ch,
                       mic[ch][0], mic[ch][1], mic[ch][2], mic[ch][3], mic[ch][4]);
            }
        }
        aec_process_frame_1thread(
            &aec_main_state, &aec_shadow_state,
            aec_out, aec_shadow_out,
            (const int32_t (*)[FRAME_ADVANCE])mic,      /* y_data: mic[0..3] */
            (const int32_t (*)[FRAME_ADVANCE])ref);      /* x_data: ref[0..1] */

        /* Write AEC output (4 channels) */
        interleave(aec_out, write_buf_4ch, MAX_AEC_Y_CHANS, FRAME_ADVANCE);
        xscope_fwrite(&out_aec, (uint8_t *)write_buf_4ch,
                      MAX_AEC_Y_CHANS * FRAME_ADVANCE * sizeof(int32_t));

        /* ---- Stage 2: IC + VNR ---- */
        ic_filter(&ic_state, aec_out[0], aec_out[1], ic_out_buf);
        ic_calc_vnr_pred(&ic_state,
                         &vnr_pred_state.input_vnr_pred,
                         &vnr_pred_state.output_vnr_pred);
        ic_adapt(&ic_state, vnr_pred_state.input_vnr_pred);

        /* Write IC output (1 channel) */
        xscope_fwrite(&out_ic, (uint8_t *)ic_out_buf,
                      FRAME_ADVANCE * sizeof(int32_t));

        /* ---- Stage 3: NS ---- */
        ns_process_frame(&ns_state, ns_out_buf, ic_out_buf);

        /* Write NS output (1 channel) */
        xscope_fwrite(&out_ns, (uint8_t *)ns_out_buf,
                      FRAME_ADVANCE * sizeof(int32_t));

        /* ---- Stage 4: AGC ---- */
        agc_meta_data_t agc_md;
        agc_md.vnr_flag = AGC_META_DATA_NO_VNR;
        agc_md.aec_ref_power = AGC_META_DATA_NO_AEC;
        agc_md.aec_corr_factor = AGC_META_DATA_NO_AEC;

        agc_process_frame(&agc_state, agc_out_buf, ns_out_buf, &agc_md);

        /* Write AGC output (1 channel) */
        xscope_fwrite(&out_agc, (uint8_t *)agc_out_buf,
                      FRAME_ADVANCE * sizeof(int32_t));

        /* ---- Stage 5: DOA ---- */
        /* DOA on raw mic input */
        for (int ch = 0; ch < 4; ch++) {
            memcpy(&doa_input[ch * FRAME_ADVANCE], mic[ch],
                   FRAME_ADVANCE * sizeof(int32_t));
        }
        float doa_raw = doa4_process_frame(&doa_raw_state, doa_input, -31);
        xscope_fwrite(&out_doa_raw, (uint8_t *)&doa_raw, sizeof(float));

        /* DOA on AEC output */
        for (int ch = 0; ch < 4; ch++) {
            memcpy(&doa_input[ch * FRAME_ADVANCE], aec_out[ch],
                   FRAME_ADVANCE * sizeof(int32_t));
        }
        float doa_angle = doa4_process_frame(&doa_state, doa_input, -31);
        xscope_fwrite(&out_doa, (uint8_t *)&doa_angle, sizeof(float));

        /* Print first few DOA results for diagnostics */
        if (f < 5) {
            printf("  Frame %d: doa_raw=%.4f rad (%.1f deg), doa_aec=%.4f rad (%.1f deg)\n",
                   f, doa_raw, doa_raw * 57.2957795f,
                   doa_angle, doa_angle * 57.2957795f);
        }

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

        /* SRP-PHAT diagnostics */
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

        /* Progress every 100 frames */
        if ((f + 1) % 100 == 0 || f == num_batch_frames - 1) {
            printf("  Frame %d / %d\n", f + 1, num_batch_frames);
        }
    }

    /* Update WAV data sizes in case we didn't process all frames */
    int frames_processed = num_batch_frames;
    wav_update_data_size(&out_aec, frames_processed * MAX_AEC_Y_CHANS * sizeof(int32_t) * FRAME_ADVANCE);
    wav_update_data_size(&out_ic,  frames_processed * 1 * sizeof(int32_t) * FRAME_ADVANCE);
    wav_update_data_size(&out_ns,  frames_processed * 1 * sizeof(int32_t) * FRAME_ADVANCE);
    wav_update_data_size(&out_agc, frames_processed * 1 * sizeof(int32_t) * FRAME_ADVANCE);

    printf("Batch processing complete. %d frames processed.\n", frames_processed);

    /* Close all files (also signals host to exit) */
    xscope_close_all_files();

    /* Terminate */
    _Exit(0);
}
