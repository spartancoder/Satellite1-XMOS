#pragma once

#include "servicer.h"
#include <stdint.h>

// Resource ID for Audio Pipeline Settings servicer
// 30=audio_cfg, 31=DOA, 32=this servicer
#define AUDIO_PIPELINE_SETTINGS_SERVICER_RESID (32)
#define AUDIO_PIPELINE_SETTINGS_SERVICER_NUM_RESOURCES (1)

// Number of microphones (from MIC_ARRAY_CONFIG_MIC_COUNT)
#define MIC_GAIN_NUM_CHANNELS (4)

// Q8.8 fixed-point gain representation
// 1.0x gain = 256 (0x0100)
// Range: 0.0x to 255.996x in 0.00390625 steps
#define MIC_GAIN_NEUTRAL (256)   // 1.0x gain (no change)
#define MIC_GAIN_SHIFT (8)       // Q8.8 format shift

// Command IDs
enum e_audio_pipeline_settings_cmd_map {
    AUDIO_PIPELINE_SETTINGS_CMD_MIC_GAIN = 0,
    AUDIO_PIPELINE_SETTINGS_CMD_DOA_LED_ENABLED = 1,
    NUM_AUDIO_PIPELINE_SETTINGS_CMDS = 2
};

/**
 * Per-microphone gain values.
 * Each gain is Q8.8 fixed-point (256 = 1.0x).
 */
typedef struct {
    uint16_t gain[MIC_GAIN_NUM_CHANNELS];
} mic_gain_t;

/**
 * LED control settings.
 */
typedef struct {
    uint8_t doa_led_enabled;  // 1 = automatic DOA LED enabled, 0 = disabled
} led_settings_t;

// Default values
#define DOA_LED_ENABLED_DEFAULT (1)  // Enabled by default

/**
 * Audio Pipeline Settings servicer context.
 */
typedef struct {
    servicer_t *servicer;
    device_control_t **device_control_ctx;
    size_t device_control_ctx_count;
    mic_gain_t *mic_gain;       // Pointer to shared gain values
    led_settings_t *led_settings;  // Pointer to shared LED settings
} audio_pipeline_settings_servicer_ctx_t;

/**
 * Initialize Audio Pipeline Settings servicer.
 *
 * @param ctx          Servicer context to initialize
 * @param mic_gain     Pointer to shared mic_gain struct
 * @param led_settings Pointer to shared led_settings struct
 */
void audio_pipeline_settings_servicer_init(audio_pipeline_settings_servicer_ctx_t *ctx,
                                           mic_gain_t *mic_gain,
                                           led_settings_t *led_settings);

/**
 * Start Audio Pipeline Settings servicer task.
 *
 * @param ctx                    Servicer context
 * @param device_control_ctx     Device control context array
 * @param device_control_ctx_count Number of device control contexts
 */
void audio_pipeline_settings_servicer_start(audio_pipeline_settings_servicer_ctx_t *ctx,
                                            device_control_t **device_control_ctx,
                                            size_t device_control_ctx_count);
