# Roadmap: Satellite1 Multi-Mic Beamforming Firmware

## Overview

This roadmap delivers a modular, testable DOA (Direction of Arrival) module that enables 4-mic spatial localization for the Satellite1 voice assistant. The journey begins with establishing project scaffolding and testing infrastructure, then implements the core DOA module using SRP-PHAT algorithm for 3D spatial estimation, and finally integrates DOA data transmission to the ESP32 via SPI. Each phase delivers a complete, verifiable capability that builds on the previous one, enabling iterative development and validation against real hardware.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Foundation** - Establish project scaffolding, 4-mic capture, and testing infrastructure (completed 2026-02-16)
- [ ] **Phase 2: DOA Module** - Implement SRP-PHAT direction of arrival estimation with 3D localization
- [ ] **Phase 3: SPI Integration** - Transmit DOA data to ESP32 with backwards-compatible protocol

## Phase Details

### Phase 1: Foundation
**Goal**: Establish new firmware variant, enable 4-mic PDM capture, and create testing infrastructure for isolated module development
**Depends on**: Nothing (first phase)
**Requirements**: FND-01, FND-02, FND-03, FND-04, FND-05, FND-06
**Success Criteria** (what must be TRUE):
  1. New firmware variant `satellite1_firmware_beamformer` builds successfully with no errors
  2. Firmware captures audio from all 4 microphones (PDM channels 0-3 active)
  3. Synthetic test framework runs known audio scenarios and produces repeatable results
  4. xSIM simulation executes firmware to completion without hardware
  5. Module directory structure follows XMOS voice library patterns (api/, src/, tests/)
**Plans**: 4/4 complete

Plans:
- [x] 01-01: Clone beamformer variant from fixed_delay baseline
- [x] 01-02: Enable 4-mic PDM capture and verify audio output
- [x] 01-03: Create synthetic test framework and xSIM infrastructure
- [x] 01-04: Establish module directory structure

### Phase 2: DOA Module
**Goal**: Implement SRP-PHAT-based direction of arrival estimation for 3D spatial localization with noise source tracking
**Depends on**: Phase 1
**Requirements**: DOA-01, DOA-02, DOA-03, DOA-04, DOA-05, DOA-06, DOA-07
**Success Criteria** (what must be TRUE):
  1. DOA module outputs azimuth (0-360 degrees) relative to mic array for detected sound sources
  2. DOA estimates remain stable during speech and stop updating during silence periods (VAD integration)
  3. Elevation estimation provides approximate vertical angle for detected sound sources
  4. Constant noise sources (TV, devices) are identified and tracked separately from transient speech
  5. DOA module API is exposed via doa_init(), doa_process_frame(), doa_get_estimate() and all tests pass
**Plans**: TBD

Plans:
- [ ] 02-01: Implement SRP-PHAT DOA estimation with azimuth and elevation
- [ ] 02-02: Integrate VAD to prevent DOA updates during silence
- [ ] 02-03: Implement constant noise source tracking
- [ ] 02-04: Expose DOA module API and write comprehensive tests

### Phase 3: SPI Integration
**Goal**: Transmit DOA data (azimuth, elevation, noise sources) to ESP32 via SPI with backwards-compatible protocol
**Depends on**: Phase 2
**Requirements**: SPI-01, SPI-02, SPI-03
**Success Criteria** (what must be TRUE):
  1. DOA data (azimuth and elevation) is transmitted to ESP32 via existing SPI interface
  2. Existing SPI communication protocol remains functional for all non-DOA messages (backwards compatibility)
  3. Noise source direction metadata is output to ESP32 when persistent interference is detected
**Plans**: TBD

Plans:
- [ ] 03-01: Extend SPI protocol for DOA data transmission
- [ ] 03-02: Implement noise source metadata output
- [ ] 03-03: Test backwards compatibility and end-to-end DOA data flow

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation | 4/4 | Complete    | 2026-02-16 |
| 2. DOA Module | 0/4 | Not started | - |
| 3. SPI Integration | 0/3 | Not started | - |

**Overall Progress:** 4/11 plans complete (36%)
