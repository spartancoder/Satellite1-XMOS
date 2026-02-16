// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef AUDIO_PIPELINE_DSP_H_
#define AUDIO_PIPELINE_DSP_H_

#include <stdint.h>
#include "app_conf.h"


/* Note: Changing the order here will effect the channel order for
 * audio_pipeline_input() and audio_pipeline_output()
 *
 * For 4-mic bypass:
 * - Internally captures 4 microphones
 * - Outputs 2 channels (first 2 mics) to ESP32
 * - Channel mapping:
 *   - samples[0]: Mic 0 output to ESP32
 *   - samples[1]: Mic 1 output to ESP32
 *   - samples[2]: Mic 2 (internal use only)
 *   - samples[3]: Mic 3 (internal use only)
 */
typedef struct {
    int32_t samples[appconfAUDIO_PIPELINE_CHANNELS][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
    int32_t aec_reference_audio_samples[2][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
    int32_t mic_samples_passthrough[appconfAUDIO_PIPELINE_CHANNELS][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
} frame_data_t;

#endif /* AUDIO_PIPELINE_DSP_H_ */
