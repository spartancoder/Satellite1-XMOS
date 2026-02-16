# Project Research Summary

**Project:** Satellite1 Multi-Mic Beamforming Firmware
**Domain:** Embedded Voice Assistant Audio Processing with 3D Spatial Localization
**Researched:** 2026-02-15
**Confidence:** MEDIUM

## Executive Summary

This project implements Direction of Arrival (DOA), Differential Time of Arrival (DTOA), and beamforming for a 4-mic circular array on XMOS XU316 firmware. Expert implementations use GCC-PHAT for TDOA estimation, MVDR beamforming for spatial filtering, and careful handling of spatial aliasing constraints (3.4kHz limit for this array geometry). The recommended approach is modular: separate lib_doa, lib_beamforming, and lib_dtoa modules following XMOS voice library patterns, integrated via the generic_pipeline framework with dual-tile distribution (Tile 1: PDM capture + DOA/DTOA, Tile 0: beamforming + existing processing).

Key risks are significant but manageable: uncalibrated microphone mismatches cause random performance degradation, spatial aliasing above 3.4kHz creates ambiguous DOA estimates, and real-time processing constraints (15ms frame deadline at 16kHz) require careful optimization. Mitigation strategies include per-mic calibration at startup, explicit frequency band limiting (DC-3.4kHz for directional accuracy), and synthetic testing via xSIM before hardware validation. The 4-mic coplanar array cannot reliably estimate elevation—this must be acknowledged as a hardware limitation rather than an algorithmic problem.

## Key Findings

### Recommended Stack

The XMOS ecosystem provides mature libraries optimized for XS3 architecture. XMOS lib_mic_array (v6.0.0) handles PDM capture with phase-aligned synchronization required for TDOA. XMOS lib_xcore_math (v2.4.0) leverages the Vector Processing Unit (VPU) for FFT, Block Floating Point (BFP), and matrix operations—critical for efficient GCC-PHAT and MVDR implementation. XMOS lib_audio_dsp (v1.4.0) provides common audio DSP stages for post-processing. For algorithm reference, the robin1001/beamforming repository offers C implementations of MVDR, GCC-PHAT, and delay-sum beamforming with MATLAB verification.

Testing infrastructure relies on Python tools: pyroomacoustics for synthetic room simulation, scipy.signal for reference implementations, and pytest for parametrized testing across azimuth/elevation/distance combinations. On-device testing uses Unity framework (pattern established in fwk_voice) and xSIM for cycle-accurate simulation without hardware.

**Core technologies:**
- **XMOS lib_mic_array v6.0.0**: PDM capture and decimation — phase-aligned multi-mic capture required for TDOA estimation
- **XMOS lib_xcore_math v2.4.0**: DSP primitives (FFT, BFP, matrix ops) — VPU-accelerated operations for efficient real-time processing
- **GCC-PHAT algorithm**: Time Difference of Arrival estimation — robust to reverberation, industry standard for single-source DOA
- **MVDR beamforming**: Minimum Variance Distortionless Response — optimal beamformer with superior noise rejection vs delay-sum

### Expected Features

Voice assistant users expect spatial awareness for beam steering, noise suppression, and responsive interaction. Table stakes include DOA estimation (azimuth only), DTOA computation, basic beamforming (delay-and-sum), real-time operation (<30ms latency), and integration with existing AEC/VNR/NS/AGC pipeline. Differentiators include 3D DOA with elevation, adaptive MVDR beamforming, moving speaker tracking, and post-beamforming enhancement for residual noise. Features to defer to v2+ include multi-talker detection, distance estimation, frequency-dependent beamforming (to address spatial aliasing), and deep learning approaches (too computationally heavy for XMOS XU316).

**Must have (table stakes):**
- **DTOA Module** — Foundation for all spatial processing; required input for DOA
- **DOA Estimation (Azimuth)** — Enables beam steering; GCC-PHAT provides 2-4 degree accuracy
- **Fixed Beamforming (Delay-and-Sum)** — Basic spatial focusing; computationally efficient
- **VAD Integration** — Required for stable DOA; prevents updates during silence
- **SPI Location Output** — Pass DOA to ESP32; enables downstream applications

**Should have (competitive):**
- **Adaptive Beamforming (MVDR)** — Superior noise rejection; addresses performance gaps
- **Moving Speaker Tracking** — Smoother beam following; better UX
- **Post-Beamforming Enhancement** — Additional speech quality boost

**Defer (v2+):**
- **3D DOA (Elevation)** — 4-mic coplanar array cannot reliably estimate elevation
- **Multi-Talker Detection** — Significant complexity increase; advanced feature
- **Distance Estimation** — Requires multi-frequency analysis; challenging with small array

### Architecture Approach

Follow XMOS voice library patterns for modular, testable components. Major components: lib_doa (GCC-PHAT TDOA estimation to angle conversion), lib_beamforming (MVDR and delay-sum beamforming with steerable weights), lib_dtoa (differential time delays between mic pairs), mvdr_pipeline (dual-tile audio pipeline integrating new modules with existing AEC/VNR/NS/AGC), and doa_servicer (device control framework for ESP32 SPI communication). Tile assignment: Tile 1 handles PDM capture, DOA estimation, and DTOA (needs raw mic data), Tile 0 handles beamforming, IC/VNR/NS/AGC, and I2S output (more CPU available). Inter-tile communication via port 7 passes DOA results and DTOA delays; extended frame_data_t structure carries direction estimates.

**Major components:**
1. **lib_doa** — GCC-PHAT TDOA estimation, geometry calculations, angle output with confidence scoring
2. **lib_beamforming** — MVDR adaptive beamformer with WNG constraint, delay-sum fallback, steerable weights
3. **lib_dtoa** — Cross-correlation delays between mic pairs, sub-sample interpolation
4. **mvdr_pipeline** — Dual-tile generic_pipeline orchestration, module integration, inter-tile coordination
5. **doa_servicer** — ESP32 SPI communication, DOA data protocol, control plane interface

### Critical Pitfalls

Uncalibrated microphone mismatches cause random performance fluctuation and beam pattern distortion—implement per-mic gain/phase calibration at startup and store coefficients. Spatial aliasing above 3.4kHz for this array geometry creates ambiguous DOA estimates and beam side-lobes—explicitly limit operational range to DC-3.4kHz for directional accuracy. White noise amplification in MVDR beamformers at low frequencies produces audible hiss—use WNG-constrained beamformers (diagonally loaded MVDR) and fall back to delay-sum when WNG too low. Real-time deadline violations (15ms frame at 16kHz) cause audio glitches—profile cycle counts, reserve 20-30% headroom, use coarse-to-fine DOA search. Module boundary violations impede extraction—define clear interfaces, use opaque handles, minimize shared state.

1. **Uncalibrated Microphone Mismatch** — Implement per-mic gain/phase calibration at startup; store coefficients and provide recalibration mode
2. **Spatial Aliasing Above 3.4kHz** — Explicitly limit operational frequency range to DC-3.4kHz; document as design constraint
3. **White Noise Amplification (MVDR)** — Use WNG-constrained beamformers; monitor WNG and fall back to delay-sum when too low
4. **Real-Time Deadline Violations** — Profile cycle counts on target hardware; reserve 20-30% headroom; use coarse-to-fine DOA search
5. **Elevation Estimation with Coplanar Array** — Acknowledge limitation; output azimuth only; mark elevation as unsupported

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Test Infrastructure and Module Foundations
**Rationale:** Synthetic testing infrastructure must exist before module development to catch algorithm bugs early and avoid hardware-dependent debugging. Module boundaries defined now prevent expensive refactoring later.
**Delivers:** xSIM-based synthetic test framework, pytest-based algorithm validation tools, module API contracts, fixed-point numerical design decisions
**Addresses:** DTOA Module foundation, DOA Module foundation, test infrastructure
**Avoids:** Insufficient synthetic test coverage, module boundary violations, fixed-point overflow

### Phase 2: DOA and DTOA Module Implementation
**Rationale:** DOA and DTOA are foundational for all spatial processing. Implementing them first enables beamforming to be developed with real direction estimates rather than synthetic inputs.
**Delivers:** lib_doa with GCC-PHAT implementation, lib_dtoa with cross-correlation delays, synthetic tests validating time delay accuracy (< 15 degrees DOA error), reverberation handling
**Uses:** lib_xcore_math (FFT, BFP), lib_mic_array (PDM capture), pyroomacoustics (synthetic test generation)
**Implements:** DOA estimation (azimuth only, with confidence scoring), DTOA calculation, multipath rejection
**Avoids:** Reverberation degradation, front-back ambiguity issues

### Phase 3: Beamforming Module and Pipeline Integration
**Rationale:** Beamforming requires DOA estimates as input and must integrate with existing pipeline. MVDR with WNG constraint prevents white noise amplification.
**Delivers:** lib_beamforming with MVDR and delay-sum, WNG monitoring and fallback, mvdr_pipeline dual-tile integration, inter-tile communication for DOA/DTOA data
**Uses:** lib_doa (direction estimates), lib_dtoa (delays), generic_pipeline framework, device_control (ESP32 SPI)
**Implements:** MVDR adaptive beamforming, delay-sum fallback, beamformer weight calculation
**Avoids:** White noise amplification, pipeline order dependency issues (AEC-beamforming interaction)

### Phase 4: Post-Processing and Enhancement
**Rationale:** Post-beamforming enhancement adds residual noise suppression to complement spatial filtering. Moving speaker tracking improves UX by smoothing DOA output.
**Delivers:** Post-processing module (Wiener filter), moving speaker tracking (Kalman filter), real-world validation on hardware
**Implements:** Post-beamforming enhancement, DOA tracking with velocity constraints, null steering (if time permits)
**Avoids:** "Ghost" DOA detections, sudden beam direction changes

### Phase Ordering Rationale

- Synthetic testing infrastructure first to catch algorithm bugs before hardware integration (prevents expensive refactoring)
- DOA/DTOA before beamforming because beamforming requires direction estimates as input
- Beamforming before post-processing because enhancement modules need beamformed audio input
- Post-processing last because it's optional enhancement, not required for core functionality
- Dual-tile pipeline integration in Phase 3 to avoid module boundary violations and enable parallel testing

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 3 (Beamforming Module):** MVDR implementation details (covariance matrix updates, diagonal loading parameters), WNG constraint tuning—sparse practical examples in XMOS context
- **Phase 4 (Post-Processing):** Moving speaker tracking algorithm selection (Kalman vs particle filter), null steering interference detection—specialized domain knowledge required

Phases with standard patterns (skip research-phase):
- **Phase 1 (Test Infrastructure):** Well-documented pytest patterns, xSIM simulation follows XMOS examples
- **Phase 2 (DOA/DTOA):** GCC-PHAT is industry standard with numerous implementations; lib_mic_array and lib_xcore_math APIs are well-documented

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | XMOS official documentation (HIGH confidence) for core libraries; algorithm references well-established |
| Features | MEDIUM | Table stakes and MVP definition clear; differentiators require validation in real-world testing |
| Architecture | HIGH | XMOS voice library patterns documented in codebase; dual-tile architecture proven in existing firmware |
| Pitfalls | MEDIUM | Academic sources validated aliasing and white noise issues; XMOS-specific integration patterns inferred |

**Overall confidence:** MEDIUM

Core stack choices and architecture patterns are well-supported by official XMOS documentation and established voice library examples. Feature prioritization based on competitor analysis and voice assistant requirements is sound. Pitfalls research identified critical issues (spatial aliasing, calibration, WNG) with high-quality sources. Medium confidence overall because some integration details (MVDR tuning on XMOS, post-processing specific algorithms) require experimental validation. Lower frequency limit (~500Hz) identified but needs verification during implementation.

### Gaps to Address

- **Lower frequency limit determination:** Research identifies ~500Hz as practical lower bound due to SNR and wavelength constraints, but requires validation during implementation with actual hardware
- **MVDR WNG constraint tuning parameters:** Diagonal loading amounts and WNG thresholds need experimental determination during Phase 3
- **Reverberation handling effectiveness:** SRP-PHAT suggested as alternative to GCC-PHAT for reverberant scenarios, but requires comparative testing
- **Post-processing algorithm selection:** Wiener filter vs DNN-based post-filtering needs evaluation based on performance vs complexity tradeoff
- **ESP32 SPI protocol extension:** Backwards-compatible protocol for DOA data transmission needs design during Phase 3

## Sources

### Primary (HIGH confidence)
- XMOS lib_mic_array v6.0.0 — PDM capture library, XS3-optimized, phase-aligned multi-mic capture
- XMOS lib_xcore_math v2.4.0 — VPU-accelerated math library, FFT/BFP operations
- XMOS lib_audio_dsp v1.4.0 — Audio DSP library, pipeline stages
- XMOS XTC Tools 15.3.1 — Build toolchain, xSIM simulator
- XMOS XCORE-VOICE Solution Programming Guide v2.3.1 — Voice processing framework patterns
- XMOS Mic Array Programming Guide — Microphone array implementation
- XMOS Voice Library Pattern Analysis — `modules/voice/modules/lib_aec/api/aec_api.h`, `modules/voice/modules/lib_ic/api/ic_api.h` (existing codebase)

### Secondary (MEDIUM confidence)
- robin1001/beamforming (GitHub) — C/C++ implementations of MVDR, GCC-PHAT, delay-sum with MATLAB verification
- pyroomacoustics — Python room simulation for synthetic testing
- "Localization using Angle-of-Arrival Triangulation" (arXiv, 2025) — GCC-PHAT fundamentals and comparison
- "A Reduced Complexity Acoustic-Based 3D DoA Estimation" (MDPI Sensors, 2024) — GCC-PHAT interpolation techniques
- ODAS: Open embeddeD Audition System — GCC-PHAT implementation strategies, coarse-to-fine search
- "Robust Three-Microphone Speech Source Localization" (PMC) — DOA estimation fundamentals

### Tertiary (LOW confidence)
- 2025 research papers on end-to-end DOA-guided speech extraction — Recent academic work, needs validation
- "High-Resolution DOA Estimation of UAVs" (MDPI, 2025) — Dynamic simulation environment, not directly applicable
- "3D Multiple Sound Source Localization" (MDPI, 2022) — Circular array techniques, but older reference
- ReSpeaker XVF3800 product documentation — Commercial product references, implementation details not fully disclosed

---
*Research completed: 2026-02-15*
*Ready for roadmap: yes*
