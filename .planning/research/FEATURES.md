# Feature Research: 3D Audio DOA, DTOA, and Beamforming for Voice Assistant

**Domain:** Embedded Voice Assistant Audio Processing
**Researched:** 2025-02-15
**Confidence:** MEDIUM

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete as a voice assistant.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **DOA Estimation (Azimuth)** | Enables beam steering toward speaker; all modern voice assistants locate sound sources | LOW | GCC-PHAT is standard; 2-4 degree accuracy achievable with 4-mic circular array |
| **DTOA (Time Difference of Arrival)** | Required input for DOA; fundamental to microphone array processing | LOW | GCC-PHAT algorithm is industry standard; robust to reverberation |
| **Fixed Beamforming (Delay-and-Sum)** | Basic spatial filtering to focus on user direction; baseline for far-field performance | LOW | Simple to implement; minimal processing overhead |
| **Real-time Operation (< 30ms latency)** | Voice assistants require low latency for responsive interaction | MEDIUM | XMOS XU316 capable; must fit in frame budget (16kHz = ~62.5ms per frame) |
| **Noise Suppression** | Essential for far-field operation in noisy homes; users expect clean voice pickup | MEDIUM | Already exists (NS module); post-filtering after beamforming improves performance |
| **AEC (Acoustic Echo Cancellation)** | Required when device has speakers; prevents device from hearing its own output | MEDIUM | Already exists in pipeline; must work with beamforming |
| **Voice Activity Detection (VAD)** | Determines when speech is present for beamformer adaptation and DOA updates | LOW | Required for DOA stability and power management |
| **SPI Location Output** | Needed to pass DOA data to ESP32 for downstream applications | LOW | Simple data protocol; must be real-time |
| **16kHz Sample Rate** | Industry standard for voice processing; matches ASR input requirements | LOW | Already configured |
| **TDM I2S Output** | Pass multi-channel audio to ESP32 for processing and streaming | LOW | Already exists; 48kHz TDM supports 6 channels |

### Differentiators (Competitive Advantage)

Features that set the product apart. Not required, but valuable.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **3D DOA (Azimuth + Elevation)** | Full spatial awareness; competitive advantage over 2D-only solutions; enables better tracking | MEDIUM | With 4-mic coplanar array, elevation requires alternative techniques (intensity, phase) |
| **Distance Estimation** | User proximity awareness; enables adaptive gain and privacy features | HIGH | Requires intensity or TDOA-based ranging; challenging with small array |
| **Adaptive Beamforming (MVDR)** | Superior noise rejection vs fixed beamforming; better performance in challenging environments | MEDIUM | Requires noise covariance estimation; computationally more intensive |
| **Null Steering** | Explicitly suppresses known interference sources (TV, other speakers); improves SNR | MEDIUM | GSC beamformer architecture; requires interference direction estimation |
| **Moving Speaker Tracking** | Smoothly follows user; better user experience for mobile interactions | HIGH | Requires filtering/prediction; Kalman filter or particle filter typical |
| **Multi-Talker Detection** | Identify multiple speakers; enables speaker-specific processing | HIGH | Significantly increases complexity; DNN-based approaches common |
| **Reverberation Handling** | Maintain performance in reflective rooms (high RT60); common in homes | MEDIUM | Dereverberation techniques; early reflection cancellation |
| **Frequency-Dependent Beamforming** | Wideband performance; addresses spatial aliasing at high frequencies | MEDIUM | Subband processing required; mitigates ~3.4kHz aliasing limit |
| **Post-Beamforming Enhancement** | Additional speech quality boost after spatial filtering; complements beamforming | MEDIUM | Wiener filter or DNN-based post-filtering |
| **Low-Power Wake Integration** | Enable always-on wake word detection with beamforming assistance | MEDIUM | VAD must be extremely low power; wake word integration point critical |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems for our hardware/constraints.

| Anti-Feature | Why Requested | Why Problematic | Alternative |
|--------------|---------------|-----------------|-------------|
| **Deep Learning DOA** | Promises higher accuracy | Too computationally heavy for XMOS XU316 with 4 mics | Use GCC-PHAT; proven adequate for voice assistant use case |
| **8+ Microphone Array** | Better spatial resolution | Hardware redesign required; ESP32 SPI bandwidth limit; cost increase | Use virtual microphone techniques; post-filtering enhancement |
| **Audio-Visual Fusion** | Camera + audio for better tracking | Requires camera hardware; increases complexity significantly | Audio-only tracking adequate for most voice assistant use cases |
| **Full DNN Beamforming** | State-of-the-art performance | Requires significant compute; real-time constraints on embedded platform | MVDR + post-filter; hybrid approaches |
| **3D Beamforming (Z-axis)** | True volumetric audio capture | 4-mic circular array is coplanar; no Z-axis information | Focus on azimuth + elevation estimation; accept 2.5D limitations |
| **Ultra-Wideband Audio (> 16kHz)** | Higher audio quality | Increases processing load; voice content primarily in speech band; ASR optimized for 16kHz | 16kHz is sufficient for voice; focus on clarity not bandwidth |
| **Multiple Simultaneous Beam Outputs** | Support multiple users | Processing multiplies per output; limited MIPS budget | Single beam + multi-talker detection; defer multi-beam to future |

---

## Feature Dependencies

```
[DTOA Module]
    └──requires──> [Raw Microphone Data] (4-channel PDM)

[DOA Module]
    └──requires──> [DTOA Module] (TDOA estimates)
    └──requires──> [Array Geometry] (mic positions, radius, angles)

[Fixed Beamformer (Delay-and-Sum)]
    └──requires──> [DOA Module] (steering direction)
    └──requires──> [Raw Microphone Data]

[Adaptive Beamformer (MVDR)]
    └──requires──> [DOA Module] (steering direction)
    └──requires──> [Noise Estimation] (covariance matrix)
    └──requires──> [Raw Microphone Data]
    └──enhances──> [Fixed Beamformer]

[Null Steering]
    └──requires──> [DOA Module] (interference direction)
    └──requires──> [Adaptive Beamformer] (GSC architecture)

[Post-Beamforming Enhancement]
    └──requires──> [Any Beamformer] (beamformer output)
    └──enhances──> [All Beamformers]

[3D DOA (Elevation)]
    └──requires──> [DOA Module] (azimuth estimate)
    └──requires──> [Intensity/Ranging] (elevation from monaural cues)

[Distance Estimation]
    └──requires──> [DTOA Module] (time delays)
    └──requires──> [Intensity Analysis] (signal level decay)

[Moving Speaker Tracking]
    └──requires──> [DOA Module] (raw estimates)
    └──requires──> [VAD] (speech presence)
    └──enhances──> [DOA Module] (smoothed output)

[SPI Location Output]
    └──requires──> [DOA Module] (estimates)
    └──requires──> [Distance Estimation] (if 3D enabled)
```

### Dependency Notes

- **DTOA requires Raw Microphone Data:** TDOA estimation needs synchronous samples from all 4 PDM microphones; cannot work with already-beamed audio
- **DOA requires DTOA:** All DOA algorithms (GCC-PHAT-based) fundamentally operate on time difference estimates; cannot skip TDOA step
- **Adaptive Beamformer enhances Fixed Beamformer:** MVDR provides superior noise rejection but adds complexity; can layer on top of fixed beamformer infrastructure
- **3D DOA requires alternative techniques:** 4-mic coplanar array cannot directly measure elevation; requires monaural cues (intensity, phase) or approximation
- **Moving Speaker Tracking enhances DOA:** Raw DOA estimates are noisy; tracking provides smooth, stable output; critical for good user experience
- **Post-Beamforming Enhancement complements all beamformers:** Adds residual noise suppression after spatial filtering; independent of beamformer type

---

## MVP Definition

### Launch With (v1)

Minimum viable product — what's needed to validate the concept and function as a voice assistant.

- [ ] **DTOA Module** — Foundation for all spatial processing; GCC-PHAT algorithm for robust TDOA estimation between mic pairs
- [ ] **DOA Module (Azimuth Only)** — Enables beam steering; GCC-PHAT to DOA conversion using circular array geometry
- [ ] **Fixed Beamformer (Delay-and-Sum)** — Basic spatial focusing; computationally efficient; adequate for initial use case
- [ ] **VAD Integration** — Required for stable DOA; prevents updates during silence
- [ ] **SPI Location Output** — Pass DOA (azimuth) to ESP32; enables downstream applications
- [ ] **Integration with Existing Pipeline** — Fit before AEC/VNR/NS/AGC stages; use as pre-processing step
- [ ] **Real-time Operation** — Fit within frame budget; <30ms end-to-end latency

### Add After Validation (v1.x)

Features to add once core is working and validated.

- [ ] **Adaptive Beamforming (MVDR)** — Superior noise rejection; addresses performance gaps found in v1 testing
- [ ] **Moving Speaker Tracking** — Improves user experience; smoother beam following
- [ ] **Post-Beamforming Enhancement** — Additional speech quality boost; addresses residual noise issues
- [ ] **3D DOA (Elevation Estimation)** — Competitive differentiation; adds spatial awareness
- [ ] **Null Steering** — Explicit interference suppression; addresses specific user scenarios (TV, kitchen)

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] **Multi-Talker Detection** — Advanced feature; significant complexity increase
- [ ] **Distance Estimation** — Proximity awareness; privacy implications
- [ ] **Frequency-Dependent Beamforming** — Wideband optimization; address spatial aliasing
- [ ] **Deep Learning Integration** — If XMOS resources permit; consider for specific sub-modules

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| DTOA Module | HIGH | LOW | P1 |
| DOA (Azimuth) | HIGH | LOW | P1 |
| Fixed Beamformer | HIGH | LOW | P1 |
| VAD Integration | HIGH | LOW | P1 |
| SPI Location Output | HIGH | LOW | P1 |
| Moving Speaker Tracking | MEDIUM | MEDIUM | P2 |
| Adaptive Beamforming (MVDR) | HIGH | MEDIUM | P2 |
| Post-Beamforming Enhancement | MEDIUM | MEDIUM | P2 |
| 3D DOA (Elevation) | LOW | MEDIUM | P3 |
| Null Steering | LOW | MEDIUM | P3 |
| Distance Estimation | LOW | HIGH | P3 |
| Multi-Talker Detection | LOW | HIGH | P3 |
| Deep Learning DOA | LOW | HIGH | P3 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Competitor Feature Analysis

| Feature | Amazon Echo (7 mics) | Google Home (2 mics) | Our Approach (4 mics) |
|---------|---------------------|----------------------|----------------------|
| Microphone Array | 7 mics circular | 2 mics | 4 mics circular |
| DOA Estimation | Yes (azimuth) | Yes (azimuth) | Yes (azimuth, optionally elevation) |
| Beamforming | Adaptive (MVDR) | Beamforming | Start with fixed, add adaptive (MVDR) |
| Noise Suppression | Multi-stage | Deep learning | Existing NS + post-beamforming enhancement |
| AEC | Yes | Yes | Already exists in pipeline |
| Multi-Talker | Yes | Limited | Future consideration |
| Spatial Aliasing Mitigation | Virtual mics | 2 mics = limited | Post-filtering, frequency-dependent |
| Real-time Latency | <20ms | <30ms | Target <30ms |

**Notes:**
- Amazon Echo's 7-mic array provides better spatial resolution but at higher cost
- Our 4-mic array is a middle ground; virtual microphone techniques can partially compensate
- Google Home's 2-mic approach shows that even minimal arrays can work with good algorithms
- Our differentiators: 3D DOA (elevation), integration flexibility with ESP32, lower cost than 7-mic solutions

---

## Hardware-Specific Considerations

### Spatial Aliasing Constraint
With 4 mics at 90° separation and 3.55mm radius:
- Spatial aliasing frequency: ~3.4kHz (where mic spacing > wavelength/2)
- Impact: DOA estimation unreliable above this frequency
- Mitigation: Frequency-dependent beamforming, post-filtering, accept limitation (voice content mainly < 3kHz)

### XMOS XU316 Constraints
- Limited MIPS budget for real-time processing
- 16kHz sample rate = 62.5ms frame time
- Target: <30ms end-to-end latency
- Tradeoff: Algorithm complexity vs real-time performance

### 4-Mic Circular Array Limitations
- Coplanar array cannot directly measure elevation
- Limited spatial resolution vs 6-8 mic arrays
- Mitigation: Virtual microphone techniques, post-filtering, monaural cues for elevation

### ESP32 Integration
- SPI bandwidth limits for multi-channel data
- Real-time requirements for location output
- Design: Send DOA estimates, not raw TDOA data

---

## Sources

### DOA and DTOA Algorithms
- [Localization using Angle-of-Arrival Triangulation (arXiv, 2025)](https://arxiv.org/html/2508.16908v1) — AoA estimation comparison: GCC-PHAT, GCC+, MUSIC
- [A Reduced Complexity Acoustic-Based 3D DoA Estimation (MDPI Sensors, 2024)](https://www.mdpi.com/1424-8220/24/7/2344) — GCC-PHAT interpolation for improved TDOA accuracy
- [Accuracy comparison of four AoA techniques (ResearchGate)](https://www.researchgate.net/figure/Accuracy-comparison-of-four-AoA-techniques-IAC-GCC-PHAT-MUSIC-and-Delay-and-Sum_fig4_338828367) — GCC-PHAT robust to reverberation
- [Time-Delay of Arrival (TDoA) and Direction of Arrival (DoA) - Speech Processing Book](https://speechprocessingbook.aalto.fi/Enhancement/tdoa.html) — GCC and GCC-PHAT fundamentals
- [Learning Multi-Target TDOA Features for Sound Event Localization (arXiv, 2024)](https://arxiv.org/html/2408.17166v1) — TDOA to DOA mapping with array geometry

### Beamforming
- [Beamforming for Direction-of-Arrival (DOA) Estimation - Survey (ResearchGate, 2025)](https://www.researchgate.net/publication/Beamforming-for-Direction-of-Arrival-DOA-Estimation-Survey) — Spatial filtering techniques
- [Acoustic beamforming with circular microphone arrays (AIP JASA, 2024)](https://pubs.aip.org/asa/jasa/article/156/1/405/3303425/A-circular-microphone-array-with-virtual) — Circular array beamforming
- [Loudspeaker Beamforming to Enhance Speech Recognition in VDA Systems (arXiv, 2025)](https://arxiv.org/abs/2501.06087) — Beamforming for voice assistants
- [Speech Enhancement Algorithm Based on Microphone Array (MDPI Electronics, 2024)](https://www.mdpi.com/2079-9292/13/22/5188) — Beamforming + post-filtering architecture
- [MVDR Beamformer on Raspberry Pi (GitHub)](https://github.com/hamidreza-km/MVDR-Beamformer) — Lightweight real-time MVDR implementation

### Post-Filtering and Enhancement
- [Speech Enhancement Based on Beamforming and Post-Filtering (Interspeech 2020)](https://www.isca-archive.org/interspeech_2020/cheng20b_interspeech.pdf) — Beamforming + DNN post-filtering
- [An appropriate post-filtering for GSC beamformer (CEUR-WS.org)](https://ceur-ws.org/Vol-4057/paper15.pdf) — GSC beamformer post-filtering
- [Explainable DNN-based Beamformer with Postfilter (arXiv, 2024)](https://arxiv.org/html/2411.10854v1) — Attention-based beamforming
- [Postfilter for Dual Channel Speech Enhancement Using Coherence (MDPI, 2024)](https://www.mdpi.com/1424-8220/24/12/3979) — Multichannel speech enhancement

### Voice Assistant Requirements
- [Recommendations and Considerations for Far-Field Voice Applications (XMOS Whitepaper)](https://www.xmos.com/file/recommendations-and-considerations-for-testing-and-optimising-far-field-voice-applications-whitepaper?version=latest) — XMOS 4-mic reference design
- [Building voice assistants for home audio (Qualcomm, 2020)](https://www.qualcomm.com/news/onq/2020/08/guest-post-dsp-concepts-building-voice-assistants-home-audio) — Microphone spacing and beamforming
- [How do voice assistants handle noise and speech interruptions? (Tencent Cloud, 2025)](https://www.tencentcloud.com/learn/1040181) — Noise filtering algorithms
- [When to Choose a DSP for Processing Voice Commands (Texas Instruments)](https://www.ti.com/lit/an/slaa907b/slaa907b.pdf) — 4-8 mic recommendation for beamforming
- [ETSI TS 103 504 (2020)](https://www.etsi.org/deliver/etsi_ts/103500_103599/103504/01.01.01_60/ts_103504v010101p.pdf) — Voice assistant testing standards

### 3D DOA and Tracking
- [Real-Time 3D Sound Localization with Microphone Arrays (ECE Journals, 2025)](https://ecejournals.onlinelibrary.wiley.com/doi/10.1002/eng2.12880) — 3D DOA with 2.9° azimuth, 3.8° elevation accuracy
- [Multiple Sound Source Localization in Three Dimensions (IEEE, 2022)](https://ieeexplore.ieee.org/document/9997687) — Azimuth, elevation, distance estimation
- [Lightweight multi-DOA tracking of mobile speech sources (ResearchGate)](https://www.researchgate.net/publication/277351469_Lightweight_multi-DOA_tracking_of_mobile_speech_sources) — Moving speaker tracking
- [Audio-Visual Speaker Tracking (arXiv, 2023)](https://arxiv.org/html/2310.14778v2) — Multi-sensor tracking approaches
- [Voice Enhancement Direction of Arrival (Alango Technologies)](https://www.alango.com/direction-of-arrival.php) — Real-time DOA for voice assistants

### Competitors and Industry
- [Amazon Echo vs Google Home Comparison (Beebom)](https://beebom.com/amazon-echo-vs-google-home/) — Microphone array comparison (7 mics vs 2 mics)
- [ReSpeaker Mic Array v2.0 (Seeed Studio)](https://files.seeedstudio.com/wiki/ReSpeaker_Mic_Array_V2/res/ReSpeaker%2520MicArray%2520v2.0%2520Product%2520Brief.pdf) — 4-mic far-field reference design

### Multi-Talker and Advanced Features
- [WPD++: An Improved Neural Beamformer for Simultaneous Speech (arXiv, 2020)](https://arxiv.org/abs/2011.09379) — Multi-talker beamforming
- [End-to-End Multi-Microphone Speaker Extraction (arXiv, 2025)](https://arxiv.org/abs/2502.06586) — Speaker extraction from mixtures
- [Improving Multi-talker Binaural DOA Estimation (Springer, 2025)](https://link.springer.com/article/10.1007/s11063-025-02199-x) — Deep learning multi-talker DOA

### Reverberation and Acoustic Environment
- [Making Machines Understand Us in Reverberant Rooms (ResearchGate)](https://www.researchgate.net/publication/235333019_Making_Machines_Understand_Us_in_Reverberant_Rooms_Robustness_Against_Reverberation_for_Automatic_Speech_Recognition) — Reverberation impact on ASR
- [A Low-Complexity Robust Beamforming Using Diagonal Unloading (IEEE/ACM, 2017)](https://dl.acm.org/doi/pdf/10.1109/TASLP.2017.2789321) — Performance with RT60 = 0.4s
- [TELEVAL: A Dynamic Benchmark for Spoken Language (arXiv, 2025)](https://arxiv.org/html/2507.18061v3) — Testing with varying RT60 values

---

*Feature research for: 3D Audio DOA, DTOA, and Beamforming for Voice Assistant*
*Researched: 2025-02-15*
*Confidence: MEDIUM (some WebSearch-only findings; verified with multiple sources where possible)*
