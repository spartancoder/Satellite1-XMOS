#pragma once

#include "servicer.h"
#include <stdint.h>

// Resource ID for DOA servicer
#define DOA_SERVICER_RESID (31)
#define DOA_SERVICER_NUM_RESOURCES (1)

// Number of DOA sources supported
#define DOA_MAX_SOURCES (3)
#define DOA_RESULT_SIZE (19)  // Size of doa_result_t in bytes

// Command IDs
enum e_doa_servicer_cmd_map {
    DOA_SERVICER_CMD_GET_DOA = 0,
    NUM_DOA_SERVICER_RESID_CMDS = 1
};

/**
 * DOA source data structure.
 * Size: 6 bytes per source.
 */
typedef struct {
    int16_t azimuth_cdeg;    // Centidegrees: 0-35999 (0.00°-359.99°)
    int16_t elevation_cdeg;  // Centidegrees: -9000 to +9000 (-90.00° to +90.00°)
    uint8_t confidence;      // 0-100 percentage
    uint8_t vad;             // Voice Activity Detection: 0=no voice, 1=voice detected
} doa_source_t;

/**
 * DOA result structure.
 * Size: 19 bytes total (6 × 3 + 1).
 * Updated by audio pipeline, read by servicer.
 */
typedef struct {
    doa_source_t sources[DOA_MAX_SOURCES];  // Fixed 3 slots
    uint8_t count;                           // Number of valid sources (1-3)
} doa_result_t;

/**
 * DOA servicer context.
 */
typedef struct {
    servicer_t *servicer;
    device_control_t **device_control_ctx;
    size_t device_control_ctx_count;
    doa_result_t *doa_result;  // Pointer to shared DOA result
} doa_servicer_ctx_t;

/**
 * Initialize DOA servicer.
 *
 * @param ctx        Servicer context to initialize
 * @param doa_result Pointer to shared DOA result struct
 */
void doa_servicer_init(doa_servicer_ctx_t *ctx, doa_result_t *doa_result);

/**
 * Start DOA servicer task.
 *
 * @param ctx                    Servicer context
 * @param device_control_ctx     Device control context array
 * @param device_control_ctx_count Number of device control contexts
 */
void doa_servicer_start(doa_servicer_ctx_t *ctx,
                        device_control_t **device_control_ctx,
                        size_t device_control_ctx_count);

/**
 * Helper: Convert radians to centidegrees.
 */
static inline int16_t doa_rad_to_cdeg(float rad) {
    // Convert radians to degrees, then to centidegrees
    float deg = rad * (180.0f / 3.14159265358979323846f);
    // Wrap to 0-360 range
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return (int16_t)(deg * 100.0f);
}