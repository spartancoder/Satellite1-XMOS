# DOA SPI Export Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Export DOA data from GCC-PHAT to ESP32 via SPI using a new DOA servicer.

**Architecture:** A shared `doa_result_t` struct is updated by the audio pipeline on tile 1 and read by a new DOA servicer on tile 0. The servicer responds to SPI read commands with 19 bytes of DOA data (3 sources × 6 bytes + 1 count byte).

**Tech Stack:** XMOS xcore, FreeRTOS, device_control framework, GCC-PHAT DOA

---

## File Structure

| File | Action | Purpose |
|------|--------|---------|
| `satellite-xmos-firmware/src/control/doa_servicer.h` | Create | Data structures and servicer API |
| `satellite-xmos-firmware/src/control/doa_servicer.c` | Create | Servicer implementation |
| `satellite-xmos-firmware/src/main.c` | Modify | Add shared DOA result, update pipeline |
| `satellite-xmos-firmware/src/app_conf.h` | Modify | Add DOA servicer resource ID |
| `satellite-xmos-firmware/bsp_config/SATELLITE1/platform/platform_init.c` | Modify | Increment servicer count |
| `satellite-xmos-firmware/firmware.cmake` | Modify | Add doa_servicer.c to build |

---

## Chunk 1: Data Structures and Header File

### Task 1: Create DOA Servicer Header

**Files:**
- Create: `satellite-xmos-firmware/src/control/doa_servicer.h`

- [ ] **Step 1: Create doa_servicer.h with data structures**

Create the header file with the DOA data structures and servicer API:

```c
#pragma once

#include "servicer.h"
#include <stdint.h>

// Resource ID for DOA servicer
#define DOA_SERVICER_RESID (31)
#define DOA_SERVICER_NUM_RESOURCES (1)

// Number of DOA sources supported
#define DOA_MAX_SOURCES (3)

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
```

- [ ] **Step 2: Verify file compiles**

Run a quick syntax check by attempting to build:
```bash
cd /workspace && source .venv/bin/activate && cmake -B build --toolchain modules/core/xmos_cmake_toolchain/xs3a.cmake 2>&1 | head -50
```

Expected: CMake configuration succeeds (may have warnings about missing implementation)

- [ ] **Step 3: Commit header file**

```bash
git add satellite-xmos-firmware/src/control/doa_servicer.h
git commit -m "$(cat <<'EOF'
feat(doa): add DOA servicer header with data structures

- Define doa_source_t (6 bytes: azimuth, elevation, confidence, vad)
- Define doa_result_t (19 bytes: 3 sources + count)
- Add doa_servicer_ctx_t and API functions
- Add doa_rad_to_cdeg() helper for unit conversion

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Chunk 2: Servicer Implementation

### Task 2: Implement DOA Servicer

**Files:**
- Create: `satellite-xmos-firmware/src/control/doa_servicer.c`

- [ ] **Step 1: Create doa_servicer.c with servicer implementation**

```c
#include <string.h>
#include "debug_print.h"
#include "servicer.h"
#include "doa_servicer.h"
#include "FreeRTOS.h"
#include "platform/platform_conf.h"

// Command map - defines what commands this servicer accepts
static control_cmd_info_t doa_servicer_cmd_map[] = {
    { DOA_SERVICER_CMD_GET_DOA, DOA_RESULT_SIZE, sizeof(uint8_t), CMD_READ_ONLY },
};

// Compile-time size check
_Static_assert(sizeof(doa_result_t) == 19, "doa_result_t must be 19 bytes");
_Static_assert(sizeof(doa_source_t) == 6, "doa_source_t must be 6 bytes");

//-----------------Servicer read callback function-----------------------//
DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t doa_servicer_read_cmd(control_resid_t resid,
                                           control_cmd_t cmd,
                                           uint8_t *payload,
                                           size_t payload_len,
                                           void *app_data)
{
    control_ret_t ret = CONTROL_SUCCESS;
    doa_servicer_ctx_t *ctx = (doa_servicer_ctx_t *) app_data;
    servicer_t *servicer = ctx->servicer;

    // For read commands, payload[0] is reserved for status
    payload_len -= 1;
    uint8_t *payload_ptr = &payload[1];

    debug_printf("DOA servicer on tile %d received READ command %02x for resid %02x\n",
                 THIS_XCORE_TILE, cmd, resid);

    control_resource_info_t *current_res_info = get_res_info(resid, servicer);
    xassert(current_res_info != NULL);

    control_cmd_info_t *current_cmd_info;
    ret = validate_cmd(&current_cmd_info, current_res_info, cmd, payload_ptr, payload_len);
    if (ret != CONTROL_SUCCESS) {
        payload[0] = ret;
        return ret;
    }

    uint8_t cmd_id = CONTROL_CMD_CLEAR_READ(cmd);
    switch (cmd_id) {
        case DOA_SERVICER_CMD_GET_DOA:
            // Copy DOA result to payload (19 bytes)
            memcpy(payload_ptr, ctx->doa_result, sizeof(doa_result_t));
            payload[0] = CONTROL_SUCCESS;
            debug_printf("DOA: az=%d, el=%d, conf=%d, vad=%d, count=%d\n",
                         ctx->doa_result->sources[0].azimuth_cdeg,
                         ctx->doa_result->sources[0].elevation_cdeg,
                         ctx->doa_result->sources[0].confidence,
                         ctx->doa_result->sources[0].vad,
                         ctx->doa_result->count);
            return CONTROL_SUCCESS;

        default:
            debug_printf("DOA SERVICER UNHANDLED COMMAND: %02x\n", cmd_id);
            ret = CONTROL_BAD_COMMAND;
            payload[0] = ret;
            return ret;
    }
}

//-----------------Servicer write callback function-----------------------//
DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t doa_servicer_write_cmd(control_resid_t resid,
                                            control_cmd_t cmd,
                                            const uint8_t *payload,
                                            size_t payload_len,
                                            void *app_data)
{
    // DOA servicer is read-only, no write commands
    debug_printf("DOA servicer received unexpected WRITE command %02x for resid %02x\n",
                 cmd, resid);
    return CONTROL_BAD_COMMAND;
}

//-----------------Servicer task-----------------------//
void doa_servicer_task(void *args)
{
    device_control_servicer_t servicer_ctx;
    doa_servicer_ctx_t *ctx = (doa_servicer_ctx_t *) args;
    servicer_t *servicer = ctx->servicer;

    xassert(servicer != NULL);

    control_resid_t *resources = (control_resid_t *) pvPortMalloc(
        servicer->num_resources * sizeof(control_resid_t));
    for (int i = 0; i < servicer->num_resources; i++) {
        resources[i] = servicer->res_info[i].resource;
    }

    control_ret_t dc_ret;
    debug_printf("DOA servicer registering, ID %d, tile %d, core %d\n",
                 servicer->id, THIS_XCORE_TILE, rtos_core_id_get());

    dc_ret = device_control_servicer_register(&servicer_ctx,
                                              ctx->device_control_ctx,
                                              ctx->device_control_ctx_count,
                                              resources,
                                              servicer->num_resources);

    vPortFree(resources);

    if (dc_ret != CONTROL_SUCCESS) {
        debug_printf("DOA servicer registration failed: %d\n", dc_ret);
        return;
    }

    debug_printf("DOA servicer registered successfully\n");

    // Main command processing loop
    for (;;) {
        device_control_servicer_cmd_recv(&servicer_ctx,
                                         doa_servicer_read_cmd,
                                         doa_servicer_write_cmd,
                                         ctx,
                                         RTOS_OSAL_WAIT_FOREVER);
    }
}

//-----------------Initialization functions-----------------------//
void doa_servicer_init(doa_servicer_ctx_t *ctx, doa_result_t *doa_result)
{
    static servicer_t servicer;
    static control_resource_info_t servicer_res_info[DOA_SERVICER_NUM_RESOURCES];

    ctx->servicer = &servicer;

    memset(&servicer, 0, sizeof(servicer_t));
    servicer.id = DOA_SERVICER_RESID;
    servicer.start_io = 0;
    servicer.num_resources = DOA_SERVICER_NUM_RESOURCES;

    servicer.res_info = &servicer_res_info[0];
    servicer.res_info[0].resource = DOA_SERVICER_RESID;
    servicer.res_info[0].command_map.num_commands = NUM_DOA_SERVICER_RESID_CMDS;
    servicer.res_info[0].command_map.commands = doa_servicer_cmd_map;

    ctx->doa_result = doa_result;
}

void doa_servicer_start(doa_servicer_ctx_t *ctx,
                        device_control_t **device_control_ctx,
                        size_t device_control_ctx_count)
{
    ctx->device_control_ctx = device_control_ctx;
    ctx->device_control_ctx_count = device_control_ctx_count;

    xTaskCreate(
        doa_servicer_task,
        "DOA servicer",
        RTOS_THREAD_STACK_SIZE(doa_servicer_task),
        ctx,
        appconfDEVICE_CONTROL_SPI_PRIORITY - 1,
        NULL
    );
}
```

- [ ] **Step 2: Update header with size constant**

Add the size constant to the header file for the command map:

```c
// In doa_servicer.h, add after DOA_MAX_SOURCES:
#define DOA_RESULT_SIZE (19)  // Size of doa_result_t in bytes
```

- [ ] **Step 3: Commit servicer implementation**

```bash
git add satellite-xmos-firmware/src/control/doa_servicer.c satellite-xmos-firmware/src/control/doa_servicer.h
git commit -m "$(cat <<'EOF'
feat(doa): implement DOA servicer

- Read-only servicer responding to RESID 31, CMD 0
- Returns 19-byte doa_result_t payload
- Includes compile-time size assertions
- Follows audio_cfg_servicer pattern

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Chunk 3: Integration with Main and Build System

### Task 3: Add Shared DOA Result to main.c

**Files:**
- Modify: `satellite-xmos-firmware/src/main.c`

- [ ] **Step 1: Add include and shared DOA result global**

At the top of main.c, add the include after the existing includes:

```c
#include "doa_servicer.h"
```

After the existing global `doa4_state_t doa;` (around line 50), add:

```c
// Shared DOA result - updated by pipeline, read by servicer
DWORD_ALIGNED volatile doa_result_t doa_result_shared;
```

- [ ] **Step 2: Update DOA processing to populate shared struct**

In the `audio_pipeline_input_i2s_and_mic()` function, after the DOA calculation (around line 247), update the shared struct:

Replace:
```c
#if ON_TILE(1)
    float ang = doa4_process_frame(&doa, mic_ptr, -31);
    static uint8_t led_buffer[LED_RING_NUM_LEDS * 3];
    // ... LED code ...
```

With:
```c
#if ON_TILE(1)
    float ang = doa4_process_frame(&doa, mic_ptr, -31);

    // Update shared DOA result
    doa_result_shared.sources[0].azimuth_cdeg = doa_rad_to_cdeg(ang);
    doa_result_shared.sources[0].elevation_cdeg = 0;  // Flat array, no elevation
    doa_result_shared.sources[0].confidence = 100;    // Placeholder
    doa_result_shared.sources[0].vad = 1;             // Placeholder
    doa_result_shared.sources[1].confidence = 0;      // Unused
    doa_result_shared.sources[2].confidence = 0;      // Unused
    doa_result_shared.count = 1;

    static uint8_t led_buffer[LED_RING_NUM_LEDS * 3];
    // ... rest of LED code unchanged ...
```

- [ ] **Step 3: Initialize DOA result and start servicer in startup_task**

In the `startup_task()` function, after the audio_cfg_servicer_start call (around line 397), add:

```c
#if ON_TILE(0)
    // DOA servicer
    static doa_servicer_ctx_t doa_servicer_ctx;
    doa_servicer_init(&doa_servicer_ctx, (doa_result_t *)&doa_result_shared);
    doa_servicer_start(&doa_servicer_ctx, device_control_ctx, 1);
#endif
```

- [ ] **Step 4: Commit main.c changes**

```bash
git add satellite-xmos-firmware/src/main.c
git commit -m "$(cat <<'EOF'
feat(doa): integrate DOA servicer with main pipeline

- Add shared doa_result_shared struct (volatile)
- Update pipeline to populate DOA result after doa4_process_frame()
- Initialize and start DOA servicer on tile 0

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

### Task 4: Update Build Configuration

**Files:**
- Modify: `satellite-xmos-firmware/firmware.cmake`
- Modify: `satellite-xmos-firmware/bsp_config/SATELLITE1/platform/platform_init.c`

- [ ] **Step 1: Add doa_servicer.c to firmware.cmake**

Find the servicer source files section and add `doa_servicer.c`:

```cmake
# In the servicer sources section, add:
${CMAKE_CURRENT_LIST_DIR}/src/control/doa_servicer.c
```

- [ ] **Step 2: Increment servicer count in platform_init.c**

In `platform_init.c`, update the servicer count from `3` to `4`:

```c
device_control_init(device_control_spi_ctx,
                    DEVICE_CONTROL_HOST_MODE,
                    4 + !!(BUILTIN_TESTS_SPI_ECHO_SERVICER), //number of servicers (was 3)
                    client_intertile_ctx,
                    1);
```

- [ ] **Step 3: Commit build configuration changes**

```bash
git add satellite-xmos-firmware/firmware.cmake
git add satellite-xmos-firmware/bsp_config/SATELLITE1/platform/platform_init.c
git commit -m "$(cat <<'EOF'
build: add DOA servicer to build configuration

- Add doa_servicer.c to firmware.cmake
- Increment servicer count from 3 to 4 in platform_init.c

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Chunk 4: Build Verification and Final Testing

### Task 5: Build and Verify

**Files:**
- None (verification only)

- [ ] **Step 1: Configure CMake build**

```bash
cd /workspace && source .venv/bin/activate
cmake -B build --toolchain modules/core/xmos_cmake_toolchain/xs3a.cmake
```

Expected: Configuration succeeds with no errors

- [ ] **Step 2: Build the firmware**

```bash
cd /workspace/build
make satellite1_firmware_fixed_delay 2>&1 | tail -100
```

Expected: Build completes successfully, producing `satellite1_firmware_fixed_delay.xe`

- [ ] **Step 3: Verify SPI payload size**

Check that the payload size is correct by reviewing the build output or using xobjdump:

```bash
xobjdump -t satellite1_firmware_fixed_delay.xe | grep -i doa
```

Expected: DOA servicer symbols are present

- [ ] **Step 4: Final commit (if any fixes were needed)**

If any fixes were required during build, commit them:

```bash
git add -A
git commit -m "$(cat <<'EOF'
fix(doa): build fixes for DOA servicer integration

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Testing Notes

### Hardware Testing Checklist

After successful build, test on hardware:

1. **SPI Communication**
   - ESP32 sends: `[0x1F] [0x80] [0x13]` (RESID=31, CMD=0|0x80, LEN=19)
   - XMOS responds: `[0x13] [0x00] [19 bytes of DOA data]`

2. **DOA Accuracy**
   - Rotate sound source around the device
   - Verify azimuth values change appropriately (0-35999 centidegrees)

3. **Unused Slots**
   - Verify `sources[1].confidence == 0` and `sources[2].confidence == 0`
   - Verify `count == 1`

### ESP32 Integration Code Reference

```c
// ESP32 SPI read example
esp_err_t read_doa(doa_result_t *result) {
    uint8_t tx_buf[256] = {0};
    uint8_t rx_buf[256] = {0};

    // Build read command
    tx_buf[0] = 0x1F;  // RESID 31
    tx_buf[1] = 0x80;  // CMD 0 with read bit
    tx_buf[2] = 19;    // LEN

    spi_transaction_t trans = {
        .length = 8 * 256,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };

    esp_err_t ret = spi_device_transmit(spi_device, &trans);
    if (ret != ESP_OK) return ret;

    // Parse response (skip LEN and STATUS bytes)
    memcpy(result, &rx_buf[2], sizeof(doa_result_t));
    return ESP_OK;
}
```

---

## Summary

| Task | Files | Status |
|------|-------|--------|
| 1. Create header | doa_servicer.h | - [ ] |
| 2. Implement servicer | doa_servicer.c | - [ ] |
| 3. Integrate with main | main.c | - [ ] |
| 4. Update build | firmware.cmake, platform_init.c | - [ ] |
| 5. Build and verify | - | - [ ] |

**Total commits:** 4-5
