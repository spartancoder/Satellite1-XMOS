# DOA SPI Export Design Specification

**Date:** 2026-03-23
**Status:** Draft
**Branch:** four_mics_sandbox

## Overview

This specification describes how to export Direction of Arrival (DOA) data calculated via GCC-PHAT from the XMOS firmware to an upstream ESP32 via SPI. The design supports current single-source detection and future multi-source detection (up to 3 sources).

## Requirements

### Functional Requirements

1. **FR-1:** Export DOA azimuth and elevation angles via SPI to ESP32
2. **FR-2:** Support up to 3 simultaneous sound sources (future)
3. **FR-3:** Include confidence values for each detected source
4. **FR-4:** Make DOA data available to internal beamformer (future)

### Non-Functional Requirements

1. **NFR-1:** Single degree accuracy (1°) minimum
2. **NFR-2:** Efficient for both XMOS (no FP overhead) and ESP32
3. **NFR-3:** Low latency access for internal beamformer
4. **NFR-4:** ESP32 polls at ~100ms intervals

## Design Decisions

### 1. Data Format: Int16 Fixed-Point

**Decision:** Use int16 centidegrees for angles, int8 for confidence.

**Rationale:**
- XMOS xcore architecture is optimized for integer operations (no FP hardware)
- ESP32-S3 handles integers efficiently
- Centidegrees (0-35999) provide 0.01° precision, exceeding 1° requirement
- Compact: 5 bytes per source vs 12 bytes with float32

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| azimuth | int16 | 0-35999 | Centidegrees (0.00°-359.99°) |
| elevation | int16 | -9000 to +9000 | Centidegrees (-90.00° to +90.00°) |
| confidence | uint8 | 0-100 | Percentage confidence |
| vad | uint8 | 0-1 | Voice Activity Detection flag (0=silence/noise, 1=voice) |

### 2. Servicer Architecture: New DOA Servicer

**Decision:** Create a new dedicated DOA servicer (RESID 31) rather than extending audio_cfg_servicer.

**Rationale:**
- Clean separation of concerns
- Easier to extend independently
- Dedicated resource ID for DOA functionality

### 3. Data Flow: Shared Struct

**Decision:** Use a shared struct updated by audio pipeline, read by servicer and beamformer.

**Rationale:**
- Zero overhead for internal beamformer (direct memory read)
- No queue contention for high-frequency internal access
- ESP32 polling is infrequent (~100ms), no contention issues
- Single source of truth

### 4. Multiple Sources: Fixed Array

**Decision:** Always return 3 source slots, unused slots have confidence=0.

**Rationale:**
- Fixed payload size simplifies SPI protocol
- ESP32 knows exact structure ahead of time
- Unused slots clearly indicated by confidence=0

### 5. Smoothing: Raw Output

**Decision:** Expose raw DOA output without smoothing.

**Rationale:**
- Keeps servicer simple
- Smoothing can be added later in pipeline, beamformer, or ESP32
- Allows iteration on smoothing algorithm without firmware changes

## Data Structures

### DOA Source Structure

```c
typedef struct {
    int16_t azimuth_cdeg;    // Centidegrees: 0-35999 (0.00°-359.99°)
    int16_t elevation_cdeg;  // Centidegrees: -9000 to +9000 (-90.00° to +90.00°)
    uint8_t confidence;      // 0-100 percentage
    uint8_t vad;             // Voice Activity Detection: 0=no voice, 1=voice detected
} doa_source_t;
```

**Size:** 6 bytes per source

### DOA Result Structure

```c
typedef struct {
    doa_source_t sources[3]; // Fixed 3 slots
    uint8_t count;           // Number of valid sources (1-3)
} doa_result_t;
```

**Size:** 19 bytes total (6 × 3 + 1)

## SPI Protocol

### Resource ID

| Servicer | RESID |
|----------|-------|
| DOA Servicer | 31 |

### Commands

| CMD ID | Direction | Name | Payload | Description |
|--------|-----------|------|---------|-------------|
| 0 | Read | GET_DOA | 19 bytes | Get all DOA data |

### Response Payload Layout (19 bytes)

| Offset | Bytes | Field | Type | Description |
|--------|-------|-------|------|-------------|
| 0 | 2 | sources[0].azimuth | int16 LE | Azimuth in centidegrees |
| 2 | 2 | sources[0].elevation | int16 LE | Elevation in centidegrees |
| 4 | 1 | sources[0].confidence | uint8 | Confidence 0-100 |
| 5 | 1 | sources[0].vad | uint8 | Voice Activity Detection (0 or 1) |
| 6 | 2 | sources[1].azimuth | int16 LE | Azimuth in centidegrees |
| 8 | 2 | sources[1].elevation | int16 LE | Elevation in centidegrees |
| 10 | 1 | sources[1].confidence | uint8 | Confidence 0-100 |
| 11 | 1 | sources[1].vad | uint8 | Voice Activity Detection (0 or 1) |
| 12 | 2 | sources[2].azimuth | int16 LE | Azimuth in centidegrees |
| 14 | 2 | sources[2].elevation | int16 LE | Elevation in centidegrees |
| 16 | 1 | sources[2].confidence | uint8 | Confidence 0-100 |
| 17 | 1 | sources[2].vad | uint8 | Voice Activity Detection (0 or 1) |
| 18 | 1 | count | uint8 | Number of valid sources (1-3) |

### Example Transaction

**ESP32 Request (Read DOA):**
```
TX: [0x1F] [0x80] [0x13]  // RESID=31, CMD=0|0x80 (read), LEN=19
```

**XMOS Response:**
```
RX: [0x13] [0x00] [az0_lo] [az0_hi] [el0_lo] [el0_hi] [conf0] [vad0] ...
    // LEN=19, STATUS=SUCCESS, followed by 19-byte payload
```

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         main.c                              │
│                                                             │
│  ┌────────────────┐         ┌──────────────────────────┐   │
│  │ Audio Pipeline │────────►│ doa_result_t (shared)    │   │
│  │                │  update │ [sources[3], count]      │   │
│  │ doa4_process_  │         │                          │   │
│  │ frame()        │         │                          │   │
│  └────────────────┘         └────────────┬─────────────┘   │
│                                          │                  │
│  ┌────────────────┐                      │                  │
│  │ Beamformer     │◄─────────────────────┤                  │
│  │ (future)       │       read           │                  │
│  └────────────────┘                      │                  │
│                                          │                  │
└──────────────────────────────────────────┼──────────────────┘
                                           │
┌──────────────────────────────────────────┼──────────────────┐
│                                  doa_servicer.c             │
│                                          │                  │
│  ┌────────────────────────────────────┐  │                  │
│  │ DOA Servicer Task                  │◄─┘                  │
│  │                                    │  read on SPI poll   │
│  │ - Registered with RESID 31         │                     │
│  │ - Handles CMD 0 (GET_DOA)          │                     │
│  │ - Reads shared doa_result_t        │                     │
│  └─────────────┬──────────────────────┘                     │
│                │                                            │
│                ▼                                            │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ SPI Response                                        │    │
│  │ [LEN][STATUS][19-byte DOA payload]                  │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Implementation Plan

### Files to Create

| File | Purpose |
|------|---------|
| `satellite-xmos-firmware/src/control/doa_servicer.h` | DOA servicer header, data structures |
| `satellite-xmos-firmware/src/control/doa_servicer.c` | DOA servicer implementation |

### Files to Modify

| File | Changes |
|------|---------|
| `satellite-xmos-firmware/src/main.c` | Add shared `doa_result_t`, update after `doa4_process_frame()` |
| `satellite-xmos-firmware/src/app_conf.h` | Add DOA servicer resource ID |
| `satellite-xmos-firmware/bsp_config/SATELLITE1/platform/platform_init.c` | Increment servicer count |
| `satellite-xmos-firmware/firmware.cmake` | Add doa_servicer.c to build |

### Implementation Steps

1. **Define data structures** in `doa_servicer.h`
2. **Create DOA servicer** following existing servicer pattern (audio_cfg_servicer as template)
3. **Add shared `doa_result_t`** global in `main.c`
4. **Update pipeline** to write DOA results to shared struct after `doa4_process_frame()`
5. **Initialize and start servicer** in startup sequence
6. **Update servicer count** in `device_control_init()`

## Current vs Future

| Aspect | Current Implementation | Future Enhancement |
|--------|------------------------|-------------------|
| Sources | 1 (sources[0] only) | Up to 3 sources |
| Elevation | Always 0 (flat circular array) | Calculated if 3D array geometry |
| Confidence | Fixed 100 (placeholder) | From GCC-PHAT peak correlation strength |
| VAD | Fixed 1 (placeholder) | From voice activity detection pipeline |
| Smoothing | None | Optional moving average filter |

## Testing

### Unit Testing

1. Verify servicer responds to RESID 31, CMD 0
2. Verify payload is exactly 19 bytes
3. Verify byte order (little-endian) for int16 fields
4. Verify VAD field is present (placeholder value 1)

### Integration Testing

1. ESP32 can successfully poll DOA via SPI
2. DOA values update as audio input changes
3. Unused source slots have confidence=0

### Hardware Testing

1. Verify DOA accuracy with known sound source positions
2. Verify ESP32 receives correct values at 100ms polling rate

## References

- `modules/fph/doa/api/gcc_phat.h` - Current DOA API
- `modules/fph/doa/src/gcc_phat.c` - Current DOA implementation
- `docs/SPI_DATA_FLOW_TO_ESP32.md` - SPI protocol documentation
- `satellite-xmos-firmware/src/control/audio_cfg_servicer.c` - Servicer pattern reference
