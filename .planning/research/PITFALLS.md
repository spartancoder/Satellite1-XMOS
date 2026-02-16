# Pitfalls Research

**Domain:** XMOS XU316 DOA/DTOA/Beamforming with 4-mic Circular Array
**Researched:** 2025-02-15
**Confidence:** MEDIUM

## Executive Summary

Microphone array-based sound source localization and beamforming on XMOS XU316 faces several critical pitfalls that commonly lead to project failure or severe degradation of performance. The most significant issues are: (1) microphone calibration mismatches causing unpredictable DOA errors and beam pattern distortion, (2) spatial aliasing above ~3.4kHz for the specified 3.55mm radius circular array, (3) white noise amplification at low frequencies when using superdirective beamformers, and (4) real-time processing budget violations leading to audio glitches. Successful projects require careful module boundary design, extensive synthetic testing before hardware validation, and explicit handling of the frequency-limited operation imposed by the array geometry.

## Critical Pitfalls

### Pitfall 1: Uncalibrated Microphone Mismatch

**What goes wrong:**
Microphones have inherent variations in gain, phase response, and frequency characteristics. Without calibration, these mismatches cause random performance fluctuations in beamforming and DOA estimation. For second-order circular arrays, gain and phase errors can cause deviations in mainlobe direction or even mainlobe orientation reversal. Severe mismatch can make array performance worse than a single microphone.

**Why it happens:**
Manufacturing tolerances produce different sensitivity per microphone. PDM-to-PDM conversion and signal path variations introduce phase differences. Thermal characteristics change over time. Developers assume "identical microphones = identical response" and skip calibration.

**Consequences:**
- DOA estimates jump between adjacent directions
- Beamformed output has worse SNR than single mic
- Beam pattern distorted, pointing away from intended direction
- Performance "good one day, terrible the next" (random fluctuation)

**How to avoid:**
1. Implement per-microphone gain calibration at startup
2. Store phase calibration coefficients for frequency bands
3. Use reference signal calibration (94dB SPL at 1kHz from controlled source)
4. Periodically recalibrate in-field or provide calibration mode
5. Design software to be mismatch-invariant where possible (certain beamformer classes have this property)

**Warning signs:**
- DOA angle oscillates between two values for stationary source
- Beamformed audio quality varies day-to-day without environmental changes
- Different microphones produce noticeably different RMS levels in quiet environment
- Source appears to "move" when actually stationary

**Phase to address:** Phase 1 - Module Foundation

---

### Pitfall 2: Spatial Aliasing Above Array Design Frequency

**What goes wrong:**
Spatial aliasing occurs when wavelength is smaller than twice the distance between neighboring microphones (Nyquist criterion for spatial sampling). For a 3.55mm radius circular array with 90-degree spacing between adjacent mics, this limit is approximately 3.4kHz. Above this frequency, directional information becomes ambiguous, causing side-lobes in beam pattern and DOA misestimation.

**Why it happens:**
Developers treat beamformers as working across full frequency range (e.g., 20Hz-8kHz for speech). The circular array geometry fundamentally cannot resolve direction above f_max = c/(2*d) where d is adjacent mic spacing. At 3.55mm radius, adjacent mics are ~2.5mm apart, giving ~3.4kHz spatial Nyquist.

**Consequences:**
- Above 3.4kHz: beam pattern develops severe side-lobes
- DOA estimation ambiguous above aliasing frequency (wrong direction)
- High-frequency speech formants (F2, F3 ~2-4kHz) degraded
- Unexpected sources detected at ghost directions
- Sibilants and high-frequency speech components lost

**How to avoid:**
1. Explicitly define operational frequency range: DC to 3.4kHz for full 360-degree performance
2. Implement bandpass filtering before beamforming/DOA to limit input to valid range
3. Design post-processing to handle higher frequencies differently (possibly omnidirectional above aliasing)
4. Document limitations: "Directional accuracy only guaranteed below 3.4kHz"
5. Consider array redesign if higher frequency performance required (larger radius, more mics)

**Warning signs:**
- DOA accuracy degrades significantly for higher-pitched speakers vs lower-pitched
- Beamformed output has "swirly" or phasey quality on certain speakers
- Multiple ghost DOA detections when only one source present
- Spectral analysis shows artifacts primarily above 3kHz

**Phase to address:** Phase 1 - Module Foundation

---

### Pitfall 3: White Noise Amplification at Low Frequencies

**What goes wrong:**
Superdirective and MVDR beamformers maximize directivity at the expense of White Noise Gain (WNG). At low frequencies, WNG becomes very low, meaning the array amplifies spatially white noise. This is particularly serious at low frequencies and causes audible hiss that drowns out speech.

**Why it happens:**
Developers use textbook MVDR or superdirective formulas without WNG constraints. The math optimizes for directivity assuming perfect microphones. Real microphone arrays have limited aperture and mismatch, causing the optimizer to use extreme weights that amplify sensor noise. Circular arrays with small spacing are especially susceptible.

**Consequences:**
- Significant hiss in beamformed output at quiet voice levels
- SNR actually worse than single microphone at low frequencies
- Speech intelligibility reduced due to noise floor elevation
- User perception: "faint voice with loud background hiss"
- Performance varies dramatically with microphone distance (near-field vs far-field)

**How to avoid:**
1. Use WNG-constrained beamformers (diagonally loaded MVDR, etc.)
2. Implement minimum variance distortionless response with white noise gain constraint
3. Fall back to delay-and-sum at frequencies where WNG is too low
4. Monitor WNG during operation; switch modes when below threshold
5. Use regularized covariance matrices in adaptive algorithms

**Warning signs:**
- Beamformed audio has audible hiss that single mic doesn't
- Spectrogram shows elevated noise floor across low frequencies
- SNR measurements show beamformer worse than single mic
- Noise level scales with beamformer "strength" (more directivity = more hiss)

**Phase to address:** Phase 2 - Beamforming Module

---

### Pitfall 4: Real-Time Deadline Violations on XMOS XU316

**What goes wrong:**
Audio processing must complete within frame advance time (240 samples @ 16kHz = 15ms). Complex beamforming/GCC-PHAT algorithms exceed available computation budget, causing glitches, buffer underruns, or dropped frames. On XU316 with multi-tile architecture, inter-tile communication adds unpredictable latency.

**Why it happens:**
Algorithm complexity is estimated in isolation (O(n^3) or similar). Actual embedded implementation has overhead: FFT windowing, inter-tile message passing, memory access patterns, cache misses. Developers optimize algorithm asymptotics but miss constant factors and architecture-specific costs.

**Consequences:**
- Audio glitches (pops, clicks) during complex processing
- Intertile message queue overflow
- Watchdog resets or system hangs
- Inconsistent performance: works in testing, fails in production load
- Degraded voice quality from frame drops

**How to avoid:**
1. Profile actual cycle count per frame on target hardware (not just algorithm analysis)
2. Reserve 20-30% headroom in cycle budget for worst-case
3. Use coarse-to-fine search strategies for DOA (e.g., ODAS approach: coarse sphere then refined region)
4. Minimize inter-tile data transfer (compute on tile 1, send only results)
5. Implement quality scaling: reduce accuracy/precision when deadline approaching
6. Use frame-advance aware algorithms (process can span multiple frames if needed)

**Warning signs:**
- Frame processing time > 12ms regularly (close to 15ms deadline)
- Intertile queue depth grows over time
- Audio glitches correlate with complex acoustic scenes
- XSIM warnings about processing time exceeded

**Phase to address:** Phase 1 - Module Foundation

---

### Pitfall 5: Reverberation and Multipath Degradation

**What goes wrong:**
GCC-PHAT and basic DOA algorithms assume line-of-sight propagation. In real rooms with reflections, multiple path arrivals create correlation peaks at incorrect time delays. Beamforming based on wrong DOA amplifies reflections, suppressing the desired source.

**Why it happens:**
Algorithms validated in anechoic chamber. Developers assume "good enough" performance in typical room. Reverberation time (RT60) in typical home (0.4-0.8s) creates significant late reflections. Direct path may not have highest correlation if early reflections are strong.

**Consequences:**
- DOA jumps to wall direction when source stationary
- Beamformed audio sounds "distant" or "reverberant"
- Wrong beam direction chosen when speaker near reflective surface
- Voice activity detection failures due to ambiguous correlation peaks
- Poor performance in echoey rooms vs anechoic testing

**How to avoid:**
1. Implement direct-path dominance weighting (prefer earliest arrivals)
2. Use GCC-PHAT with robust front-end (LP whitening, sub-band processing)
3. Add multipath rejection to correlation processing
4. Implement tracking with velocity constraints (sources don't teleport)
5. Test in realistic reverberant environments, not just anechoic
6. Consider SRP-PHAT over plain GCC-PHAT for reverberant scenarios

**Warning signs:**
- DOA angles correlate with room geometry more than source position
- Performance much worse in-room vs outdoor
- Beamformed output has "hollow" or distant quality
- Correlation function has multiple prominent peaks of similar magnitude

**Phase to address:** Phase 2 - DOA/DTOA Module

---

### Pitfall 6: Insufficient Synthetic Test Coverage

**What goes wrong:**
Testing only with real microphones and speakers misses fundamental algorithm bugs. Synthetic testing can inject controlled signals to validate calibration, time delay estimation accuracy, and frequency response without physical hardware variations. Projects relying solely on "real-world" testing waste time chasing hardware-variant bugs.

**Why it happens:**
Hardware is slow/limited; synthetic testing is fast. Developers validate with real hardware early due to availability. Without synthetic tests, it's impossible to distinguish algorithm bugs from microphone calibration issues, room effects, or hardware faults.

**Consequences:**
- Algorithm bugs discovered late (after hardware changes)
- Calibration errors undetected until production
- Impossible to regress test without identical physical setup
- Long debug cycles chasing "mysterious" issues
- Test cases don't cover edge conditions (very close source, very far source, etc.)

**How to avoid:**
1. Develop separate synthetic test firmware for xsim
2. Generate test signals: point sources at known positions, varying SNR, multipath, calibration sweeps
3. Validate DOA error vs expected with synthetic input
4. Test time delay estimation accuracy with known delays
5. Verify frequency response of each "virtual microphone" independently
6. Use synthetic tests as gate before hardware integration

**Warning signs:**
- Bugs requiring hardware debug to understand
- Need to re-run physical tests after code changes
- Test flakiness blamed on environmental factors
- Edge cases never tested due to physical setup limitations

**Phase to address:** Phase 0 - Test Infrastructure (must precede all development)

---

### Pitfall 7: Module Boundary Violations (Impeding Extraction)

**What goes wrong:**
DOA, DTOA, beamforming, and post-processing modules share data structures, constants, and processing functions without clean boundaries. When attempting to extract modules to separate repositories, circular dependencies appear. ESP32 communication format changes require changes across multiple files.

**Why it happens:**
"Get it working first" mindset. Optimizing for integration speed rather than module portability. Shared state across modules for convenience. No defined interfaces/contracts between modules.

**Consequences:**
- Impossible to extract module without pulling in half the codebase
- ESP32 protocol changes require modifications in many places
- DOA and beamforming tightly coupled; can't use DOA separately
- Synthetic tests can't test module in isolation
- Code organization makes multi-tenant deployment impossible

**How to avoid:**
1. Define clear module interfaces at project start
2. Use opaque handles/structs for inter-module communication
3. Minimize shared constants; pass configuration via init
4. Keep ESP32 format translation in dedicated layer
5. Design for testability: each module must be mockable
6. Document module boundaries explicitly in architecture

**Warning signs:**
- Multiple #include dependencies for seemingly separate functionality
- Global state variables modified by multiple modules
- Module functions require knowledge of other modules' internals
- Adding a feature requires changes across many files

**Phase to address:** Phase 1 - Module Foundation (architectural decision point)

---

### Pitfall 8: Elevation Estimation with Coplanar Array

**What goes wrong:**
Four microphones in a circle (coplanar) cannot reliably estimate elevation. Developers attempt 3D DOA with insufficient degrees of freedom, getting ambiguous or wildly fluctuating elevation estimates. Azimuth may work reasonably, but elevation is essentially random.

**Why it happens:**
Coplanar array has no vertical resolution. Mathematical methods (like MUSIC) break down with rank deficiency. Circular array symmetric in XY plane produces same TDOA pattern for elevation theta and -theta. Confusion with front-back ambiguity masks the problem.

**Consequences:**
- Elevation estimate varies between 0 and 90 degrees arbitrarily
- 3D tracking fails even when azimuth tracking works
- Distance estimation meaningless without valid elevation
- User confusion: "it points up when speaker is level"

**How to avoid:**
1. Acknowledge limitation: coplanar array = 2D DOA only
2. Output azimuth only; mark elevation as "unsupported" or "assumed 0"
3. If 3D is required, add out-of-plane microphones (5th mic on stem, or dual-ring)
4. Document 3D DOA as requiring different hardware
5. Consider phased development: 2D DOA first, 3D later with hardware change

**Warning signs:**
- Elevation estimate changes wildly between frames
- Elevation doesn't correlate with actual source height
- Covariance matrix near-singular in DOA estimation
- Mathematical warnings in solver for underdetermined system

**Phase to address:** Phase 2 - DOA/DTOA Module (scope definition)

---

### Pitfall 9: Pipeline Order Dependency (AEC-Beamforming Interaction)

**What goes wrong:**
Beamforming placed before AEC reduces AEC effectiveness. Beamformed signals suppress echo reference direction, making echo estimation harder. AEC placed after beamforming processes already-corrupted reference. Double-talk detection fails. Echo returns through beamformer sidelobes.

**Why it happens:**
Treating modules as independent blocks. Pipeline order chosen by convenience rather than physics. Not considering that AEC needs unprocessed reference and beamforming needs echo-cancelled input. Multi-tile architecture adds latency that affects double-talk detection timing.

**Consequences:**
- Echo leakage despite AEC enabled
- Double-talk speaker suppressed by AEC
- Beamformer points at echo source (reflection)
- Performance worse with beamforming + AEC than AEC alone
- Inconsistent results with different pipeline configurations

**How to avoid:**
1. Define canonical pipeline order: PDM -> Decimation -> (calibration) -> Beamforming -> AEC -> VNR -> NS -> AGC
2. If beamforming before AEC, use non-beamformed reference for AEC
3. Account for pipeline latency in double-talk detection
4. Test each pipeline stage independently and in combination
5. Document required order and explain physics rationale

**Warning signs:**
- AEC metrics worse with beamforming enabled
- Echo returns after certain "cool-down" time
- Double-talk detection never fires
- Different variants produce different echo performance

**Phase to address:** Phase 2 - Integration and Pipeline Order

---

### Pitfall 10: Fixed-Point Overflow and Precision Loss

**What goes wrong:**
GCC-PHAT correlation, beamforming weights, and covariance calculations exceed fixed-point headroom on XMOS. Intermediate values overflow, wrap, or saturate, causing silent failures. Precision loss accumulates through pipeline, resulting in audible artifacts or algorithm instability.

**Why it happens:**
Floating-point reference implementation converted to Q31 fixed-point for XMOS. Intermediate products of correlation or beamforming can exceed range. Headroom analysis done only with typical signals; edge cases (clipping, low noise, pathological signals) cause overflow. Rounding errors compound through FFT windowing.

**Consequences:**
- Silent overflow: algorithm produces garbage but doesn't crash
- Beamforming weights produce wrong pattern
- Correlation peaks missed due to saturation
- Numerical instability in adaptive algorithms
- Inconsistent results between Q31 and float32 simulation

**How to avoid:**
1. Use Q.31 format with 0.31 decimal bits for headroom
2. Add saturation detection with early warning
3. Implement guard bits in critical accumulators
4. Profile maximum intermediate values with synthetic edge cases
5. Use 64-bit accumulators where intermediate may overflow
6. Test with pathological signals (clipping, all ones, pink noise at -1dBFS)

**Warning signs:**
- Algorithm works at some levels but fails at others
- Inconsistent results across different input amplitudes
- Spectrogram shows sudden drops/clicks
- xsim differs from hardware in unpredictable ways

**Phase to address:** Phase 1 - Module Foundation (numerical design point)

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|-----------|-------------------|------------------|------------------|
| Skip calibration in initial prototype | Faster initial development | Performance degrades over time; hard to debug later | Only for concept validation (proof of algorithm), never for production code |
| Assume omnidirectional above 3.4kHz | Simpler code, works initially | Poor performance validated in-field; confused users | Never: fundamental physics violation |
| Use delay-and-sum beamformer only | Guaranteed to work, simple | Missed directivity gains, poor SNR | Maybe for MVP, must plan upgrade path |
| Ignore reverberation handling | Simpler algorithm, better anechoic performance | Fails in real rooms, poor voice quality | Only for anechoic chamber testing only |
| Combine DOA and beamforming modules tightly | Faster iteration initially | Cannot extract, cannot test separately, can't evolve modules | Never: violates module portability requirement |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|------------------|------------------|
| ESP32 SPI Communication | Sending all audio samples for processing | Send only DOA results, status; keep audio on XMOS |
| Inter-tile Communication | Blocking calls from audio thread | Use non-blocking sends, separate control path |
| I2S Output | Outputting beamformed only | Support dual output: beamformed + reference for debugging |
| AEC Reference | Using beamformed reference | Use pre-beamforming reference for AEC |
| GPIO RPC | Directly calling module functions | RPC layer maintains boundary; module callable from test harness |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|-----------|------------|------------------|
| Excessive inter-tile data | Queue depth growing, dropped frames | Process on tile 1, send results only; minimize transfer size | Always: with 240-sample frames |
| Full-resolution DOA search | Frame time > 15ms, glitches | Coarse-to-fine search; limit candidate directions | With 4+ candidate DOA tracking |
| All-pair GCC-PHAT | N^2 correlation load | Use relevant pairs only (e.g., ODAS directivity model) | With 4-mic array (6 pairs OK, but 8+ needs optimization) |
| No WNG constraint | Audible hiss at low frequencies | Diagonal loading, regularized covariance | With MVDR or superdirective beamformers |
| Unbounded FFT frame size | Cache thrashing, latency increase | Use power-of-2 frame sizes matching frame advance | With frame advances > 512 samples |

---

## Security Mistakes

| Mistake | Risk | Prevention |
|----------|------|------------|
| Unvalidated input from ESP32 | Control commands crash system | Bounds checking, command validation before action |
| DOA results trusted blindly | Beamformer amplifies wrong direction | DOA confidence scoring; fallback to omnidirectional when low confidence |
| Calibration values from untrusted source | Permanent performance degradation | Validate calibration range; require min SNR for auto-calibration |
| Exposed debug interfaces in production | Unauthorized access crashes audio pipeline | Compile-time removal of debug interfaces |

---

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|----------|---------------|------------------|
| "Ghost" DOA detections | Voice assistant responds to non-existent sources | Minimum dwell time before reporting; DOA confidence threshold |
| Sudden beam direction changes | Audio jumps, jarring transitions | Smooth DOA tracking; rate-of-change limits |
| No elevation when 3D claimed | User tries to adjust height, no effect | Document 2D-only limitation; elevation disabled in UI |
| Beamformed audio only | Can't hear from unexpected direction | Always output omnidirectional reference alongside beamformed |

---

## "Looks Done But Isn't" Checklist

- [ ] **DOA Module:** Often missing calibration storage — verify per-mic calibration coefficients persisted and loaded
- [ ] **DOA Module:** Often missing confidence scoring — verify low-DOA-confidence produces omnidirectional fallback
- [ ] **Beamforming Module:** Often missing WNG monitoring — verify beamformer disables superdirective mode when WNG too low
- [ ] **DTOA Module:** Often missing sub-sample interpolation — verify TDOA resolution exceeds sample period
- [ ] **Pipeline Integration:** Often missing order documentation — verify explicit documented order and justification
- [ ] **Module Extraction:** Often missing interface contracts — verify module compiles and tests independently
- [ ] **Synthetic Tests:** Often missing multipath simulation — verify DOA handles reflected sources
- [ ] **Synthetic Tests:** Often missing edge cases — verify behavior at max distance (near-field vs far-field)
- [ ] **Fixed-Point Math:** Often missing overflow guards — verify saturating operations have early warning
- [ ] **ESP32 Protocol:** Often missing version field — verify backwards compatibility with existing protocol

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|----------|---------------|------------------|
| Uncalibrated microphones | HIGH | Perform factory calibration; store coefficients; add periodic recalibration mode |
| Spatial aliasing degradation | MEDIUM | Bandlimit input to <3.4kHz; document as design constraint |
| White noise amplification | LOW | Switch to delay-and-sum; implement WNG monitoring and fallback |
| Real-time deadline violations | HIGH | Profile cycle counts; implement coarse-to-fine; simplify algorithm; add quality scaling |
| Module boundary violations | HIGH | Refactor interfaces; extract to separate repos; add mock tests |
| Pipeline order issues | MEDIUM | Reorder pipeline; document canonical order; test all permutations |
| Fixed-point overflow | HIGH | Add guard bits; implement saturation detection; expand accumulator width |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|----------|------------------|--------------|
| Uncalibrated microphones | Phase 1: Module Foundation | Per-mic frequency response within spec; DOA consistent across calibrations |
| Spatial aliasing | Phase 1: Module Foundation | Frequency response rolls off correctly above 3.4kHz; DOA accuracy maintained to limit |
| White noise amplification | Phase 2: Beamforming | SNR measurement with beamformer vs single mic shows no degradation at low frequencies |
| Real-time deadline violations | Phase 1: Module Foundation | Frame time < 12ms for all test cases; no queue growth in long-running tests |
| Reverberation degradation | Phase 2: DOA/DTOA | DOA accuracy > 15 degrees in RT60 < 0.5s rooms; tracking velocity reasonable |
| Insufficient synthetic coverage | Phase 0: Test Infrastructure | Algorithm bugs caught in xsim before hardware; regression tests pass without hardware |
| Module boundary violations | Phase 1: Module Foundation | Module compiles in isolation; mock tests pass; no cross-module includes |
| Elevation estimation | Phase 2: DOA/DTOA | Scope documented as 2D-only; elevation API marked unsupported or constant |
| Pipeline order | Phase 2: Integration | AEC metrics same with beamforming as without; double-talk detection works |
| Fixed-point overflow | Phase 1: Module Foundation | No saturation in edge case tests; Q31 results match float32 simulation |

---

## Sources

### Spatial Aliing
- [Increasing the spatial aliasing frequency of circular arrays via...](https://dael.euracoustics.org/confs/fa2025/data/articles/000465.pdf) — Spatial aliasing occurs when wavelength < 2*d, limiting circular array performance
- [Circular microphone array based beamforming and source...](https://www.spsc.tugraz.at/sites/default/files/Clenet10_DA.pdf) — Main advantages of circular array less affected by spatial aliasing than linear arrays, but still limited
- [Towards an enhanced performance of uniform circular arrays at low...](https://backend.orbit.dtu.dk/ws/portalfiles/portal/58834161/Towards+an+enhanced+performance+of+UCAs+at+low+frequencies.pdf) — Following Nyquist, array presents spatial aliasing from ~2.8 kHz (similar to our 3.4kHz)
- [Moving microphone arrays to reduce spatial aliasing in the...](https://pubs.aip.org/asa/jasa/article-pdf/124/6/3648/10705864/3648_1_online.pdf) — Example: linear array with 10cm spacing has max frequency ~3400 Hz

### White Noise Amplification
- [Fully Automatic Balance between Directivity Factor and White Noise...](https://www.isca-archive.org/interspeech_2022/meng22b_interspeech.pdf) — Superdirective beamformer maximizes directivity at expense of amplifying spatially white noise in low-frequency bands
- [Microphone array beamforming based on maximization of the front...](https://pubs.aip.org/asa/jasa/article/144/6/3450/994112/Microphone-array-beamforming-based-on-maximization) — Beamformer suffers from white noise amplification, particularly serious at low frequencies
- [Superdirective Beamforming Based on Krylov Matrix](https://dl.acm.org/doi/pdf/10.1109/TASLP.2016.2618003) — For superdirective beamformer, WNG can be very low at low frequencies, indicating significant white noise amplification

### Microphone Mismatch and Calibration
- [The impact of sensor mismatch errors on the robustness of circular...](https://www.sciencedirect.com/science/article/abs/pii/S0003682X25000556) — Gain and phase errors cause random performance fluctuation; second-order arrays can have mainlobe direction reversal
- [A New Gain-Phase Error Pre-Calibration Method for Uniform Linear...](https://www.mdpi.com/1424-8220/23/5/2544) — Gain-phase error calibration problem; performance degraded by mismatch
- [Reverberation-Robust Self-Calibration and Synchronization of...](https://www.mdpi.com/1424-8220/24/1/114) — Methods for geometric scaling and synchronization parameter estimation

### GCC-PHAT and DOA Implementation
- [ODAS: Open embeddeD Audition System](https://www.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2022.854444/full) — GCC-PHAT implementation strategies, coarse-to-fine search, closed array optimization
- [Steered Response Power for Sound Source Localization: a tutorial...](https://link.springer.com/article/10.1186/s13636-024-00377-z) — SRP-PHAT prominent flavor using GCC-PHAT; challenges in large outdoor environments
- [Direction of Arrival of Sound using a circular 4 microphone array?](https://www.reddit.com/r/DSP/comments/1bnjtq5/direction_of_arrival_of_sound_using_a_circular_4/) — Practical discussion of 4-mic circular array limitations
- [CNN-based Robust Sound Source Localization with SRP-PHAT for...](https://dl.acm.org/doi/full/10.1145/3586996) — Challenges deploying robust SSL on resource-constrained edge devices

### Real-Time Processing
- [Building a High-Performance Multi-Threaded Audio Processing...](https://acestudio.ai/blog/multi-threaded-audio-processing/) — Never block the real-time audio thread; delay leads to audible glitches
- [Real-time audio programming 101: time waits for nothing](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing) — Blocking operations in audio code cause glitches
- [lib_audio_dsp: Audio DSP Library - XMOS](https://www.xmos.com/documentation/XM-015103-UG/pdf/lib_audio_dsp_v1.4.0.pdf) — Each thread in DSP pipeline must complete execution in less time than frame advance

### Pipeline Integration and AEC
- [Joint Neural AEC and Beamforming with Double-Talk Detection](https://www.isca-archive.org/interspeech_2022/kothapally22_interspeech.pdf) — AEC and beamforming interaction challenges in full-duplex communication
- [Multichannel Acoustic Echo Cancellation With Beamforming in...](https://israelcohen.com/wp-content/uploads/2023/12/Multichannel_Acoustic_Echo_Cancellation_With_Beamforming_in_Dynamic_Environments.pdf) — Multichannel echo canceller with beamformer adapting to changing environments
- [Voice Processing Pipeline - XMOS XVF3800 v3.2.1](https://www.xmos.com/documentation/XM-014888-PC/html/modules/fwk_xvf/doc/datasheet/03_audio_pipeline.html) — XMOS reference pipeline with AEC and beamforming integration

### Front-Back Ambiguity and Elevation
- [2D Direction of Arrival Estimation Using Uniform Circular Arrays With...](https://ieeexplore.ieee.org/iel7/6287639/9668973/09690883.pdf) — Elevation pattern diversity for unambiguous 2D DOA with UCAs
- [Deep learning-based direction of arrival estimation for underwater...](https://rgu-repository.worktribe.com/OutputFile/3168713) — Linear arrays suffer from front-back ambiguity; circular arrays offer 360-degree coverage but still have challenges
- [DOA estimation with Lightweight Network on LLM-Aided Simulated...](https://arxiv.org/html/2511.08012v1) — Front-back ambiguity in raw DOA angles; mapping applied for mitigation

### 4-Mic Circular Array Specific
- [Rank-1 ambiguity DOA estimation of circular array with fewer sensors](https://www.researchgate.net/publication/4007890_Rank-1_ambiguity_DOA_estimation_of_circular_array_with_fewer_sensors) — UCAs usually considered free of ambiguity (rank-1) but fewer sensors create issues

---

*Pitfalls research for: XMOS XU316 DOA/DTOA/Beamforming*
*Researched: 2025-02-15*
