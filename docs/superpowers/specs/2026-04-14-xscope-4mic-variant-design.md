# xSCOPE 4-Mic Firmware Variant Design

## Overview

Create an xSCOPE firmware variant for the XK-VOICE-SQ66 that streams 4 raw microphone channels from device to host via xTAG4, accepts 4-channel WAV input from host to device, and provides observation points at every DSP pipeline stage for debug and tuning. The design supports three pipeline variants (empty, fixed_delay, adec) implemented incrementally starting with empty.

## Architecture

### Host Communication Stack

Dual transport approach:

- **lib_device_control** (over xSCOPE transport): control commands such as input source switching, parameter tuning
- **Raw xscope probes** (`xscope_raw()`): high-bandwidth framed audio and metadata streaming

lib_device_control lives at `modules/rtos/modules/lib_device_control` (v5.0.1) and supports XSCOPE as a transport natively.

### Build Variant

A new CMake variant file `xk-voice-sq66-xscope-4mic.cmake` extends the existing SQ66 build pattern. It enables:

- `-fxscope` flag for xTAG4 JTAG-based xscope
- Extended `config-xscope-4mic.xscope` XML for base probes (most probes registered via code)
- `xscope_audio_io.c/h` compiled into the firmware
- Pipeline selection via `FFVA_PIPELINES_INT` (same mechanism as existing variants)

### Input Source Switching

Runtime switchable between PDM hardware input and xscope-injected input:

- `xscope_audio_io_host_input_active()` returns whether host is currently sending audio
- When active, pipeline reads injected frames instead of PDM-decoded mic data
- When inactive, pipeline reads PDM data as normal
- Raw mic monitoring stream (device-to-host) operates independently of input source

## Cross-Platform Portability

### Platform Differences

SQ66 and SATELLITE1 share identical pipeline topology but differ in hardware mapping:

| Aspect | XK-VOICE-SQ66 | SATELLITE1 |
|---|---|---|
| PDM capture tile | Tile 0 | Tile 1 |
| I2S output tile | Tile 1 | Tile 1 |
| DDR mode | Off (`USE_DDR=0`) | On (`USE_DDR=1`) |
| Mic port | 8-bit (`PORT_8D`) | 4-bit (`PORT_4D`) |
| Mic mapping | `{4,5,6,7}` | `{0,4,1,5}` |
| Pipeline channels | 4 in / 2 proc / 2 ref | 4 in / 2 proc / 2 ref |
| Frame size | 240 samples | 240 samples |

The pipeline code (`audio_pipeline_t1.c` / `audio_pipeline_t0.c`) uses `#if ON_TILE(0/1)` guards and is **already identical** on both platforms. PDM capture runs on whichever tile has `MICARRAY_TILE_NO`, and the intertile layer abstracts the tile routing. No tile layout restructuring is needed or desirable — the hardware pin assignments are fixed per board.

### Compile Flag Design

A single `appconfXSCOPE_4MIC_ENABLED` flag controls all xscope observation code. When disabled, every function in `xscope_audio_io.h` compiles to an empty inline stub:

```c
// xscope_audio_io.h
#if appconfXSCOPE_4MIC_ENABLED

void xscope_audio_io_init(void);
void xscope_audio_io_send_gain_mic(const int32_t samples[4][FRAME_ADVANCE]);
// ... full implementations linked from xscope_audio_io.c

#else

static inline void xscope_audio_io_init(void) {}
static inline void xscope_audio_io_send_gain_mic(const int32_t samples[4][FRAME_ADVANCE]) { (void)samples; }
// ... all functions become zero-overhead no-ops

#endif
```

This means:
- **Pipeline files contain zero IFDEFs** — observation calls are always present, compiling to nothing when the flag is off
- **SATELLITE1 enablement** is a single CMake line: `appconfXSCOPE_4MIC_ENABLED=1`
- **ESP32 communication and peripheral control** on SATELLITE1 are completely untouched
- **No conditional code in pipeline logic** — the observation calls are non-invasive either way

### SATELLITE1 Adaptation

To port to SATELLITE1 after SQ66 testing:

1. Create `satellite1-xscope-4mic.cmake` (copy SQ66 variant, change BSP paths and board name — ~10 lines)
2. Set `appconfXSCOPE_4MIC_ENABLED=1` in the new variant
3. Link `xscope_audio_io.c`
4. No changes to `xscope_audio_io.c/h` — the module is platform-agnostic
5. No changes to pipeline files — observation calls already work on both tile layouts
6. Reuse `config-xscope-4mic.xscope` as-is
7. Reuse `scripts/test_xscope_recorder.py` as-is

## Module API

New module `xscope_audio_io.c/h` provides the public API:

```c
void xscope_audio_io_init(void);
bool xscope_audio_io_host_input_active(void);
int xscope_audio_io_get_injected_frame(int32_t samples[4][FRAME_ADVANCE]);

void xscope_audio_io_send_raw_mic(const int32_t samples[4][FRAME_ADVANCE]);
void xscope_audio_io_send_gain_mic(const int32_t samples[4][FRAME_ADVANCE]);
void xscope_audio_io_send_aec_mic(const int32_t samples[4][FRAME_ADVANCE]);
void xscope_audio_io_send_aec_residual(const int32_t samples[4][FRAME_ADVANCE]);

void xscope_audio_io_send_beam(int beam_idx, const int32_t samples[FRAME_ADVANCE]);

void xscope_audio_io_send_doa(float angle_smoothed, float angle_raw, float confidence);
void xscope_audio_io_send_vad(int beam_idx, float vad_value);
void xscope_audio_io_send_beam_selection(int selected_beam, int criteria);
```

## Probe Registration Layout

Probes are registered via code using `xscope_register()` with `xscope_raw()` for framed multi-sample streaming. The XML config provides only base probes; the bulk are code-registered at init time.

### Tile 1 Audio Probes (Device-to-Host)

| Probe Name | Type | Channels | Description |
|---|---|---|---|
| `mic_raw_0..3` | raw audio | 4 | Post-PDM decode, pre-gain |
| `mic_gain_0..3` | raw audio | 4 | Post-mic-gain |
| `mic_aec_0..3` | raw audio | 4 | Post-AEC (per-mic) |
| `aec_residual_0..3` | raw audio | 4 | AEC error signal (per-mic) |

### Tile 0 Audio Probes (Device-to-Host)

| Probe Name | Type | Channels | Description |
|---|---|---|---|
| `ic_out_0..1` | raw audio | 2 | Post-IC output |
| `ic_residual_0..1` | raw audio | 2 | IC error signal |
| `ns_out_0..1` | raw audio | 2 | Post-NS output |
| `agc_out_0..1` | raw audio | 2 | Post-AGC output |
| `beam_0..2` | raw audio | 3 | Post-beamform (future) |

### Tile 0 Metadata Probes (Device-to-Host)

Metadata probes use single-value mode (`xscope_probe_data_type`) rather than framed raw mode. At frame rate (~67 Hz), single-value probes have negligible overhead and are simpler to parse on the host.

| Probe Name | Type | Description |
|---|---|---|
| `vnr_value` | float | Voice-to-noise ratio |
| `agc_gain` | float | Current AGC gain |
| `doa_angle` | float | Smoothed DOA angle |
| `doa_angle_raw` | float | Per-frame DOA angle |
| `doa_confidence` | float | DOA confidence |
| `vad_beam_0..2` | float | VAD result per beam |
| `beam_selection` | int+int | Selected beam index + criteria |

### Host-to-Device Probes

| Probe Name | Type | Channels | Description |
|---|---|---|---|
| `mic_in_0..3` | raw audio | 4 | WAV input from host |
| `input_source` | int | 1 | 0=PDM, 1=xscope |

### Bandwidth Estimate

- Tile 1 audio: 16 channels x 4 bytes x 16000 Hz = ~1.0 MB/s
- Tile 0 audio: 9 channels x 4 bytes x 16000 Hz = ~0.58 MB/s
- Metadata: negligible (~few KB/s)
- Total: ~1.6 MB/s, well within xTAG4 practical throughput of 10-20 MB/s

## Pipeline Integration

### Tile 1 (empty pipeline)

```
PDM decode → xscope_audio_io_send_raw_mic()
           → mic gain → xscope_audio_io_send_gain_mic()
           → intertile TX (to Tile 0)
```

Input source switching occurs after PDM decode: if `xscope_audio_io_host_input_active()`, read injected frame and skip PDM decode, then continue through gain stage normally.

### Tile 1 (fixed_delay pipeline)

```
PDM decode → xscope_audio_io_send_raw_mic()
           → mic gain → xscope_audio_io_send_gain_mic()
           → AEC → xscope_audio_io_send_aec_mic()
                 → xscope_audio_io_send_aec_residual()
           → intertile TX
```

### Tile 1 (adec pipeline)

Same pattern as fixed_delay with ADEC-specific AEC integration. The adec pipeline currently uses the old `appconfAUDIO_PIPELINE_CHANNELS` constant and needs migration to named channel constants as part of this work.

### Tile 0 (all pipelines)

```
intertile RX → IC → xscope_audio_io_send ic_out/residual
             → VNR → xscope_audio_io_send vnr_value
             → NS → xscope_audio_io_send ns_out
             → AGC → xscope_audio_io_send agc_out/agc_gain
             → beamforming (future) → xscope_audio_io_send_beam/vad/doa/beam_selection
             → output
```

### Observation Point Placement

Observation calls are non-invasive: they snapshot a pointer to the current frame data and call `xscope_raw()`. They do not modify the audio data or alter pipeline flow. When xscope is not connected, the calls are effectively no-ops (minimal overhead).

## Host-Side Script

Python script `scripts/test_xscope_recorder.py` using lib_device_control Python bindings:

### Recording Mode

- Connect to device via xscope transport
- Subscribe to raw audio probes
- Receive framed audio data (240 samples per frame at 16 kHz)
- Write to multi-channel WAV files (one file per observation point, or configurable grouping)
- Display metadata values (DOA, VAD, AGC gain) in real-time console output

### Injection Mode

- Load 4-channel WAV file from disk
- Stream frames to `mic_in_0..3` probes at 16 kHz rate
- Simultaneously record device output for round-trip testing

### Combined Mode

- Inject test signal while recording all observation points
- Enables end-to-end pipeline verification with known input

### IC/VNR/NS/AGC Channel Counts

The number of channels for IC, VNR, NS, and AGC observation points is deferred. The current pipeline processes 2 channels in these stages, but with 4 mics and 3 future beams, this may change. The probe registration and host script will be designed to accommodate variable channel counts when this decision is made.

## Files to Create/Modify

### New Files

| File | Purpose |
|---|---|
| `satellite-xmos-firmware/xk-voice-sq66-xscope-4mic.cmake` | Build variant definition |
| `satellite-xmos-firmware/config-xscope-4mic.xscope` | Extended xscope XML config |
| `satellite-xmos-firmware/src/xscope_audio_io.c` | Audio I/O module implementation |
| `satellite-xmos-firmware/src/xscope_audio_io.h` | Module header with public API |
| `scripts/test_xscope_recorder.py` | Host-side recording/injection script |

### Modified Files

| File | Change |
|---|---|
| `satellite-xmos-firmware/firmware.cmake` | Add new variant include |
| `satellite-xmos-firmware/audio_pipelines/reference/empty/audio_pipeline_t1.c` | Add xscope observation points + input switching |
| `satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t1.c` | Add xscope observation points + input switching |
| `satellite-xmos-firmware/audio_pipelines/reference/fixed_delay/audio_pipeline_t0.c` | Add xscope observation points |
| `satellite-xmos-firmware/audio_pipelines/reference/adec/*` | Add observation points + migrate channel constants |

## Implementation Order

1. **Build variant + xscope_audio_io module** (empty pipeline only)
2. **Host-side recording** (verify raw mic streaming works end-to-end)
3. **Input source switching** (host injection replaces PDM)
4. **fixed_delay pipeline integration** (add AEC observation points)
5. **adec pipeline integration** (migrate constants + add points)
6. **Tile 0 observation points** (IC, VNR, NS, AGC)
7. **Future: beamforming, DOA, VAD probes**
