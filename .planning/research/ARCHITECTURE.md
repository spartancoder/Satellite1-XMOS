# Architecture Research

**Domain:** XMOS XU316 Audio Processing (DOA, DTOA, Beamforming)
**Researched:** 2026-02-15
**Confidence:** MEDIUM

## Standard Architecture

### System Overview

```
+-----------------------------------------------------------------------------+
|                              Application Layer                               |
+-----------------------------------------------------------------------------+
|  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐      |
|  │  DOA API │  │Beamform  │  │ DTOA API │  │ Control  │      |
|  └─────┬────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘      |
|        │              │              │              │               |
|        ▼              ▼              ▼              ▼               |
|  ┌───────────────────────────────────────────────────────────────────────┐      |
|  │              Audio Pipeline Framework (generic_pipeline)               │      |
|  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐         │      |
|  │  │ Capture │ │  DOA    │ │Beamform │ │  Post   │         │      |
|  │  │ (PDM)   │ │  Stage  │ │  Stage  │ │Process  │         │      |
|  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘         │      |
|  └───────┼────────────┼────────────┼────────────┼─────────────┘      |
+----------┼------------┼------------┼------------┼----------------------+
           │            │            │            │
+----------┼------------┼------------┼------------┼----------------------+
|          ▼            ▼            ▼            ▼                      |
|  ┌───────────────────────────────────────────────────────────────────────┐      |
|  │                    Algorithm Libraries                            │      |
|  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │      |
|  │  │ lib_doa   │ │lib_beam   │ │lib_xcore  │ │lib_mic_  │   │      |
|  │  │   (GCC-   │ │   (MVDR)  │ │   _math   │ │ _array    │   │      |
|  │  │   PHAT)    │ │           │ │  (BFP)    │ │ (PDM)    │   │      |
|  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘   │      |
|  └───────────────────────────────────────────────────────────────────────┘      |
+-----------------------------------------------------------------------------+
           │
           ▼
+-----------------------------------------------------------------------------+
|                    Hardware Layer (XU316)                                |
|  Tile 0: Control, I2S, Output Stages  |  Tile 1: PDM, AEC, Input    |
+-----------------------------------------------------------------------------+
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|---------------|------------------------|
| **DOA Module** | Direction of Arrival estimation using GCC-PHAT algorithm | C library with frame processing API |
| **DTOA Module** | Differential Time of Arrival between mic pairs | C library calculating cross-correlation delays |
| **Beamforming Module** | MVDR delay-and-sum beamforming | C library with steerable beam weights |
| **Audio Pipeline** | Frame-based processing orchestration via generic_pipeline | Multi-stage FreeRTOS task pipeline |
| **Control API** | ESP32 communication via SPI for DOA data | Device control servicer pattern |
| **Mic Array Driver** | PDM microphone capture from 4-mic array | XMOS lib_mic_array driver |
| **VPU Integration** | Hardware acceleration for FFT/correlation | XU316 VPU instructions where applicable |

## Recommended Project Structure

```
modules/voice/modules/
├── lib_doa/                    # DOA estimation module
│   ├── api/
│   │   └── doa_api.h          # Public API (init, process_frame, get_direction)
│   ├── src/
│   │   ├── doa_gcc_phat.c     # GCC-PHAT implementation
│   │   ├── doa_geometry.c      # Mic array geometry calculations
│   │   └── doa_priv.h         # Internal state
│   ├── doc/
│   └── CMakeLists.txt
├── lib_beamforming/              # Beamforming module
│   ├── api/
│   │   ├── beamforming_api.h    # Public API (init, process_frame, set_direction)
│   │   └── beamforming_types.h  # Configuration types
│   ├── src/
│   │   ├── mvdr_beamform.c     # MVDR beamforming
│   │   ├── delay_sum.c         # Delay-and-sum fallback
│   │   └── beamforming_priv.h  # Internal state
│   ├── doc/
│   └── CMakeLists.txt
└── lib_dtoa/                   # DTOA module (optional separate or part of DOA)
    ├── api/
    │   └── dtoa_api.h
    ├── src/
    │   ├── dtoa_gcc.c          # GCC-based DTOA
    │   └── dtoa_priv.h
    ├── doc/
    └── CMakeLists.txt

satellite-xmos-firmware/
├── audio_pipelines/
│   └── mvdr/                     # New MVDR pipeline variant
│       ├── audio_pipeline.h           # Public pipeline API
│       ├── audio_pipeline_dsp.h        # Frame data structures
│       ├── audio_pipeline_t0.c         # Tile 0: beamforming + IC/VNR/NS/AGC
│       ├── audio_pipeline_t1.c         # Tile 1: PDM capture + DOA + DTOA
│       └── CMakeLists.txt
├── src/
│   ├── control/
│   │   └── doa_servicer.c          # DOA data to ESP32 via SPI
│   └── app_conf.h                  # Extended with DOA/beamforming config
```

### Structure Rationale

- **lib_doa, lib_beamforming, lib_dtoa:** Following existing `modules/voice/modules/lib_*` pattern (AEC, IC, NS, AGC, VNR)
  - Separation of concerns: DOA estimation independent of beamforming
  - Each module has clean public API for independent testing
  - Shared header structure with state, init, process_frame functions

- **mvdr audio_pipeline:** Following existing `audio_pipelines/reference/*` pattern
  - Separate T0/T1 implementations for dual-tile architecture
  - Uses generic_pipeline framework for multi-stage processing
  - Frame data structure extended to carry DOA information

- **doa_servicer:** Following existing `control/*servicer.c` pattern (GPIO, DFU, LED ring)
  - Device control framework for ESP32 SPI communication
  - Clean separation of control plane from data plane

## Architectural Patterns

### Pattern 1: XMOS Audio DSP Module

**What:** Standard module structure for audio processing algorithms

**When to use:** Any audio DSP component (DOA, beamforming, DTOA)

**Pattern:**
```c
// API header (api/module_api.h)
typedef struct module_state_t module_state_t;

typedef struct module_config_t {
    int sample_rate;
    int frame_advance;
    int num_channels;
    // module-specific config
} module_config_t;

// Required API functions
int32_t module_init(module_state_t *state, const module_config_t *config);
void module_process_frame(module_state_t *state,
                      int32_t (*output)[NUM_CHANNELS][FRAME_ADVANCE],
                      const int32_t (*input)[NUM_CHANNELS][FRAME_ADVANCE]);
```

**Rationale:** All existing XMOS voice modules (AEC, IC, NS, AGC) use this pattern. Frame-advance of 240 samples at 16kHz is standard. State structure holds FFT buffers, delay lines, etc.

### Pattern 2: Dual-Tile Pipeline Distribution

**What:** Split audio processing across Tile 0 and Tile 1

**When to use:** When processing has CPU/memory constraints or needs to leverage different tile capabilities

**Tile Assignment:**
- **Tile 1:** PDM capture, AEC, DOA estimation, DTOA
  - Reasons: PDM driver runs on T1, AEC already there, DOA needs raw mic data
  - Output: Direction estimate to T0 via intertile port

- **Tile 0:** Beamforming, IC, VNR, NS, AGC, I2S output
  - Reasons: More CPU available for complex algorithms, existing stages are here
  - Input: Processed audio + direction estimate from T1

**Inter-tile Communication:**
```c
// Extended frame_data_t includes DOA information
typedef struct {
    int32_t samples[4][FRAME_ADVANCE];  // 4 mic channels
    float_doa_estimate_t doa_result;     // Direction of arrival
    dtoa_delays_t dtoa_delays;         // Delays for beamforming
    // ... existing fields (vnr_pred_flag, etc.)
} frame_data_t;

// Send from T1 to T0 via existing port 7
rtos_intertile_tx(intertile_ctx, appconfAUDIOPIPELINE_PORT,
                  frame_data, sizeof(frame_data_t));
```

**Trade-offs:**
- **Pros:** Leverages existing T1 PDM/AEC, balanced CPU load
- **Cons:** More complex inter-tile data transfer

### Pattern 3: Generic Pipeline Integration

**What:** Use XMOS generic_pipeline framework for multi-stage processing

**When to use:** Any frame-based audio pipeline with multiple stages

**Example:**
```c
// Tile 1 pipeline stages
const pipeline_stage_t t1_stages[] = {
    (pipeline_stage_t)stage_pdm_capture,
    (pipeline_stage_t)stage_doa_estimation,
    (pipeline_stage_t)stage_dtoa_calculation,
};

// Tile 0 pipeline stages
const pipeline_stage_t t0_stages[] = {
    (pipeline_stage_t)stage_beamforming,
    (pipeline_stage_t)stage_ic_vnr,
    (pipeline_stage_t)stage_ns,
    (pipeline_stage_t)stage_agc,
    (pipeline_stage_t)stage_output_i2s,
};
```

**Trade-offs:**
- **Pros:** FreeRTOS task management, queue handling handled automatically
- **Cons:** Each stage runs in separate task (memory overhead)

### Pattern 4: Device Control Servicer

**What:** Use device_control framework for ESP32 communication

**When to use:** Control plane data (DOA direction, configuration) needs to go to ESP32 via SPI

**Example:**
```c
// doa_servicer.c
typedef struct {
    uint16_t direction_angle;  // 0-359 degrees
    uint8_t confidence;       // DOA confidence
} doa_status_t;

static control_ret_t doa_read_cmd_cb(control_resid_t resid,
                                     control_cmd_t cmd,
                                     uint8_t *payload,
                                     size_t payload_len,
                                     void *app_data)
{
    if (resid == DOA_CONTROL_RESOURCE_ID && cmd == DOA_CMD_GET_DIRECTION) {
        doa_status_t *status = (doa_status_t*)payload;
        status->direction_angle = current_doa.angle;
        status->confidence = current_doa.confidence;
    }
    return CONTROL_SUCCESS;
}
```

**Trade-offs:**
- **Pros:** Clean integration with existing SPI transport, reusable pattern
- **Cons:** Adds control plane overhead

## Data Flow

### Audio Data Flow

```
PDM Mics (4 channels)
    ↓
[Tile 1] PDM Capture (lib_mic_array)
    ↓
[Tile 1] DOA Estimation (GCC-PHAT)
    ├─→ Direction estimate (via port 7)
    └─→ DTOA delays (via port 7)
    ↓
Raw 4-channel audio (via port 7)
    ↓
[Tile 0] Beamforming (MVDR)
    ├─ Uses DTOA delays from T1
    └─ Steerable beam weights
    ↓
[Tile 0] IC/VNR/NS/AGC
    ↓
[Tile 0] I2S Output
    ↓
ESP32 (processed audio + DOA data via SPI control)
```

### Control Flow (DOA to ESP32)

```
[Tile 1] DOA Estimation Task
    ↓ Calculates new direction
    ↓ Updates shared DOA state
    ↓
[Tile 0] DOA Servicer Task
    ← Device Control Framework
    ← SPI Driver (Tile 0)
    ↓
ESP32
    ← SPI Transaction (reads DOA data)
    ↓ ESP32 Application
```

### Key Data Flows

1. **Mic Capture to DOA:** PDM -> Decimation -> Frame buffer -> GCC-PHAT -> Angle
2. **DOA to Beamforming:** Angle -> DTOA calculation -> Per-channel delays -> MVDR weights
3. **DOA to ESP32:** Angle/Confidence -> Device Control -> SPI -> ESP32
4. **Beamformed Output:** 4-mic inputs + weights -> Single channel output -> IC/VNR/NS/AGC -> I2S

## Scaling Considerations

| Complexity | Memory Requirements | CPU Load | Real-time Constraints |
|------------|-------------------|------------|---------------------|
| GCC-PHAT (4 mics, 240 samples) | ~8KB (FFT buffers, correlation) | Medium (FFT per frame) | 16kHz frame rate = 15ms deadline |
| MVDR Beamforming (4 channels) | ~4KB (covariance, weights) | Medium-High (matrix ops) | Must fit in frame time |
| Combined Pipeline | ~20KB total | High (all stages) | T1: capture+DOA, T0: beamform+AGC |

### Scaling Priorities

1. **First bottleneck:** FFT computation in GCC-PHAT (T1)
   - Mitigation: Use XU316 VPU FFT instructions, optimize for 240-sample frames
2. **Second bottleneck:** MVDR weight calculation (T0)
   - Mitigation: Update weights less frequently (e.g., every 4th frame), use subspace tracking

## Anti-Patterns

### Anti-Pattern 1: Monolithic DOA+Beamforming Module

**What people do:** Combining DOA estimation and beamforming into one large module

**Why it's wrong:**
- Cannot test DOA independently
- Cannot swap beamforming algorithms without changing DOA
- Harder to understand and maintain

**Do this instead:** Separate lib_doa and lib_beamforming with clean interfaces

### Anti-Pattern 2: Direct ESP32 SPI Access from Pipeline Stages

**What people do:** Calling SPI directly from audio pipeline tasks

**Why it's wrong:**
- Blocks real-time audio processing
- Violates separation of data and control planes
- Breaks existing device_control framework

**Do this instead:** Use device_control servicer pattern with shared state

### Anti-Pattern 3: Hardcoded Mic Array Geometry

**What people do:** Hardcoding delays for 4-mic circular array in beamforming

**Why it's wrong:**
- Cannot adapt to different hardware configurations
- Cannot tune for manufacturing variations
- Makes testing difficult

**Do this instead:** Configurable geometry in module_config_t, runtime calculation of DTOA

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|----------|---------------------|---------|
| **lib_mic_array** | PDM capture on Tile 1 | 4-mic PDM driver, existing in codebase |
| **lib_xcore_math** | BFP, FFT operations | Provides BFP (Block Floating Point) for efficient DSP |
| **generic_pipeline** | Multi-stage orchestration | FreeRTOS-based pipeline framework |
| **device_control** | ESP32 SPI communication | Existing SPI transport layer |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|---------|
| **DOA ↔ DTOA** | Shared state (delays, direction) | DTOA can be called from DOA module |
| **DOA/DTOA (T1) ↔ Beamforming (T0)** | Intertile port 7 | Extended frame_data_t structure |
| **Beamforming ↔ IC/VNR/NS/AGC** | Direct function call (same tile) | Standard pipeline stage pattern |
| **DOA data ↔ ESP32** | Device Control + SPI | Servicer task on Tile 0 |

## Build Order Implications

| Module | Dependencies | Must Build After |
|---------|---------------|------------------|
| **lib_doa** | lib_xcore_math (BFP, FFT) | xmath library |
| **lib_dtoa** | lib_doa (optional) or lib_xcore_math | doa or xmath |
| **lib_beamforming** | lib_xcore_math, lib_dtoa (optional) | xmath and possibly dtoa |
| **mvdr_pipeline** | lib_doa, lib_beamforming, existing voice libs | doa, beamforming, aec, ic, ns, agc |

**Build sequence:**
1. Core libraries (xmath, mic_array)
2. DOA module (lib_doa)
3. DTOA module (lib_dtoa) - can be parallel with lib_doa if independent
4. Beamforming module (lib_beamforming)
5. MVDR pipeline (integrates all modules)

## Test Architecture

### Unit Testing (xsim)

Each module should have unit tests that can run in xsim without hardware:

```
tests/unit/
├── test_doa_gcc_phat.c         # Test GCC-PHAT with synthetic signals
├── test_dtoa.c                  # Test DTOA calculation
└── test_beamforming_mvdr.c        # Test MVDR with known inputs
```

### Synthetic Test Firmware

Create minimal firmware for algorithm validation:

```
tests/firmware/
├── test_doa.xe                # DOA-only firmware
├── test_beamform.xe             # Beamforming-only firmware
└── test_full.xe                # Full pipeline test firmware
```

**Test signals:** Generate synthetic audio with known DOA using:
- Time-delayed sinusoids
- Impulse responses
- Known geometry configurations

## Sources

- **XMOS Voice Library Pattern:** Analysis of `modules/voice/modules/lib_aec/api/aec_api.h`, `modules/voice/modules/lib_ic/api/ic_api.h`
- **XMOS Mic Array Library:** `modules/io/modules/mic_array/lib_mic_array/api/mic_array.h`
- **Generic Pipeline Framework:** `modules/rtos/modules/sw_services/generic_pipeline/api/generic_pipeline.h`
- **Device Control Framework:** `modules/fph/rtos_device_control/api/device_control.h`
- **Existing Pipeline Pattern:** `satellite-xmos-firmware/audio_pipelines/reference/adec/audio_pipeline_t0.c`, `audio_pipeline_t1.c`
- **GCC-PHAT Documentation:** [Generalized Cross Correlation Direction of Arrival Estimator](https://documentation.dspconcepts.com/awe-designer/8.D.2.6/generalized-cross-correlation-direction-of-arrival/)
- **XMOS MVDR Research:** [Motion Parameter Estimation of a Moving Acoustic Source](https://www.mdpi.com/1424-8220/22/1/1011) (Sensors, 2024)
- **XMOS Microphone Array Library:** [XMOS lib_mic_array Programming Guide](https://github.com/xmos/lib_mic_array)

---
*Architecture research for: XMOS XU316 DOA, DTOA, and Beamforming*
*Researched: 2026-02-15*
