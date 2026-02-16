# Stack Research

**Domain:** 3D Audio DOA, DTOA, and Beamforming on XMOS XU316 with 4-Mic Circular Array
**Researched:** 2025-02-15
**Confidence:** MEDIUM

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|----------------|
| XMOS lib_mic_array | v6.0.0 | PDM microphone capture and decimation | Official XMOS library optimized for XS3 architecture with vector processing unit. Supports 1-16 microphones, configurable frame sizes, and phase-aligned capture required for DOA algorithms. |
| XMOS lib_xcore_math | v2.4.0 | DSP primitives (FFT, BFP, vector ops) | Official XMOS math library leveraging XS3 Vector Processing Unit (VPU). Provides Block Floating Point (BFP) arithmetic, FFT/DCT, and linear filtering. Required for efficient GCC-PHAT and beamforming. |
| XMOS lib_audio_dsp | v1.4.0 | Audio-specific DSP stages | Official XMOS audio DSP library providing common signal processing functions optimized for xcore processors. Includes pipeline stages useful for post-beamforming audio processing. |
| XMOS XTC Tools | 15.3.1 | Build toolchain and xSIM simulator | Required toolchain for XMOS XS3A architecture. xSIM provides cycle-accurate simulation for testing without hardware. |
| XMOS fwk_voice (XCORE-VOICE) | v2.3.1 | Voice processing framework | XMOS voice solution framework providing C-based SDK for audio front-end applications. Reference for existing DSP stage patterns (AEC, VNR, NS, AGC). |
| GCC-PHAT algorithm | Custom (standard) | Time Difference of Arrival estimation | Industry-standard TDOA estimation algorithm robust to reverberation. Required for accurate DOA on small circular arrays. |
| MVDR beamforming | Custom (standard) | Minimum Variance Distortionless Response beamforming | Optimal beamformer that minimizes variance while maintaining distortionless response. Superior to delay-sum for directional enhancement. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| kissFFT (or similar) | N/A | Lightweight FFT for cross-correlation | Use only if lib_xcore_math FFT does not meet requirements. Generally prefer lib_xcore_math for VPU-optimized transforms. |
| robin1001/beamforming | N/A | Reference implementations | Use as algorithm reference for MVDR, GSC, Delay-Sum, and GCC-PHAT. C/C++ implementations with MATLAB equivalents for verification. |
| myPyroomacoustics | Latest | Synthetic audio test generation | Use for generating test datasets with configurable room acoustics, microphone arrays, and sound sources. Essential for algorithm validation before hardware testing. |
| pyroomacoustics | Latest | Room simulation and algorithm testing | Use for rapid development and testing of audio array processing algorithms in Python. Simulates complex acoustic environments. |
| sign_xcorr | N/A | Cross-correlation with FFT | Use as reference for FFT-based cross-correlation implementation in C. Demonstrates normalization techniques for correlation coefficients. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| pytest | Python unit test framework | Already in use in project. Use with parametrization for testing multiple azimuth/elevation/distance combinations. |
| numpy | Numerical computing | Required for synthetic test generation. Use for creating test signals, noise, and reference outputs. |
| scipy | Signal processing | Use `scipy.signal` for reference filter implementations and cross-correlation validation. |
| xSIM (XMOS simulator) | Cycle-accurate firmware simulation | Use for algorithm verification on XS3 architecture without hardware. Supports extensive tracing for debugging. |
| xtag debugger | Hardware debugging | Required for real-time debugging on XMOS hardware. |
| Unity (embedded C test framework) | Unit testing on-device | Used by existing fwk_voice tests. Pattern established for C-level DSP testing. |

## Installation

```bash
# Python dependencies for test generation and validation
uv pip install numpy scipy soundfile pytest

# XMOS dependencies (submodules)
git submodule update --init --recursive

# Build with existing toolchain
source /opt/xmos/XTC/15.3.1/SetEnv
cmake -B build -DUSE_DEV_TRACKING=ON --toolchain xmos_cmake_toolchain/xs3a.cmake
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|------------------------|
| lib_xcore_math VPU-optimized FFT | kissFFT / pocketfft | Use alternative only if porting to non-XMOS platforms. lib_xcore_math is hardware-accelerated. |
| MVDR beamforming | Delay-Sum beamforming | Use Delay-Sum for simplicity when beamforming is a secondary feature or computational budget is extremely constrained. MVDR provides better SNR improvement. |
| Custom GCC-PHAT implementation | SRP-PHAT (Steered Response Power) | Use SRP-PHAT when multi-source localization is required. GCC-PHAT is more efficient for single-source DOA. |
| Custom C implementation | XVF3800 VocalFusion SDK | Use XVF3800 SDK if "black box" solution is acceptable. Custom C implementation provides portability and algorithm control. |
| Circular array TDOA (planar DOA) | 3D array with elevation | Use planar (azimuth-only) arrays when elevation is not required. 3D localization requires additional microphones or array configuration. |
| Fixed-point BFP arithmetic | 32-bit floating point | Use floating point only during development/debugging. Fixed-point BFP is required for production on XMOS to maintain performance and accuracy tradeoff. |

## What NOT to Use

| Avoid | Why | Use Instead |
|--------|-----|--------------|
| MATLAB-only implementation | Cannot be ported to embedded C without rewrite. Use MATLAB only for algorithm prototyping and verification. |
| DSP libraries not optimized for XS3 | Poor performance, wasted MIPS. Use lib_xcore_math which leverages VPU. |
| MUSIC algorithm for 4-mic circular array | Requires at least N+1 microphones for N sources, computationally expensive, poor performance on small arrays. Use GCC-PHAT-based TDOA which is efficient and robust for single-source DOA. |
| Blind source separation (BSS) | Too complex, unstable for real-time voice. Use beamforming followed by existing VNR/NS stages. |
| Neural network DOA | Requires training data, heavy computational load. Use classical signal processing (GCC-PHAT + MVDR) for deterministic performance. |
| Pure azimuth DOA (2D) | Limited utility for voice interaction. Implement 3D DOA with elevation for more natural user interaction. |
| Arbitrary precision floating point | Not available in hardware, poor performance. Use Block Floating Point (BFP) from lib_xcore_math which matches hardware capabilities. |
| lib_mic_array v5.x for XS3 | v5.0.0 was major redesign for XS3, earlier versions for XS2. Current project uses XS3, must use v6.0.0+. |

## Stack Patterns by Variant

**If implementing DOA only (azimuth + elevation, no distance):**
- Use GCC-PHAT TDOA estimation on adjacent mic pairs
- Interpolate between pairs for smooth angle output
- Because circular 4-mic array has 90 spacing, TDOA is reliable for adjacent pairs

**If implementing full 3D DOA (azimuth + elevation + distance):**
- Use GCC-PHAT TDOA for azimuth/elevation
- Use amplitude ratio or multi-frequency analysis for distance
- Because single circular array cannot resolve distance without reference or multi-frequency processing

**If implementing MVDR beamforming:**
- Use covariance matrix computation from mic inputs
- Use matrix inversion via lib_xcore_math or custom implementation
- Because MVDR requires adaptive update of steering vector for optimal null placement

**If implementing real-time processing at 16kHz:**
- Use frame-based processing (240 samples per frame at 16kHz)
- Update DOA estimate every frame for voice interaction
- Because 16ms latency (1 frame at 16kHz) is acceptable for voice commands

**If implementing spatial aliasing mitigation:**
- Limit beamforming frequency range to ~3.4kHz for 3.55mm radius circular array
- Use separate beamformers for different frequency bands
- Because adjacent mic pair spacing creates spatial aliasing above ~3.4kHz

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| lib_mic_array v6.0.0 | XMOS XTC Tools 15.3.0+, XS3 architecture only | Requires lib_xcore_math dependency |
| lib_xcore_math v2.4.0 | XMOS XTC Tools 15.0.0+ | Provides VPU-accelerated functions for XS3 |
| lib_audio_dsp v1.4.0 | XMOS XTC Tools 15.2.0+ | Audio DSP library compatible with xcore.ai multichannel platform |
| fwk_voice v2.3.1 | XMOS XTC Tools 15.2.0+ | Voice solution framework, requires XCORE.AI evaluation kit |

## Frequency Band Considerations

For 4-mic circular array with 3.55mm radius:

| Frequency Range | Spatial Aliasing | Recommendation |
|----------------|-------------------|---------------|
| < 500 Hz | Poor resolution (wavelength >> array diameter) | Use larger array or accept coarse DOA |
| 500 Hz - 3.4 kHz | Good resolution, no aliasing | **Primary operating range for voice** |
| 3.4 kHz - 8 kHz | Spatial aliasing on adjacent pairs | Use diagonal pairs only or sub-band beamforming |
| > 8 kHz | Severe spatial aliasing | Not suitable for DOA with this array geometry |

Lower frequency limit: ~500 Hz (practical for voice due to SNR and wavelength constraints)

## Testing Frameworks

### Python Testing (Algorithm Validation)

```python
# pytest-based testing for synthetic audio generation
import pytest
import numpy as np
from scipy import signal

@pytest.fixture
def synthetic_voice_signal():
    """Generate synthetic voice at 16kHz."""
    return generate_voice_signal(
        frequency=1000,
        sample_rate=16000,
        duration=0.5,
        noise_level=0.01
    )

@pytest.mark.parametrize("azimuth", [0, 45, 90, 135, 180, 225, 270, 315])
@pytest.mark.parametrize("elevation", [-30, -15, 0, 15, 30])
def test_doa_estimation(synthetic_voice_signal, azimuth, elevation):
    """Test DOA estimation accuracy across directions."""
    # Generate multichannel signal from array geometry
    multichannel = simulate_array_capture(
        signal=synthetic_voice_signal,
        array_radius=0.00355,
        mics=4,
        azimuth=azimuth,
        elevation=elevation
    )
    # Run DOA estimation
    estimated = gcc_phat_doa(multichannel, sample_rate=16000)
    # Assert accuracy within X degrees
    assert angle_difference(estimated, (azimuth, elevation)) < 15
```

### Embedded C Testing (Unit Tests)

```c
// Unity-based testing for DOA module
#include "unity.h"
#include "doa_api.h"

void test_gcc_phat_peak_detection(void)
{
    // Setup test signals with known delay
    int32_t signal_a[256];
    int32_t signal_b[256];
    generate_test_signals(signal_a, signal_b, 5); // 5 sample delay

    // Run GCC-PHAT
    float_s32_t correlation[256];
    gcc_phat(signal_a, signal_b, correlation, 256);

    // Verify peak at expected position
    int32_t peak_index = find_peak(correlation, 256);
    TEST_ASSERT_EQUAL_INT(5, peak_index);
}

void test_mic_array_delay_compensation(void)
{
    // Test that delays compensate for circular array geometry
    int32_t delays[4];
    calculate_mic_delays(45, 0, delays); // Azimuth 45, Elevation 0

    // Verify expected delay pattern
    // ... assertions for delay values
}
```

### Synthetic Test Generation Tools

| Tool | Purpose | Usage |
|-------|---------|---------|
| pyroomacoustics | Room simulation with configurable arrays | `create_microphone_array()`, `simulate_room()` |
| numpy | Signal generation and basic processing | `np.sin()`, `np.fft.fft()`, `np.correlate()` |
| scipy | Advanced signal processing | `scipy.signal.correlate()`, `scipy.signal.butter()` |
| soundfile | Audio file I/O for test data | `soundfile.write()`, `soundfile.read()` |

## Sources

### XMOS Official Documentation (HIGH confidence)

- [lib_mic_array: PDM microphone array library v6.0.0](https://www.xmos.com/file/lib_mic_array) — Microphone capture library for XMOS XS3, requires lib_xcore_math
- [lib_xcore_math: xcore optimised math v2.4.0](https://www.xmos.com/documentation//XM-015059-UG/html/doc/rst/src/introduction.html) — Vector processing functions utilizing XS3 VPU, including FFT and BFP operations
- [lib_audio_dsp: Audio DSP Library v1.4.0](https://www.xmos.com/documentation/XM-015103-UG/pdf/lib_audio_dsp_v1.4.0.pdf) — Official audio DSP library for XMOS processors (2025-06-27)
- [XCORE-VOICE Solution Programming Guide v2.3.1](https://www.xmos.com/documentation/XM-014785-PC/pdf/sln_voice_programming_guide_v2.3.1.pdf) — Voice processing framework documentation (2025-04-14)
- [XTC Tools Guide v15.3.1](https://www.xmos.com/documentation/XM-014363-PC/pdf/xtc_tools_guide_v15.3.pdf) — Build toolchain documentation
- [Mic Array Programming Guide](https://www.xmos.com/documentation/XM-014926-PC/pdf/mic_array_programming_guide.pdf) — Microphone array implementation guide for xcore.ai devices
- [xSIM Simulator Documentation](https://laforge.gnumonks.org/blog/20170902-xmos/) — Cycle-accurate simulator integrated in xTIMEcomposer

### Algorithm References (MEDIUM confidence)

- [robin1001/beamforming GitHub Repository](https://github.com/robin1001/beamforming) — C/C++ implementations of Delay-Sum, MVDR, GSC, and GCC-PHAT with MATLAB verification
- [A New DOA Estimation Method Using Circular Microphone Array](https://www.eurasip.org/Proceedings/Eusipco/Eusipco2007/Papers/b1l-d05.pdf) — Circular array DOA using TDOA
- [Steered Response Power for Sound Source Localization: a tutorial](https://link.springer.com/article/10.1186/s13636-024-00377-z) — Tutorial on GCC-PHAT computation and DOA (2024-11-24)
- [Robust Three-Microphone Speech Source Localization](https://pmc.ncbi.nlm.nih.gov/articles/PMC8681871/) — DOA estimation fundamentals for beamforming
- [Localization using Angle-of-Arrival Triangulation](https://arxiv.org/html/2508.16908v1) — Delay estimation using GCC-PHAT and TDoA (2025-08-23)

### Testing Tools (MEDIUM confidence)

- [pyroomacoustics Documentation](https://pyroomacoustics.readthedocs.io/) — Python package for room acoustic simulation and array processing
- [Acoular: Analyzing multichannel audio from microphone arrays](https://pypi.org/project/acoular/) — Python package for acoustic source localization algorithms (BSD 3-clause)
- [Beamforming Library - Array Signal Processing Simulation](https://github.com/AkenoSyuRi/beamforming) — Python implementation with MVDR, CBF, MUSIC, supporting arbitrary array shapes
- [kiss_xcorr: Cross-Correlation with KISS FFT](https://github.com/lucidfrontier45/kiss_xcorr) — Dedicated cross-correlation library using KISS FFT
- [sign_xcorr: Audio Signal Cross-Correlation](https://github.com/jchavanton/sign_xcorr) — Sample program for cross-correlation using FFT on PCM audio
- [NumPy Testing Guidelines](https://numpy.org/doc/2.4/reference/testing.html) — NumPy uses pytest framework (v2.4+), official testing documentation
- [SciPy Signal Processing](https://docs.scipy.org/doc/scipy/tutorial/signal.html) — Comprehensive documentation on scipy.signal module for filters and signal processing operations

### Product/Implementation References (LOW-MEDIUM confidence)

- [ReSpeaker XMOS XVF3800 4-Mic Array](https://www.seeedstudio.com/reSpeaker-Mic-Array-v3.0/) — Commercial product using XMOS XU316 with AEC, beamforming, DOA, dereverberation (2025)
- [XVF3800 Programming Guide v3.2.1](https://www.xmos.com/documentation/XM-014888-PC/pdf/xvf3800_programming_guide_v3.2.1.pdf) — VocalFusion processor documentation with microphone array processing capabilities
- [XMOS VocalFusion SDK Documentation Hub](https://www.xmos.com/documentation/XM-014888-PC/html/modules/fwk_xvf/doc/user_guide/05_building_the_firmware.html) — Firmware build instructions for XVF3800-based solutions

### 2025 Research Papers (LOW confidence)

- [End-to-End DOA-Guided Speech Extraction in Noisy Multi-Talker Scenarios](https://arxiv.org/html/2507.20926v1) — Uses pyroomacoustics for simulation with 3-channel circular array (2025-07-28)
- [High-Resolution DOA Estimation of UAVs Using Microphone Arrays](https://www.mdpi.com/2076-3417/15/15/8734) — Dynamic simulation environment with rotating microphone array (2025-10-10)
- [3D Multiple Sound Source Localization](https://www.mdpi.com/1424-8220/22/3/1011) — Circular microphone arrays combined with adaptive GCC-PHAT/ML algorithms for DOA (2022)

---
*Stack research for: 3D Audio DOA, DTOA, and Beamforming on XMOS XU316*
*Researched: 2025-02-15*
