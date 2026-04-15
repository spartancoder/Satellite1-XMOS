# xSCOPE 4-Mic Firmware Variant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create an xSCOPE firmware variant for XK-VOICE-SQ66 that streams 4 mic channels and observation points over xTAG4, with a host-side Python recording script.

**Architecture:** New build variant (`xk-voice-sq66-xscope-4mic.cmake`) compiles `xscope_audio_io.c/h` into firmware. Observation calls are placed in pipeline files, guarded by `appconfXSCOPE_4MIC_ENABLED`. Probes defined in XML become compile-time `#define` constants used with `xscope_bytes()` for audio and `xscope_float()` for metadata. Host Python script uses `mic_array.xscope.Endpoint` for transport.

**Tech Stack:** XMOS xCORE XTC 15.3.1, CMake/xcommon, FreeRTOS, xscope (`xscope_bytes`, `xscope_float`), Python 3 + numpy + scipy

**Spec deviation from design doc:** The spec references `xscope_raw()` which does not exist in the XMOS toolchain. The actual API is `xscope_bytes(id, size, data)` for framed byte arrays and `xscope_float(id, value)` for scalar floats. Functionally equivalent — `xscope_bytes` sends arbitrary-length byte payloads per probe ID. The spec also references `xscope_register()` for code-based registration, but when XML config is used (as in this project), `xscope_register()` is ignored. All probes are defined in the XML config file, which generates `#define` constants usable directly in C code.

---

## File Structure

### New Files

| File | Responsibility |
|---|---|
| `satellite-xmos-firmware/src/xscope_audio_io.h` | Public API with conditional stub pattern |
| `satellite-xmos-firmware/src/xscope_audio_io.c` | Probe emission implementations using xscope_bytes/xscope_float |
| `satellite-xmos-firmware/xk-voice-sq66-xscope-4mic.cmake` | Build variant for SQ66 with xscope-4mic enabled |
| `satellite-xmos-firmware/src/config-xscope-4mic.xscope` | XML probe definitions (~42 probes) |
| `scripts/test_xscope_recorder.py` | Host-side Python recording script |

### Modified Files

| File | Change |
|---|---|
| `satellite-xmos-firmware/src/app_conf.h` | Add `appconfXSCOPE_4MIC_ENABLED` flag |
| `satellite-xmos-firmware/firmware.cmake` | Include new variant cmake |
| `satellite-xmos-firmware/audio_pipelines/reference/empty/audio_pipeline_t1.c` | Add observation calls + input switching |
| `satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t1.c` | Add observation calls + input switching |
| `satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t0.c` | Add observation calls |

---

## Task 1: Add compile flag to app_conf.h

**Files:**
- Modify: `satellite-xmos-firmware/src/app_conf.h`

- [ ] **Step 1: Add `appconfXSCOPE_4MIC_ENABLED` flag**

Add after the `appconfSPI_AUDIO_MODE` block (after line 154, before the `#include "app_conf_check.h"` line):

```c
#ifndef appconfXSCOPE_4MIC_ENABLED
#define appconfXSCOPE_4MIC_ENABLED    0
#endif
```

- [ ] **Step 2: Commit**

```bash
git add satellite-xmos-firmware/src/app_conf.h
git commit -m "feat: add appconfXSCOPE_4MIC_ENABLED compile flag"
```

---

## Task 2: Create xscope probe XML config

**Files:**
- Create: `satellite-xmos-firmware/src/config-xscope-4mic.xscope`

This file defines ALL probes. The XML file generates `#define` constants for each probe name (e.g., `mic_raw_0` becomes probe ID 0, `mic_raw_1` becomes probe ID 1, etc.). These constants are used directly in `xscope_bytes()` and `xscope_float()` calls.

Probe IDs are assigned sequentially starting from 0 based on order in the file.

- [ ] **Step 1: Create the XML config**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<xSCOPEconfig ioMode="basic" enabled="true">
    <!-- Existing probes (must be first to match original IDs) -->
    <Probe name="freertos_trace"       type="CONTINUOUS" datatype="NONE" units="NONE" enabled="true"/>
    <Probe name="pll_freq"             type="CONTINUOUS" datatype="UINT" units="NONE" enabled="true"/>

    <!-- Tile 1: Post-PDM decode, pre-gain (4 channels) -->
    <Probe name="mic_raw_0"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_raw_1"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_raw_2"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_raw_3"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 1: Post-mic-gain (4 channels) -->
    <Probe name="mic_gain_0"           type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_gain_1"           type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_gain_2"           type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_gain_3"           type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 1: Post-AEC (4 channels) -->
    <Probe name="mic_aec_0"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_aec_1"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_aec_2"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="mic_aec_3"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 1: AEC residual / error signal (4 channels) -->
    <Probe name="aec_residual_0"       type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="aec_residual_1"       type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="aec_residual_2"       type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="aec_residual_3"       type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 0: Post-IC output (2 channels) -->
    <Probe name="ic_out_0"             type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="ic_out_1"             type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 0: IC residual (2 channels) -->
    <Probe name="ic_residual_0"        type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="ic_residual_1"        type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 0: Post-NS (2 channels) -->
    <Probe name="ns_out_0"             type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="ns_out_1"             type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 0: Post-AGC (2 channels) -->
    <Probe name="agc_out_0"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="agc_out_1"            type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 0: Post-beamform (3 beams, future) -->
    <Probe name="beam_0"               type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="beam_1"               type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
    <Probe name="beam_2"               type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Tile 0: Metadata (scalar values per frame) -->
    <Probe name="vnr_value"            type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="agc_gain"             type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="doa_angle"            type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="doa_angle_raw"        type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="doa_confidence"       type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="vad_beam_0"           type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="vad_beam_1"           type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="vad_beam_2"           type="CONTINUOUS" datatype="FLOAT" units="NONE" enabled="true"/>
    <Probe name="beam_selection"       type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>

    <!-- Host-to-device: input source control -->
    <Probe name="input_source"         type="CONTINUOUS" datatype="UINT"  units="NONE" enabled="true"/>
</xSCOPEconfig>
```

- [ ] **Step 2: Commit**

```bash
git add satellite-xmos-firmware/src/config-xscope-4mic.xscope
git commit -m "feat: add xscope 4-mic probe definitions XML config"
```

---

## Task 3: Create xscope_audio_io.h

**Files:**
- Create: `satellite-xmos-firmware/src/xscope_audio_io.h`

The header implements the conditional stub pattern from the spec. When `appconfXSCOPE_4MIC_ENABLED` is 0, all functions compile to empty inline stubs — zero overhead, no .c file linked. When enabled, the real implementations are in `xscope_audio_io.c`.

- [ ] **Step 1: Create the header**

```c
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
```

- [ ] **Step 2: Commit**

```bash
git add satellite-xmos-firmware/src/xscope_audio_io.h
git commit -m "feat: add xscope_audio_io.h with conditional stub pattern"
```

---

## Task 4: Create xscope_audio_io.c

**Files:**
- Create: `satellite-xmos-firmware/src/xscope_audio_io.c`

This file is only compiled when `appconfXSCOPE_4MIC_ENABLED=1`. It uses `xscope_bytes()` for audio frames and `xscope_float()` for metadata. Probe ID constants come from the XML config (auto-generated `#define`s matching probe names).

**Key constraint:** `xscope_bytes()` sends `(id, size_in_bytes, data_ptr)`. For a single channel of 240 int32_t samples, that's 960 bytes per call. The xscope framework handles framing internally — there is no practical size limit for device-to-host transfers.

**Host-to-device injection:** The `xscope_connect_data_from_host(chanend)` + `xscope_data_from_host(chanend, buf, &n)` mechanism is used. However, host upload has a 255-byte limit per packet. For the initial implementation, injection is deferred — the host input functions return "not active" and the TODO is noted for a follow-up task.

- [ ] **Step 1: Create the implementation**

```c
// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "xscope_audio_io.h"

#if appconfXSCOPE_4MIC_ENABLED

#include <xscope.h>
#include <string.h>

/*
 * Probe ID constants are auto-generated from config-xscope-4mic.xscope.
 * The XML generates #define constants matching each probe name.
 * The probe names below correspond to the XML definitions in order.
 *
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
```

- [ ] **Step 2: Commit**

```bash
git add satellite-xmos-firmware/src/xscope_audio_io.c
git commit -m "feat: add xscope_audio_io.c with probe emission implementations"
```

---

## Task 5: Create build variant CMake file

**Files:**
- Create: `satellite-xmos-firmware/xk-voice-sq66-xscope-4mic.cmake`
- Modify: `satellite-xmos-firmware/firmware.cmake`

This is based on `xk-voice-sq66.cmake` with these changes:
- Target names use `sq66_xscope_4mic` instead of `sq66`
- Adds `appconfXSCOPE_4MIC_ENABLED=1` to compile definitions
- Uses `config-xscope-4mic.xscope` instead of `config.xscope`
- Only builds the `empty` pipeline initially (no `bypass`/`fixed_delay` loop)

- [ ] **Step 1: Create the variant CMake file**

```cmake
query_tools_version()

set(FFVA_AP empty)
set(PL_NAME empty)

set(FFVA_INT_COMPILE_DEFINITIONS
${APP_COMPILE_DEFINITIONS}
    appconfEXTERNAL_MCLK=0
    appconfI2S_ENABLED=1
    appconfUSB_ENABLED=0
    appconfUSB_AUDIO_ENABLED=0
    appconfUSB_AUDIO_MODE=0
    appconfUSB_CDC_ENABLED=0
    appconfAEC_REF_DEFAULT=appconfAEC_REF_I2S
    appconfI2S_MODE=appconfI2S_MODE_MASTER
    appconfI2S_AUDIO_SAMPLE_RATE=48000
    appconfDEVICE_CTRL_SPI=1
    appconfLED_RING=0
    appconfINPUT_SAMPLES_MIC_DELAY_MS=20
    appconfXSCOPE_4MIC_ENABLED=1
    appconfPIPELINE_BYPASS=0
)

#**********************
# Tile Targets
#**********************
set(TARGET_NAME tile0_sq66_xscope_4mic_firmware_${FFVA_AP})
add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL)
target_sources(${TARGET_NAME} PUBLIC ${APP_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${APP_INCLUDES})
target_compile_definitions(${TARGET_NAME}
    PUBLIC
        ${FFVA_INT_COMPILE_DEFINITIONS}
        THIS_XCORE_TILE=0
)
target_compile_options(${TARGET_NAME}
    PRIVATE
        ${APP_COMPILER_FLAGS}
        -fxscope
        ${CMAKE_CURRENT_LIST_DIR}/src/config-xscope-4mic.xscope
)
target_link_libraries(${TARGET_NAME}
    PUBLIC
        ${APP_COMMON_LINK_LIBRARIES}
        fph::ffva::sq66
        fph::ffva::ap::${PL_NAME}
        sln_voice::app::ffva::sp::passthrough
        ${CMAKE_CURRENT_LIST_DIR}/src/config-xscope-4mic.xscope
)
target_link_options(${TARGET_NAME} PRIVATE ${APP_LINK_OPTIONS})
unset(TARGET_NAME)

set(TARGET_NAME tile1_sq66_xscope_4mic_firmware_${FFVA_AP})
add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL)
target_sources(${TARGET_NAME} PUBLIC ${APP_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${APP_INCLUDES})
target_compile_definitions(${TARGET_NAME}
    PUBLIC
        ${FFVA_INT_COMPILE_DEFINITIONS}
        THIS_XCORE_TILE=1
)
target_compile_options(${TARGET_NAME}
    PRIVATE
        ${APP_COMPILER_FLAGS}
        -fxscope
        ${CMAKE_CURRENT_LIST_DIR}/src/config-xscope-4mic.xscope
)
target_link_libraries(${TARGET_NAME}
    PUBLIC
        ${APP_COMMON_LINK_LIBRARIES}
        fph::ffva::sq66
        fph::ffva::ap::${PL_NAME}
        sln_voice::app::ffva::sp::passthrough
        ${CMAKE_CURRENT_LIST_DIR}/src/config-xscope-4mic.xscope
)
target_link_options(${TARGET_NAME} PRIVATE ${APP_LINK_OPTIONS})
unset(TARGET_NAME)

#*********************
# Create version.h
#*********************
SET(VERSIONING_CMD "build")
if(USE_DEV_TRACKING)
    list(APPEND VERSIONING_CMD "--track")
endif()

add_custom_target(sq66_xscope_4mic_firmware_${FFVA_AP}_versioning
    COMMAND ${Python3_EXECUTABLE} ${VERSIONING_SCRIPT} ${VERSIONING_CMD} sq66_xscope_4mic_firmware_${FFVA_AP}
    COMMENT "Running versioning.py build sq66_xscope_4mic_firmware_${FFVA_AP}"
    VERBATIM
)
add_dependencies(tile0_sq66_xscope_4mic_firmware_${FFVA_AP} sq66_xscope_4mic_firmware_${FFVA_AP}_versioning)
add_dependencies(tile1_sq66_xscope_4mic_firmware_${FFVA_AP} sq66_xscope_4mic_firmware_${FFVA_AP}_versioning)

#**********************
# Merge binaries
#**********************
merge_binaries(sq66_xscope_4mic_firmware_${FFVA_AP} tile0_sq66_xscope_4mic_firmware_${FFVA_AP} tile1_sq66_xscope_4mic_firmware_${FFVA_AP} 1)

#**********************
# Create run and debug targets
#**********************
create_run_target(sq66_xscope_4mic_firmware_${FFVA_AP})
create_debug_target(sq66_xscope_4mic_firmware_${FFVA_AP})
create_upgrade_img_target(sq66_xscope_4mic_firmware_${FFVA_AP} ${XTC_VERSION_MAJOR} ${XTC_VERSION_MINOR})
```

- [ ] **Step 2: Add include to firmware.cmake**

Add at the end of `satellite-xmos-firmware/firmware.cmake` (after line 121, the `xk-voice-sq66-usb.cmake` include):

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/xk-voice-sq66-xscope-4mic.cmake)
```

- [ ] **Step 3: Commit**

```bash
git add satellite-xmos-firmware/xk-voice-sq66-xscope-4mic.cmake satellite-xmos-firmware/firmware.cmake
git commit -m "feat: add SQ66 xscope-4mic build variant"
```

---

## Task 6: Integrate observation points into empty pipeline (Tile 1)

**Files:**
- Modify: `satellite-xmos-firmware/audio_pipelines/reference/empty/audio_pipeline_t1.c`

Add `#include "xscope_audio_io.h"` and place observation calls in `audio_pipeline_input_i()` after mic input and gain stages. Since the stub pattern is used, the calls compile to nothing when `appconfXSCOPE_4MIC_ENABLED=0`.

- [ ] **Step 1: Add include and observation calls**

At the top of the file, after the existing `#include "control/audio_pipeline_settings_servicer.h"` line (line 23), add:

```c
#include "xscope_audio_io.h"
```

In the `audio_pipeline_input_i` function, after the `audio_pipeline_input()` call (line 71) and before the `stage_apply_mic_gain()` call (line 74), add:

```c
    // Observation: raw mic after PDM decode, before gain
    xscope_audio_io_send_raw_mic(frame_data->mic_samples_passthrough);
```

After the `stage_apply_mic_gain(frame_data)` call (line 74), add:

```c
    // Observation: mic after gain stage
    xscope_audio_io_send_gain_mic(frame_data->mic_samples_passthrough);
```

The modified `audio_pipeline_input_i` function should look like:

```c
static void *audio_pipeline_input_i(void *input_app_data)
{
    frame_data_t *frame_data;
    frame_data = pvPortMalloc(sizeof(frame_data_t));
    memset(frame_data, 0x00, sizeof(frame_data_t));

    audio_pipeline_input(input_app_data,
                       (int32_t *)frame_data->aec_reference_audio_samples,
                       appconfMIC_PIPELINE_REF_CHANNELS + appconfMIC_PIPELINE_INPUT_CHANNELS,
                       appconfAUDIO_PIPELINE_FRAME_ADVANCE);

    // Observation: raw mic after PDM decode, before gain
    xscope_audio_io_send_raw_mic(frame_data->mic_samples_passthrough);

    // Apply per-mic gain immediately after decimation
    stage_apply_mic_gain(frame_data);

    // Observation: mic after gain stage
    xscope_audio_io_send_gain_mic(frame_data->mic_samples_passthrough);

    memcpy(frame_data->samples, frame_data->mic_samples_passthrough, sizeof(frame_data->samples));

    return frame_data;
}
```

- [ ] **Step 2: Commit**

```bash
git add satellite-xmos-firmware/audio_pipelines/reference/empty/audio_pipeline_t1.c
git commit -m "feat: add xscope observation points to empty pipeline Tile 1"
```

---

## Task 7: Build verification

**Files:**
- No new files

Verify the firmware compiles with the new variant. This tests that:
1. The XML config generates valid probe ID constants
2. `xscope_audio_io.h` compiles (both stub and enabled modes)
3. `xscope_audio_io.c` compiles when enabled
4. The CMake variant correctly links everything
5. The existing SQ66 variants still compile (stub mode — no regression)

- [ ] **Step 1: Build the xscope-4mic variant (empty pipeline)**

Run from the build directory (adjust path to your build setup):

```bash
cd satellite-xmos-firmware
# If using the project's build script:
cmake -B build_xscope_4mic -G "Unix Makefiles" \
    -DENABLE_ALL_FFVA_PIPELINES=ON \
    -DUSE_DEV_MODE=ON
cmake --build build_xscope_4mic --target sq66_xscope_4mic_firmware_empty
```

Expected: Build succeeds with no errors. The xscope probe names should appear in the build output as generated constants.

- [ ] **Step 2: Build an existing SQ66 variant (regression check)**

```bash
cmake -B build_sq66_regression -G "Unix Makefiles" \
    -DENABLE_ALL_FFVA_PIPELINES=ON \
    -DUSE_DEV_MODE=ON
cmake --build build_sq66_regression --target sq66_firmware_empty
```

Expected: Build succeeds. The observation calls in the pipeline file compile to empty stubs (no xscope overhead, no link to xscope_audio_io.c).

- [ ] **Step 3: Commit build config if any fixes were needed**

---

## Task 8: Integrate observation points into fixed_delay pipeline (Tile 1)

**Files:**
- Modify: `satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t1.c`

Same pattern as Task 6, but also adds AEC observation points in the `stage_aec()` function.

- [ ] **Step 1: Add include and observation calls**

At the top of the file, after the `#include "control/audio_pipeline_settings_servicer.h"` line, add:

```c
#include "xscope_audio_io.h"
```

In `audio_pipeline_input_i`, after `audio_pipeline_input()` and before `stage_apply_mic_gain()`, add:

```c
    // Observation: raw mic after PDM decode, before gain
    xscope_audio_io_send_raw_mic(frame_data->mic_samples_passthrough);
```

After `stage_apply_mic_gain(frame_data)`, add:

```c
    // Observation: mic after gain stage
    xscope_audio_io_send_gain_mic(frame_data->mic_samples_passthrough);
```

In `stage_aec()`, after the `aec_process_frame_1thread()` call and before the `memcpy` that overwrites `frame_data->samples`, add observation points for the pre-AEC data. Since the AEC output goes into `stage1_output` and then `frame_data->samples` gets overwritten, place observations at the right points:

After the `aec_calc_corr_factor` line, before the `memcpy` of `stage1_output` to `frame_data->samples`, add:

```c
    // Observation: AEC output (post-AEC mic channels)
    // AEC outputs 2 channels into stage1_output; send them as 2 of the 4 mic slots
    {
        int32_t aec_out_4ch[AP_MAX_Y_CHANNELS][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
        memset(aec_out_4ch, 0, sizeof(aec_out_4ch));
        memcpy(aec_out_4ch, stage1_output, AP_MAX_Y_CHANNELS * appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
        xscope_audio_io_send_aec_mic(aec_out_4ch);
    }
```

Note: AEC is hardcoded to 2 output channels (`AP_MAX_Y_CHANNELS=2`). The observation sends 2 populated + 2 zeroed channels. This is a known limitation of the current AEC architecture.

The `stage_aec` function should look like:

```c
static void stage_aec(frame_data_t *frame_data)
{
#if appconfAUDIO_PIPELINE_SKIP_AEC
#else
    int32_t DWORD_ALIGNED stage1_output[AEC_MAX_Y_CHANNELS][appconfAUDIO_PIPELINE_FRAME_ADVANCE];

    aec_process_frame_1thread(
            &aec_state.aec_main_state,
            &aec_state.aec_shadow_state,
            stage1_output,
            NULL,
            frame_data->samples,
            frame_data->aec_reference_audio_samples);

    frame_data->max_ref_energy = aec_calc_max_input_energy(
                                    frame_data->aec_reference_audio_samples,
                                    aec_state.aec_main_state.shared_state->num_x_channels);
    frame_data->aec_corr_factor = aec_calc_corr_factor(&aec_state.aec_main_state, 0);

    // Observation: AEC output
    {
        int32_t aec_out_4ch[AP_MAX_Y_CHANNELS][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
        memset(aec_out_4ch, 0, sizeof(aec_out_4ch));
        memcpy(aec_out_4ch, stage1_output, AP_MAX_Y_CHANNELS * appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
        xscope_audio_io_send_aec_mic(aec_out_4ch);
    }

    memcpy(frame_data->samples, stage1_output, AEC_MAX_Y_CHANNELS * appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
#endif
}
```

Note: AEC residual observation is deferred — the current AEC API does not expose the residual signal directly. This will be added when the AEC integration is enhanced in a future iteration.

- [ ] **Step 2: Commit**

```bash
git add satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t1.c
git commit -m "feat: add xscope observation points to fixed_delay pipeline Tile 1"
```

---

## Task 9: Integrate observation points into fixed_delay pipeline (Tile 0)

**Files:**
- Modify: `satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t0.c`

Add observation calls at IC, VNR, NS, and AGC stages.

- [ ] **Step 1: Add include and observation calls**

At the top of the file, after the existing includes and before `#if ON_TILE(0)`, add:

```c
#include "xscope_audio_io.h"
```

In `stage_vnr_and_ic()`, after `ic_adapt()` and before the `memcpy` of `ic_output`, add:

```c
    // Observation: IC output
    {
        int32_t ic_obs[2][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
        memset(ic_obs, 0, sizeof(ic_obs));
        memcpy(ic_obs[0], ic_output, appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
        xscope_audio_io_send_ic_out(ic_obs);
    }
```

In `stage_ns()`, after the `ns_process_frame` call and before the `memcpy`, add:

```c
    // Observation: NS output
    {
        int32_t ns_obs[2][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
        memset(ns_obs, 0, sizeof(ns_obs));
        memcpy(ns_obs[0], ns_output, appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
        xscope_audio_io_send_ns_out(ns_obs);
    }
```

In `stage_agc()`, after the `agc_process_frame` call and before the final `memcpy`, add:

```c
    // Observation: AGC output
    {
        int32_t agc_obs[2][appconfAUDIO_PIPELINE_FRAME_ADVANCE];
        memset(agc_obs, 0, sizeof(agc_obs));
        memcpy(agc_obs[0], agc_output, appconfAUDIO_PIPELINE_FRAME_ADVANCE * sizeof(int32_t));
        xscope_audio_io_send_agc_out(agc_obs);
    }
```

- [ ] **Step 2: Commit**

```bash
git add satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t0.c
git commit -m "feat: add xscope observation points to fixed_delay pipeline Tile 0"
```

---

## Task 10: Create host-side Python recording script

**Files:**
- Create: `scripts/test_xscope_recorder.py`

The script uses `mic_array.xscope.Endpoint` (ctypes wrapper around `xscope_endpoint.so`) to connect to the device and record probe data. For `xscope_bytes` probes, the raw audio data arrives in the `data_bytes` parameter of the `on_record` callback.

Key design decisions:
- Subclass `Endpoint` to override `on_record` and handle byte-array data from `xscope_bytes()` probes
- Audio probes: parse `data_bytes` as `int32_t` arrays, accumulate into per-probe buffers
- Metadata probes: `data_val` contains the float/int value from `xscope_float()`/`xscope_int()`
- Write output as multi-channel WAV files grouped by observation stage

- [ ] **Step 1: Create the script**

```python
#!/usr/bin/env python3
"""xSCOPE 4-mic recording script for XK-VOICE-SQ66.

Records audio observation points streamed from the firmware via xTAG4.
Uses mic_array.xscope.Endpoint for transport.

Usage:
    # Start firmware first:
    xrun --xscope-port localhost:10234 <firmware.xe>

    # Record all observation points for 5 seconds:
    python test_xscope_recorder.py --port 10234 --duration 5

    # Record only raw and gain mics:
    python test_xscope_recorder.py --port 10234 --probes mic_raw mic_gain
"""

import argparse
import os
import sys
import time
import signal
import struct
import threading
from collections import defaultdict

import numpy as np
from scipy.io import wavfile

# Add mic_array module to path
sys.path.insert(0, os.path.join(
    os.environ.get('XMOS_TOOL_PATH', ''), '..', 'workspace',
    'modules', 'io', 'modules', 'mic_array', 'script'
))

try:
    from mic_array.xscope import Endpoint
except ImportError:
    print("ERROR: Cannot import mic_array.xscope. Ensure XMOS_TOOL_PATH is set.")
    print("       The mic_array module is at modules/io/modules/mic_array/script/")
    sys.exit(1)


# Probe names matching config-xscope-4mic.xscope
AUDIO_PROBES = [
    'mic_raw_0', 'mic_raw_1', 'mic_raw_2', 'mic_raw_3',
    'mic_gain_0', 'mic_gain_1', 'mic_gain_2', 'mic_gain_3',
    'mic_aec_0', 'mic_aec_1', 'mic_aec_2', 'mic_aec_3',
    'aec_residual_0', 'aec_residual_1', 'aec_residual_2', 'aec_residual_3',
    'ic_out_0', 'ic_out_1',
    'ic_residual_0', 'ic_residual_1',
    'ns_out_0', 'ns_out_1',
    'agc_out_0', 'agc_out_1',
    'beam_0', 'beam_1', 'beam_2',
]

METADATA_PROBES = [
    'vnr_value', 'agc_gain',
    'doa_angle', 'doa_angle_raw', 'doa_confidence',
    'vad_beam_0', 'vad_beam_1', 'vad_beam_2',
    'beam_selection', 'input_source',
]

FRAME_ADVANCE = 240
SAMPLE_RATE = 16000
BYTES_PER_SAMPLE = 4


def parse_args():
    parser = argparse.ArgumentParser(description='xSCOPE 4-mic recorder')
    parser.add_argument('--host', default='localhost', help='xSCOPE server hostname')
    parser.add_argument('--port', default='10234', help='xSCOPE server port')
    parser.add_argument('--duration', type=float, default=5.0, help='Recording duration in seconds')
    parser.add_argument('--output-dir', default='recordings', help='Output directory for WAV files')
    parser.add_argument('--probes', nargs='*', default=None,
                        help='Probe name prefixes to record (e.g. mic_raw mic_gain). Records all if omitted.')
    return parser.parse_args()


class XscopeRecorder(Endpoint):
    """Extended Endpoint that captures xscope_bytes audio data."""

    def __init__(self, probe_filter=None):
        super().__init__()
        self._audio_buffers = defaultdict(list)
        self._metadata_values = defaultdict(list)
        self._metadata_timestamps = defaultdict(list)
        self._lock = threading.Lock()
        self._start_time = None
        self._probe_filter = probe_filter  # list of prefixes, or None for all

    def _should_capture(self, probe_name):
        if self._probe_filter is None:
            return True
        return any(probe_name.startswith(prefix) for prefix in self._probe_filter)

    def on_record(self, id_, timestamp, length, data_val, data_bytes):
        probe_info = self._probe_info.get(id_)
        if probe_info is None:
            return

        probe_name = probe_info['name']

        if not self._should_capture(probe_name):
            return

        if self._start_time is None:
            self._start_time = time.time()

        if probe_name in METADATA_PROBES:
            # Scalar value from xscope_float() or xscope_int()
            if probe_info.get('data_type') == 3:  # XSCOPE_FLOAT
                val = struct.unpack('f', struct.pack('I', data_val & 0xFFFFFFFF))[0]
            else:
                val = int(data_val & 0xFFFFFFFF)
                if val >= 0x80000000:
                    val -= 0x100000000

            with self._lock:
                self._metadata_values[probe_name].append(val)
                self._metadata_timestamps[probe_name].append(timestamp)
        else:
            # Byte array from xscope_bytes()
            num_samples = length // BYTES_PER_SAMPLE
            if num_samples > 0 and data_bytes:
                samples = np.frombuffer(data_bytes[:length], dtype=np.int32)
                with self._lock:
                    self._audio_buffers[probe_name].append(samples.copy())

    def get_audio_data(self, probe_name):
        """Get accumulated audio samples for a probe as a numpy array."""
        with self._lock:
            chunks = self._audio_buffers.get(probe_name, [])
            if not chunks:
                return np.array([], dtype=np.int32)
            return np.concatenate(chunks)

    def get_metadata(self, probe_name):
        """Get accumulated metadata values for a probe."""
        with self._lock:
            return list(self._metadata_values.get(probe_name, []))

    def elapsed(self):
        if self._start_time is None:
            return 0.0
        return time.time() - self._start_time


def group_probes_by_prefix(probe_names):
    """Group probe names by their prefix (e.g. mic_raw_0..3 -> mic_raw)."""
    groups = defaultdict(list)
    for name in probe_names:
        parts = name.rsplit('_', 1)
        if len(parts) == 2 and parts[1].isdigit():
            prefix = parts[0]
        else:
            prefix = name
        groups[prefix].append(name)
    return dict(groups)


def write_audio_wav(output_dir, group_name, probe_names, recorder):
    """Write a multi-channel WAV file for a group of audio probes."""
    channels = []
    max_len = 0
    for name in sorted(probe_names):
        data = recorder.get_audio_data(name)
        if len(data) > max_len:
            max_len = len(data)
        channels.append(data)

    if max_len == 0:
        print(f"  No data for {group_name}, skipping")
        return

    # Pad shorter channels with zeros
    padded = []
    for ch in channels:
        if len(ch) < max_len:
            padded.append(np.pad(ch, (0, max_len - len(ch))))
        else:
            padded.append(ch)

    # Interleave channels: shape (max_len, num_channels)
    interleaved = np.column_stack(padded)

    # Normalize int32 to int16 for WAV
    wav_data = (interleaved >> 16).astype(np.int16)

    filepath = os.path.join(output_dir, f"{group_name}.wav")
    wavfile.write(filepath, SAMPLE_RATE, wav_data)
    print(f"  Written {filepath} ({max_len} samples, {len(padded)} channels, "
          f"{max_len / SAMPLE_RATE:.2f}s)")


def write_metadata_csv(output_dir, probe_name, recorder):
    """Write metadata values to a CSV file."""
    values = recorder.get_metadata(probe_name)
    if not values:
        return

    filepath = os.path.join(output_dir, f"{probe_name}.csv")
    with open(filepath, 'w') as f:
        f.write("frame,value\n")
        for i, val in enumerate(values):
            f.write(f"{i},{val}\n")
    print(f"  Written {filepath} ({len(values)} values)")


def main():
    args = parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    print(f"Connecting to xSCOPE at {args.host}:{args.port}...")
    recorder = XscopeRecorder(probe_filter=args.probes)

    if recorder.connect(args.host, args.port):
        print("ERROR: Failed to connect")
        sys.exit(1)

    print("Connected. Recording...")
    print(f"  Duration: {args.duration}s")
    print(f"  Probe filter: {args.probes or 'all'}")
    print("  Press Ctrl+C to stop early")

    try:
        # Consume all probes
        recorder.consume(lambda ts, name, val: None)
        time.sleep(args.duration)
    except KeyboardInterrupt:
        print("\nStopped by user")
    finally:
        elapsed = recorder.elapsed()
        print(f"\nRecording complete ({elapsed:.2f}s)")
        print("Saving...")

        # Save audio probes
        captured_audio = [p for p in AUDIO_PROBES if recorder.get_audio_data(p).size > 0]
        groups = group_probes_by_prefix(captured_audio)
        for group_name, probe_names in sorted(groups.items()):
            write_audio_wav(args.output_dir, group_name, probe_names, recorder)

        # Save metadata
        for probe_name in METADATA_PROBES:
            values = recorder.get_metadata(probe_name)
            if values:
                write_metadata_csv(args.output_dir, probe_name, recorder)

        recorder.disconnect()
        print(f"\nOutput saved to {args.output_dir}/")


if __name__ == '__main__':
    main()
```

- [ ] **Step 2: Commit**

```bash
git add scripts/test_xscope_recorder.py
git commit -m "feat: add host-side xscope recording script"
```

---

## Task 11: End-to-end verification on hardware

**Files:**
- No new files

This task requires physical hardware (XK-VOICE-SQ66 + xTAG4). It verifies the complete recording pipeline.

- [ ] **Step 1: Flash and run the firmware**

```bash
# Build the firmware (from Task 7 build directory)
cmake --build build_xscope_4mic --target sq66_xscope_4mic_firmware_empty

# Run with xscope port open
xrun --xscope-port localhost:10234 build_xscope_4mic/bin/sq66_xscope_4mic_firmware_empty.xe
```

- [ ] **Step 2: Record audio from the device**

In a separate terminal:

```bash
python scripts/test_xscope_recorder.py --port 10234 --duration 5 --output-dir recordings/test1
```

Expected: WAV files created in `recordings/test1/` for each observation stage that has data. For the empty pipeline, expect `mic_raw.wav` (4 channels) and `mic_gain.wav` (4 channels).

- [ ] **Step 3: Verify recorded audio**

```bash
# Check WAV file contents
python -c "
from scipy.io import wavfile
import numpy as np
sr, data = wavfile.read('recordings/test1/mic_raw.wav')
print(f'mic_raw.wav: sr={sr}, shape={data.shape}, dtype={data.dtype}')
print(f'  peak: {np.max(np.abs(data))}')
print(f'  rms: {np.sqrt(np.mean(data.astype(float)**2)):.1f}')
print(f'  duration: {len(data)/sr:.2f}s')
"
```

Expected: 4-channel WAV at 16 kHz, ~5 seconds duration, non-zero audio content (room noise at minimum).

- [ ] **Step 4: Commit any fixes**

---

## Spec Coverage Check

| Spec Requirement | Task |
|---|---|
| `appconfXSCOPE_4MIC_ENABLED` compile flag | Task 1 |
| Probe XML config (~42 probes) | Task 2 |
| Conditional stub pattern in header | Task 3 |
| `xscope_bytes()` for audio, `xscope_float()` for metadata | Task 4 |
| Build variant CMake | Task 5 |
| Empty pipeline Tile 1 observation points | Task 6 |
| Build verification (new + regression) | Task 7 |
| fixed_delay Tile 1 (raw, gain, AEC) | Task 8 |
| fixed_delay Tile 0 (IC, NS, AGC) | Task 9 |
| Host Python recording script | Task 10 |
| End-to-end hardware verification | Task 11 |
| Cross-platform stub pattern (SATELLITE1 ready) | Task 3 (stubs) |
| Input source switching (host injection) | **Deferred** — host upload has 255-byte limit, needs chunking protocol |
| adec pipeline integration | **Deferred** — needs channel constant migration |
| Host injection mode | **Deferred** — depends on chunking protocol |
| Beamforming/DOA/VAD probes | **Deferred** — future DSP stages |
