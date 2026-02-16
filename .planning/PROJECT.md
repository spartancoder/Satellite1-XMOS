# Satellite1 Multi-Mic Beamforming Firmware

## What This Is

XMOS XCORE.AI firmware for Satellite1 device that implements advanced audio processing with 4-mic array. The firmware adds DOA (Direction of Arrival), DTOA (Difference Time of Arrival), beamforming, and post-processing modules to maximize the device's effectiveness as a voice assistant. Processing leverages the XU316 processor's VPU capabilities for real-time performance.

## Core Value

Build a modular, testable audio pipeline that uses all 4 microphones for 3D spatial localization and beamforming to maximize voice assistant performance.

## Requirements

### Validated

- ✓ Multi-tile XMOS XU316 firmware with dual-tile architecture — existing codebase
- ✓ Configurable audio pipeline (AEC, VNR, NS, AGC, IC) with compile-time enable/disable — existing codebase
- ✓ FreeRTOS-based task scheduling and inter-tile communication — existing codebase
- ✓ Device control via SPI to ESP32-S3 companion processor — existing codebase
- ✓ 2-mic PDM capture from circular array (90° separation, 3.55mm radius) — existing codebase

### Active

- [ ] New firmware variant `satellite1_firmware_beamformer` cloned from `satellite1_firmware_fixed_delay` with identical functionality
- [ ] Enable all 4 microphones in beamformer variant
- [ ] Maintain existing SPI communication backwards compatibility while adding interfaces for DOA data
- [ ] DOA module with API, source, and tests (3D spatial localization: azimuth, elevation, distance)
- [ ] Integrate DOA into audio pipeline
- [ ] Pass DOA data (direction, altitude) to ESP32 via SPI
- [ ] DTOA module with API, source, and tests
- [ ] Beamforming module with API, source, and tests
- [ ] Post-processing module with API, source, and tests
- [ ] All modules structured for extraction to separate git repos (portable)
- [ ] Testing via `xsim` (full firmware simulation)
- [ ] Synthetic test firmwares for isolated device testing

### Out of Scope

- [ ] Mobile app or external interface beyond ESP32 SPI — ESP32 handles higher-level communication
- [ ] Video processing or camera integration — audio-only device
- [ ] Cloud-based speech recognition — output audio to ESP32 for downstream processing

## Context

**Hardware Configuration:**
- Circular 4-mic array with 90° separation, 3.55mm radius
- Current firmware uses only 2 microphones
- Spatial aliasing upper limit: ~3.4kHz for adjacent mic pairs
- Lower frequency limit: TBD based on physical array constraints and human speech characteristics

**Development Approach:**
- Modules designed for extraction to separate git repos (portable to other platforms)
- Module structure: API definition, source code, tests
- Two-tier testing: `xsim` for full firmware simulation, synthetic test firmwares for isolated device testing
- Real-time processing requirements drive use of XU316 VPU capabilities

**Iterative Development:**
- Milestone structure is directional but evolves based on experimental results
- Testing required in both synthetic and real-world scenarios
- Voice assistant use case guides feature prioritization

**Frequency Constraints:**
- Target frequency range for voice: TBD (typically 300Hz-4kHz for speech)
- Spatial aliasing blind zone above ~3.4kHz
- Research required to determine lower frequency limit based on array geometry

## Constraints

- **Real-time processing**: Must maintain real-time audio pipeline performance — XMOS XU316 VPU leveraged for acceleration
- **Backwards compatibility**: Preserve existing SPI communication protocol — extend or add new interfaces without breaking ESP32 integration
- **Module portability**: Design modules for extraction to separate git repos — clean API boundaries, minimal external dependencies
- **Testing infrastructure**: Support `xsim` simulation and synthetic test firmware workflows — both required for comprehensive testing
- **Frequency limits**: Work within spatial aliasing constraints (~3.4kHz upper) and array geometry limits (lower TBD)
- **Multi-tile coordination**: Maintain dual-tile architecture (Tile 0: control/output, Tile 1: capture/DSP) — leverage existing inter-tile communication patterns

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Clone from `fixed_delay` variant | Baseline working firmware with existing pipeline | — Pending |
| New variant named `satellite1_firmware_beamformer` | Allows multiple beamformer iterations | — Pending |
| Baseline warnings from `fixed_delay` build | Prevent regression, enforce quality | — Pending |
| Module structure includes API, source, tests | Enables portability and isolated testing | — Pending |
| Two-tier testing (xsim + synthetic) | Comprehensive validation at firmware and module levels | — Pending |

---
*Last updated: 2026-02-15 after initialization*
