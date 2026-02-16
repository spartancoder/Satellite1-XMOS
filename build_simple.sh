cmake -B build -DUSE_DEV_TRACKING=ON --toolchain xmos_cmake_toolchain/xs3a.cmake
make create_upgrade_img_satellite1_firmware_fixed_delay -j12
