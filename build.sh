#!/bin/bash
BUILD_DIR=build
XMOS_TOOL_PATH=/opt/xmos/XTC/15.3.1

# Source Python environment
source .venv/bin/activate
source $XMOS_TOOL_PATH/SetEnv

# Configure CMake
cmake -B $BUILD_DIR -DUSE_DEV_TRACKING=ON --toolchain xmos_cmake_toolchain/xs3a.cmake

# Build
cd $BUILD_DIR
#make satellite1_firmware_beamformer -j12
make create_upgrade_img_satellite1_firmware_beamformer -j12
make create_upgrade_img_satellite1_firmware_fixed_delay -j12
