// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

/* STD headers */
#include <string.h>
#include <stdint.h>

/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

/* Library headers */
#include "generic_pipeline.h"

/* App headers */
#include "app_conf.h"
#include "audio_pipeline.h"
#include "audio_pipeline_dsp.h"

#if appconfAUDIO_PIPELINE_FRAME_ADVANCE != 240
#error This pipeline is only configured for 240 frame advance
#endif

#if ON_TILE(0)

static void *audio_pipeline_input_i(void *input_app_data)
{
    frame_data_t *frame_data;

    frame_data = pvPortMalloc(sizeof(frame_data_t));
    memset(frame_data, 0x00, sizeof(frame_data_t));

    size_t bytes_received = 0;
    bytes_received = rtos_intertile_rx_len(
            intertile_ctx,
            appconfAUDIOPIPELINE_PORT,
            portMAX_DELAY);

    xassert(bytes_received == sizeof(frame_data_t));

    rtos_intertile_rx_data(
            intertile_ctx,
            frame_data,
            bytes_received);

    return frame_data;
}

static int audio_pipeline_output_i(frame_data_t *frame_data,
                                   void *output_app_data)
{
    // Output only first 2 microphones to ESP32 (2-channel I2S)
    // The audio_pipeline_output function expects 6 channels in the buffer:
    // [0] = AEC+IC+NS+AGC (output ch 0)
    // [1] = AEC (output ch 1)
    // [2-5] = reserved for internal use

    // For bypass mode, output first 2 mics:
    // - Channel 0: Mic 0
    // - Channel 1: Mic 1
    // - Channels 2-5: Duplicate mic 0 and mic 1 for compatibility with I2S output

    int32_t output_buffer[6][appconfAUDIO_PIPELINE_FRAME_ADVANCE];

    // Initialize output buffer to zero
    memset(output_buffer, 0, sizeof(output_buffer));

    // Copy first 2 mics to output channels 0 and 1
    memcpy(output_buffer[0], frame_data->samples[0], appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
    memcpy(output_buffer[1], frame_data->samples[1], appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));

    // Copy mics 2 and 3 to channels 4 and 5 (for reference/diagnostics)
    memcpy(output_buffer[4], frame_data->samples[2], appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
    memcpy(output_buffer[5], frame_data->samples[3], appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));

    // Duplicate first 2 mics to channels 2 and 3 (for AEC reference compatibility)
    memcpy(output_buffer[2], frame_data->samples[0], appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
    memcpy(output_buffer[3], frame_data->samples[1], appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));

    return audio_pipeline_output(output_app_data,
                               (int32_t **)output_buffer,
                               6,  // I2S expects 6 channels internally
                               appconfAUDIO_PIPELINE_FRAME_ADVANCE);
}

static void bypass_stage(void)
{
    // No processing - just pass-through
    ;
}

static void initialize_pipeline_stages(void)
{
    // No initialization needed for bypass mode
    ;
}

void audio_pipeline_init(
    void *input_app_data,
    void *output_app_data)
{
    const int stage_count = 2;

    const pipeline_stage_t stages[] = {
        (pipeline_stage_t)bypass_stage,
        (pipeline_stage_t)bypass_stage,
    };

    const configSTACK_DEPTH_TYPE stage_stack_sizes[] = {
        configMINIMAL_STACK_SIZE + RTOS_THREAD_STACK_SIZE(bypass_stage) + RTOS_THREAD_STACK_SIZE(audio_pipeline_input_i),
        configMINIMAL_STACK_SIZE + RTOS_THREAD_STACK_SIZE(bypass_stage) + RTOS_THREAD_STACK_SIZE(audio_pipeline_output_i),
    };

    initialize_pipeline_stages();

    generic_pipeline_init((pipeline_input_t)audio_pipeline_input_i,
                        (pipeline_output_t)audio_pipeline_output_i,
                        input_app_data,
                        output_app_data,
                        stages,
                        (const size_t*) stage_stack_sizes,
                        appconfAUDIO_PIPELINE_TASK_PRIORITY,
                        stage_count);
}

#endif /* ON_TILE(0) */
