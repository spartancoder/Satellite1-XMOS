# Requirements: Satellite1 Multi-Mic Beamforming Firmware

**Defined:** 2026-02-15
**Core Value:** Build a modular, testable audio pipeline that uses all 4 microphones for 3D spatial localization and beamforming to maximize voice assistant performance.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Foundation

- [ ] **FND-01**: New firmware variant `satellite1_firmware_beamformer` cloned from `satellite1_firmware_fixed_delay` with identical functionality
- [ ] **FND-02**: Enable 4-mic PDM capture (currently 2 mics) in beamformer variant
- [ ] **FND-03**: Build firmware with no errors and no new warnings beyond `fixed_delay` baseline
- [ ] **FND-04**: Synthetic test framework created for module isolation with known audio scenarios
- [ ] **FND-05**: xSIM testing infrastructure configured for full firmware simulation
- [ ] **FND-06**: Module directory structure established following XMOS voice library patterns (api/, src/, tests/)

### DOA Module

- [ ] **DOA-01**: SRP-PHAT algorithm estimates direction of arrival from 4-mic circular array
- [ ] **DOA-02**: Azimuth estimation provides 0-360° direction relative to mic array
- [ ] **DOA-03**: VAD integration prevents DOA updates during silence periods
- [ ] **DOA-04**: Elevation estimation via monaural cues provides approximate vertical angle
- [ ] **DOA-05**: Constant noise source tracking identifies and stores directions of persistent interference (TV, devices)
- [ ] **DOA-06**: DOA module API exposes doa_init(), doa_process_frame(), doa_get_estimate()
- [ ] **DOA-07**: DOA module tests pass (xSIM simulation and synthetic test firmware)

### SPI Integration

- [ ] **SPI-01**: DOA data (azimuth + elevation) transmitted to ESP32 via SPI interface
- [ ] **SPI-02**: SPI communication maintains backwards compatibility with existing protocol
- [ ] **SPI-03**: Noise source direction metadata output to ESP32 (if interference detected)

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### DTOA Module

- **DTOA-01**: GCC-PHAT algorithm estimates time difference of arrival for all mic pairs
- **DTOA-02**: DTOA module API exposes dtoa_init(), dtoa_process_frame(), dtoa_get_delays()
- **DTOA-03**: DTOA module tests pass (xSIM simulation and synthetic test firmware)

### Beamforming Module

- **BFM-01**: Fixed beamformer (delay-and-sum) combines 4-mic signals with steering delays
- **BFM-02**: Beamformer API exposes beamformer_init(), beamformer_set_direction(), beamformer_process_frame()
- **BFM-03**: Beamformer integrates into audio pipeline before AEC stage
- **BFM-04**: Beamformer tests pass (xSIM simulation and synthetic test firmware)
- **BFM-05**: Adaptive MVDR beamformer with WNG constraint for superior noise rejection
- **BFM-06**: Null steering capability suppresses known interference directions
- **BFM-07**: Subband processing mitigates spatial aliasing above 3.4kHz

### Post-Processing Module

- **PST-01**: Residual noise suppression (Wiener filter) after beamforming
- **PST-02**: Post-processing API exposes postproc_init(), postproc_process_frame()
- **PST-03**: Post-processing tests pass (xSIM simulation and synthetic test firmware)

### Advanced DOA Features

- **DOA-08**: Moving speaker tracking (Kalman filter) smooths DOA estimates
- **DOA-09**: Multi-source DOA detection identifies multiple simultaneous speakers

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| DTOA Module (v1) | DOA module provides needed direction estimation; DTOA is internal implementation detail for v1 |
| Beamforming Module (v1) | Focus on DOA module first; beamforming deferred to v2 for iterative development |
| Post-Processing Module (v1) | Defer until beamforming implemented; post-processing depends on beamformer output |
| Deep Learning DOA | Too computationally heavy for XMOS XU316 with 4 mics; SRP-PHAT is adequate |
| 8+ Microphone Array | Hardware redesign required; ESP32 SPI bandwidth limit; cost increase |
| Audio-Visual Fusion | Requires camera hardware; increases complexity significantly |
| Full DNN Beamforming | Requires significant compute; real-time constraints on embedded platform |
| Null Steering (v1) | Requires adaptive beamformer (deferred to v2); noise tracking only in v1 as metadata |
| 3D Beamforming (Z-axis) | 4-mic circular array is coplanar; no Z-axis information available |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| FND-01 | Phase 1 | Complete |
| FND-02 | Phase 1 | Complete |
| FND-03 | Phase 1 | Complete |
| FND-04 | Phase 1 | Complete |
| FND-05 | Phase 1 | Complete |
| FND-06 | Phase 1 | Complete |
| DOA-01 | Phase 2 | Pending |
| DOA-02 | Phase 2 | Pending |
| DOA-03 | Phase 2 | Pending |
| DOA-04 | Phase 2 | Pending |
| DOA-05 | Phase 2 | Pending |
| DOA-06 | Phase 2 | Pending |
| DOA-07 | Phase 2 | Pending |
| SPI-01 | Phase 3 | Pending |
| SPI-02 | Phase 3 | Pending |
| SPI-03 | Phase 3 | Pending |

**Coverage:**
- v1 requirements: 18 total
- Mapped to phases: 18
- Unmapped: 0 ✓
- Completed: 6/18 (33%)

---
*Requirements defined: 2026-02-15*
*Last updated: 2026-02-16 after Phase 1 completion*
