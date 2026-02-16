# 01-01: Build Verification

## Build Test Results

### Configuration
- XMOS XTC version: 15.3.1
- CMake version: 3.25.1
- Python: 3.10.19
- Build config: USE_DEV_TRACKING=OFF
- Build directory: build_no_dev

### Build Results

#### fixed_delay Baseline
- Status: PASSED
- Warnings: 23
- Output: satellite1_firmware_fixed_delay.xe (7095252 bytes)
- Memory usage tile[0]: 410716+ bytes (Stack: 3828+, Code: 101280, Data: 305608)
- Memory usage tile[1]: 17532 bytes (Stack: 348, Code: 4156, Data: 13028)
- Constraints: PASSED WITH CAVEATS (tile[0]), PASSED (tile[1])

#### beamformer Variant
- Status: PASSED
- Warnings: 23 (identical to fixed_delay)
- Output: satellite1_firmware_beamformer.xe (7095240 bytes)
- Memory usage tile[0]: 410716+ bytes (Stack: 3828+, Code: 101280, Data: 305608)
- Memory usage tile[1]: 17532 bytes (Stack: 348, Code: 4156, Data: 13028)
- Constraints: PASSED WITH CAVEATS (tile[0]), PASSED (tile[1])

### Warning Comparison

Both builds produce identical warnings:
1. USB driver callbacks (7 warnings)
   - tud_descriptor_device_qualifier_cb
   - tud_descriptor_other_speed_configuration_cb
   - tud_mount_cb
   - tud_resume_cb
   - tud_suspend_cb
   - tud_umount_cb
   - usbd_app_driver_get_cb

2. USB device callbacks (2 warnings)
   - tud_xcore_data_cb
   - tud_xcore_sof_cb

3. Port mapping warnings (14 warnings)
   - XS1_PORT_1F, XS1_PORT_1E, XS1_PORT_1H, XS1_PORT_1J, XS1_PORT_1K, XS1_PORT_1I, XS1_PORT_8B
   - (Each port appears twice, once for each tile)

### CMake Targets Available

The following targets were created for beamformer:
- satellite1_firmware_beamformer (main firmware)
- tile0_satellite1_firmware_beamformer (tile 0 binary)
- tile1_satellite1_firmware_beamformer (tile 1 binary)
- satellite1_firmware_beamformer_versioning (version header generation)
- satellite1_firmware_beamformer_fat.fs (filesystem image)
- create_flash_img_satellite1_firmware_beamformer (factory image)
- create_upgrade_img_satellite1_firmware_beamformer (upgrade image)
- flash_app_satellite1_firmware_beamformer (flash target)
- make_data_partition_satellite1_firmware_beamformer (data partition)
- run_satellite1_firmware_beamformer (run target)
- debug_satellite1_firmware_beamformer (debug target)

### Conclusion

- New variant builds successfully
- Build produces no errors
- No new warnings compared to fixed_delay baseline
- Firmware image size matches baseline within 12 bytes (likely version string)
- Memory usage identical to baseline
- Ready for 4-mic configuration work in subsequent plans
