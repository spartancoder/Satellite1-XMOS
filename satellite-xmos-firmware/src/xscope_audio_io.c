// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "xscope_audio_io.h"

#if appconfXSCOPE_4MIC_ENABLED

#include <xscope.h>
#include <string.h>

/*
 * Probe ID constants are auto-generated from config-xscope-4mic.xscope.
 * The XML generates #define constants matching each probe name.
 * Audio probes use xscope_bytes() to send full frames.
 * Metadata probes use xscope_float() for scalar values.
 */

/* Frame size constants */
#define CH_FRAME_BYTES  (XSCOPE_AUDIO_IO_FRAME_ADVANCE * sizeof(int32_t))

/* ---- Init ---- */

void xscope_audio_io_init(void)
{
    xscope_config_io(XSCOPE_IO_BASIC);
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
    xscope_bytes(mic_raw_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(mic_raw_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(mic_raw_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(mic_raw_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

void xscope_audio_io_send_gain_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(mic_gain_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(mic_gain_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(mic_gain_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(mic_gain_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

void xscope_audio_io_send_aec_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(mic_aec_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(mic_aec_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(mic_aec_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(mic_aec_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

void xscope_audio_io_send_aec_residual(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(aec_residual_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(aec_residual_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
    xscope_bytes(aec_residual_2, CH_FRAME_BYTES, (const unsigned char *)samples[2]);
    xscope_bytes(aec_residual_3, CH_FRAME_BYTES, (const unsigned char *)samples[3]);
}

/* ---- Tile 0: Audio observation points ---- */

void xscope_audio_io_send_ic_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(ic_out_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(ic_out_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_ic_residual(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(ic_residual_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(ic_residual_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_ns_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(ns_out_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(ns_out_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_agc_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    xscope_bytes(agc_out_0, CH_FRAME_BYTES, (const unsigned char *)samples[0]);
    xscope_bytes(agc_out_1, CH_FRAME_BYTES, (const unsigned char *)samples[1]);
}

void xscope_audio_io_send_beam(int beam_idx, const int32_t samples[XSCOPE_AUDIO_IO_FRAME_ADVANCE])
{
    switch (beam_idx) {
        case 0: xscope_bytes(beam_0, CH_FRAME_BYTES, (const unsigned char *)samples); break;
        case 1: xscope_bytes(beam_1, CH_FRAME_BYTES, (const unsigned char *)samples); break;
        case 2: xscope_bytes(beam_2, CH_FRAME_BYTES, (const unsigned char *)samples); break;
        default: break;
    }
}

/* ---- Tile 0: Metadata observation points ---- */

void xscope_audio_io_send_vnr_value(float vnr)
{
    xscope_float(vnr_value, vnr);
}

void xscope_audio_io_send_agc_gain(float gain)
{
    xscope_float(agc_gain, gain);
}

void xscope_audio_io_send_doa(float angle_smoothed, float angle_raw, float confidence)
{
    xscope_float(doa_angle, angle_smoothed);
    xscope_float(doa_angle_raw, angle_raw);
    xscope_float(doa_confidence, confidence);
}

void xscope_audio_io_send_vad(int beam_idx, float vad_value)
{
    switch (beam_idx) {
        case 0: xscope_float(vad_beam_0, vad_value); break;
        case 1: xscope_float(vad_beam_1, vad_value); break;
        case 2: xscope_float(vad_beam_2, vad_value); break;
        default: break;
    }
}

void xscope_audio_io_send_beam_selection(int selected_beam, int criteria)
{
    xscope_int(beam_selection, ((unsigned int)criteria << 16) | ((unsigned int)selected_beam & 0xFFFF));
}

#endif /* appconfXSCOPE_4MIC_ENABLED */
