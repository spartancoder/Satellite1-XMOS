// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef XSCOPE_AUDIO_IO_H_
#define XSCOPE_AUDIO_IO_H_

#include <stdint.h>
#include <stdbool.h>
#include "app_conf.h"

#if appconfXSCOPE_4MIC_ENABLED

#define XSCOPE_AUDIO_IO_FRAME_ADVANCE  appconfAUDIO_PIPELINE_FRAME_ADVANCE
#define XSCOPE_AUDIO_IO_NUM_MICS       appconfMIC_PIPELINE_INPUT_CHANNELS

void xscope_audio_io_init(void);
bool xscope_audio_io_host_input_active(void);
int xscope_audio_io_get_injected_frame(int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);

/* Tile 1 audio observation points */
void xscope_audio_io_send_raw_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);
void xscope_audio_io_send_gain_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);
void xscope_audio_io_send_aec_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);
void xscope_audio_io_send_aec_residual(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);

/* Tile 0 audio observation points */
void xscope_audio_io_send_ic_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);
void xscope_audio_io_send_ic_residual(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);
void xscope_audio_io_send_ns_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);
void xscope_audio_io_send_agc_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]);
void xscope_audio_io_send_beam(int beam_idx, const int32_t samples[XSCOPE_AUDIO_IO_FRAME_ADVANCE]);

/* Tile 0 metadata observation points */
void xscope_audio_io_send_vnr_value(float vnr);
void xscope_audio_io_send_agc_gain(float gain);
void xscope_audio_io_send_doa(float angle_smoothed, float angle_raw, float confidence);
void xscope_audio_io_send_vad(int beam_idx, float vad_value);
void xscope_audio_io_send_beam_selection(int selected_beam, int criteria);

#else /* appconfXSCOPE_4MIC_ENABLED == 0 — zero-overhead stubs */

#define XSCOPE_AUDIO_IO_FRAME_ADVANCE  appconfAUDIO_PIPELINE_FRAME_ADVANCE
#define XSCOPE_AUDIO_IO_NUM_MICS       appconfMIC_PIPELINE_INPUT_CHANNELS

static inline void xscope_audio_io_init(void) {}
static inline bool xscope_audio_io_host_input_active(void) { return false; }
static inline int xscope_audio_io_get_injected_frame(int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) {
    (void)samples; return -1;
}

static inline void xscope_audio_io_send_raw_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }
static inline void xscope_audio_io_send_gain_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }
static inline void xscope_audio_io_send_aec_mic(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }
static inline void xscope_audio_io_send_aec_residual(const int32_t samples[XSCOPE_AUDIO_IO_NUM_MICS][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }

static inline void xscope_audio_io_send_ic_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }
static inline void xscope_audio_io_send_ic_residual(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }
static inline void xscope_audio_io_send_ns_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }
static inline void xscope_audio_io_send_agc_out(const int32_t samples[2][XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)samples; }
static inline void xscope_audio_io_send_beam(int beam_idx, const int32_t samples[XSCOPE_AUDIO_IO_FRAME_ADVANCE]) { (void)beam_idx; (void)samples; }

static inline void xscope_audio_io_send_vnr_value(float vnr) { (void)vnr; }
static inline void xscope_audio_io_send_agc_gain(float gain) { (void)gain; }
static inline void xscope_audio_io_send_doa(float angle_smoothed, float angle_raw, float confidence) {
    (void)angle_smoothed; (void)angle_raw; (void)confidence;
}
static inline void xscope_audio_io_send_vad(int beam_idx, float vad_value) { (void)beam_idx; (void)vad_value; }
static inline void xscope_audio_io_send_beam_selection(int selected_beam, int criteria) { (void)selected_beam; (void)criteria; }

#endif /* appconfXSCOPE_4MIC_ENABLED */

#endif /* XSCOPE_AUDIO_IO_H_ */
