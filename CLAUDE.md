# CLAUDE.md - Satellite1-XMOS Development Guide

## Project Overview

This is the Satellite1 XMOS firmware for FutureProofHomes, a multi-tile XMOS XCORE.AI audio processing system that implements various audio pipelines including AEC (Acoustic Echo Cancellation), VNR (Voice Noise Reduction), Noise Suppression, AGC (Automatic Gain Control), and beamforming. The firmware interfaces with an ESP32-S3 companion processor.
It is being extended to include 4 microphones doa, dtoa, beamforming, and post beamforming filtering to increase voice clairity.

## Build Commands

### Setup

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/FutureProofHomes/Satellite1-XMOS.git

# Setup Python environment
uv python install 3.10
uv venv .venv --python=3.10
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
uv pip install -r requirements.txt
```

### Build Firmware

**Linux/Mac:**

```bash
source .venv/bin/activiate
source /opt/xmos/XTC/15.3.1/SetEnv
cmake -B build -DUSE_DEV_TRACKING=ON --toolchain xmos_cmake_toolchain/xs3a.cmake
cd build
make <variant_name>                    # Build .xe only
make create_flash_img_<variant_name>    # Create factory image
make create_upgrade_img_<variant_name>  # Create upgrade image
```

### CI Build Script

```bash
./tools/ci/build_firmware.sh tools/ci/firmwares.txt
```

## Test Commands

```bash
# Run Python tests
pytest -rA --junitxml=pytest_result.xml -o junit_logging=all

# Run specific test
pytest tests/test_versioning/test_version_parsing.py::test_specific_function
```

## Firmware Variants

Key variants defined in `satellite-xmos-firmware/satellite1.cmake`:

- `satellite1_firmware_fixed_delay` - Main production variant with AEC
- `satellite1_firmware_bypass` - Raw mic streaming to ESP32
- `satellite1_firmware_adec` - Full audio processing pipeline
- `explorer1_firmware_*` - XCORE.AI evaluation board variants
- `satellite1_usb_firmware_*` - USB Audio device variants
- `satellite1_firmware_mvdr` - New Production Firmware to be created

## Architecture Overview

### Multi-Tile Architecture

- **Tile 0**: Main application control, USB interfaces, GPIO, LED ring control
- **Tile 1**: Audio processing pipeline (PDM capture, I2S, audio DSP)

### Key Directories

- `satellite-xmos-firmware/src/` - Main application source
  - `control/` - RPC-based inter-tile communication, GPIO RPC
  - `dfu_int/` - Device Firmware Update interface
  - `usb/` - USB stack implementation
  - `gpio/`, `led_ring/` - Hardware control interfaces
- `satellite-xmos-firmware/audio_pipelines/` - Audio pipeline configurations
- `modules/` - XMOS framework submodules (core, io, rtos, voice, inferencing)

### Audio Pipeline Configuration

Configured via compile-time defines in `app_conf.h`. Pipeline stages can be enabled/disabled:

- `appconfPIPELINE_BYPASS` - Bypass entire pipeline
- `appconfAUDIO_PIPELINE_SKIP_AEC` - Skip AEC stage
- `appconfAUDIO_PIPELINE_SKIP_IC_AND_VNR` - Skip IC and VNR stages
- `appconfAUDIO_PIPELINE_SKIP_NS` - Skip Noise Suppression
- `appconfAUDIO_PIPELINE_SKIP_AGC` - Skip AGC

Sample rate: 16kHz (default), configurable via `appconfAUDIO_PIPELINE_SAMPLE_RATE`

Supports TDM mode to output multiple audio channels over single I2S line.

### Inter-tile Communication

- Uses XMOS inter-tile ports (channels 0, 1, 2, 7 defined in `app_conf.h`)
- RPC framework for GPIO control between tiles

## Versioning System

- Version controlled by `firmware_version.txt` in project root
- `satellite-xmos-firmware/versioning.py` generates `version.h` during build
- Supported formats: `v{major}.{minor}.{patch}`, with optional `-alpha`, `-beta`, `-rc`, `-dev` and counter
- Dev builds with `USE_DEV_TRACKING` option automatically track build counter in `dev_tracking/` directory

## Key Files

- `satellite-xmos-firmware/CMakeLists.txt` - Main build configuration
- `satellite-xmos-firmware/firmware.cmake` - Firmware-specific build rules
- `satellite-xmos-firmware/src/app_conf.h` - Application configuration
- `satellite-xmos-firmware/satellite1.cmake` - Satellite1 firmware variant definitions
- `xmos_cmake_toolchain/xs3a.cmake` - XMOS toolchain for XS3A architecture

## Build System Requirements

- XTC-Tools Version 15.3.1 (download from xmos.com)
- Python 3.10 with `xmos-ai-tools==1.3.1`
- CMake with XMOS toolchain
- Ninja generator (recommended for Windows)

## Flashing Methods

- **Initial flash**: Via ESPHome firmware (SPI flash programming)
- **Upgrades**: Via `dfu-util -e -a 1 -D <variant>.upgrade.bin` (requires factory firmware with DFU)
- **Development**: Via xTAG (`xflash` or `xrun` commands) for XCORE.AI evaluation boards
