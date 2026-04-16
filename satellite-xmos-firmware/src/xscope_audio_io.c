// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "xscope_audio_io.h"

#if appconfXSCOPE_4MIC_ENABLED

#include <xscope.h>

/*
 * Probe ID constants — must match the order in config-xscope-4mic.xscope.
 * The .xscope XML defines probe names for the host-side tools;
 * on the target we use numeric IDs passed to xscope_bytes/float/int().
 */
enum {
    XSCOPE_ID_FREERTOS_TRACE = 0,
    XSCOPE_ID_PLL_FREQ,
    /* Tile 1: mic_raw (4 ch) */
    XSCOPE_ID_MIC_RAW_0,
    XSCOPE_ID_MIC_RAW_1,
    XSCOPE_ID_MIC_RAW_2,
    XSCOPE_ID_MIC_RAW_3,
    /* Tile 1: mic_gain (4 ch) */
    XSCOPE_ID_MIC_GAIN_0,
    XSCOPE_ID_MIC_GAIN_1,
    XSCOPE_ID_MIC_GAIN_2,
    XSCOPE_ID_MIC_GAIN_3,
    /* Tile 1: mic_aec (4 ch) */
    XSCOPE_ID_MIC_AEC_0,
    XSCOPE_ID_MIC_AEC_1,
    XSCOPE_ID_MIC_AEC_2,
    XSCOPE_ID_MIC_AEC_3,
    /* Tile 1: aec_residual (4 ch) */
    XSCOPE_ID_AEC_RESIDUAL_0,
    XSCOPE_ID_AEC_RESIDUAL_1,
    XSCOPE_ID_AEC_RESIDUAL_2,
    XSCOPE_ID_AEC_RESIDUAL_3,
    /* Tile 0: ic_out (2 ch) */
    XSCOPE_ID_IC_OUT_0,
    XSCOPE_ID_IC_OUT_1,
    /* Tile 0: ic_residual (2 ch) */
    XSCOPE_ID_IC_RESIDUAL_0,
    XSCOPE_ID_IC_RESIDUAL_1,
    /* Tile 0: ns_out (2 ch) */
    XSCOPE_ID_NS_OUT_0,
    XSCOPE_ID_NS_OUT_1,
    /* Tile 0: agc_out (2 ch) */
    XSCOPE_ID_AGC_OUT_0,
    XSCOPE_ID_AGC_OUT_1,
    /* Tile 0: beam (3 beams) */
    XSCOPE_ID_BEAM_0,
    XSCOPE_ID_BEAM_1,
    XSCOPE_ID_BEAM_2,
    /* Tile 0: metadata */
    XSCOPE_ID_VNR_VALUE,
    XSCOPE_ID_AGC_GAIN,
    XSCOPE_ID_DOA_ANGLE,
    XSCOPE_ID_DOA_ANGLE_RAW,
    XSCOPE_ID_DOA_CONFIDENCE,
    XSCOPE_ID_VAD_BEAM_0,
    XSCOPE_ID_VAD_BEAM_1,
    XSCOPE_ID_VAD_BEAM_2,
    XSCOPE_ID_BEAM_SELECTION,
    XSCOPE_ID_INPUT_SOURCE,
    XSCOPE_ID_COUNT
};

/* Frame size constants */
#define CH_FRAME_BYTES  (XSCOPE_AUDIO_IO_FRAME_ADVANCE * sizeof(int32_t))

/* ---- Init ---- */

void xscope_audio_io_init(void)
{
    xscope_config_io(XSCOPE_IO_BASIC);
    xscope_enable();
}

/* ---- Host input (injection deferred — always returns false) ---- */

bool xscope_audio_io_host_input_active(void)
{
    return false;
}

int xscope_audio_io_get_injected_frame(int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    (void)samples;
    return -1; /* No injected data available */
}

/* ---- Tile 1: Audio observation points ---- */

void xscope_audio_io_send_raw_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_MIC_RAW_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_MIC_RAW_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(XSCOPE_ID_MIC_RAW_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(XSCOPE_ID_MIC_RAW_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

void xscope_audio_io_send_gain_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_MIC_GAIN_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_MIC_GAIN_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(XSCOPE_ID_MIC_GAIN_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(XSCOPE_ID_MIC_GAIN_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

void xscope_audio_io_send_aec_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_MIC_AEC_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_MIC_AEC_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(XSCOPE_ID_MIC_AEC_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(XSCOPE_ID_MIC_AEC_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

void xscope_audio_io_send_aec_residual(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_AEC_RESIDUAL_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_AEC_RESIDUAL_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(XSCOPE_ID_AEC_RESIDUAL_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(XSCOPE_ID_AEC_RESIDUAL_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

/* ---- Tile 0: Audio observation points ---- */

void xscope_audio_io_send_ic_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_IC_OUT_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_IC_OUT_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_ic_residual(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_IC_RESIDUAL_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_IC_RESIDUAL_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_ns_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_NS_OUT_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_NS_OUT_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_agc_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(XSCOPE_ID_AGC_OUT_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(XSCOPE_ID_AGC_OUT_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_beam(int beam_idx, const int32_t samples[XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    switch (beam_idx) {
        case 0: xscope_bytes(XSCOPE_ID_BEAM_0, CH_FRAME_BYTES, (const unsigned char *)samples); break;
        case 1: xscope_bytes(XSCOPE_ID_BEAM_1, CH_FRAME_BYTES, (const unsigned char *)samples); break;
        case 2: xscope_bytes(XSCOPE_ID_BEAM_2, CH_FRAME_BYTES, (const unsigned char *)samples); break;
        default: break;
    }
}

/* ---- Tile 0: Metadata observation points ---- */

void xscope_audio_io_send_vnr_value(float vnr)
{
    xscope_float(XSCOPE_ID_VNR_VALUE, vnr);
}

void xscope_audio_io_send_agc_gain(float gain)
{
    xscope_float(XSCOPE_ID_AGC_GAIN, gain);
}

void xscope_audio_io_send_doa(float angle_smoothed, float angle_raw, float confidence)
{
    xscope_float(XSCOPE_ID_DOA_ANGLE, angle_smoothed);
    xscope_float(XSCOPE_ID_DOA_ANGLE_RAW, angle_raw);
    xscope_float(XSCOPE_ID_DOA_CONFIDENCE, confidence);
}

void xscope_audio_io_send_vad(int beam_idx, float vad_value)
{
    switch (beam_idx) {
        case 0: xscope_float(XSCOPE_ID_VAD_BEAM_0, vad_value); break;
        case 1: xscope_float(XSCOPE_ID_VAD_BEAM_1, vad_value); break;
        case 2: xscope_float(XSCOPE_ID_VAD_BEAM_2, vad_value); break;
        default: break;
    }
}

void xscope_audio_io_send_beam_selection(int selected_beam, int criteria)
{
    /* Packs selected_beam (bits 0-15) and criteria (bits 16-31) into a single int.
     * Host unpacks: beam = val & 0xFFFF, criteria = val >> 16 */
    xscope_int(XSCOPE_ID_BEAM_SELECTION, ((unsigned int)criteria << 16) | ((unsigned int)selected_beam & 0xFFFF));
}

#endif /* appconfXSCOPE_4MIC_ENABLED */
