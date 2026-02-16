---
phase: 01-foundation
plan: 01
subsystem: build-system
tags: cmake, firmware, xmos, beamformer, audio-pipeline

# Dependency graph
requires:
  - phase: none
    provides: none
provides:
  - satellite1_firmware_beamformer build target
  - beamformer_aec_ic_ns_agc_4mic_2ref CMake library
  - Foundation for 4-mic audio pipeline
affects: 01-02, 01-03, 01-04

# Tech tracking
tech-stack:
  added: none
  patterns:
    - XMOS multi-tile firmware variant creation
    - CMake INTERFACE library pattern for audio pipelines
    - foreach loop variant generation in satellite1.cmake

key-files:
  created:
    - satellite-xmos-firmware/audio_pipelines/reference/beamformer/ (copied from fixed_delay)
    - .planning/phases/01-foundation/01-01-VERIFICATION.md
  modified:
    - satellite-xmos-firmware/audio_pipelines/reference/CMakeLists.txt
    - satellite-xmos-firmware/firmware.cmake

key-decisions:
  - "Cloned fixed_delay as baseline instead of creating from scratch"
  - "Named beamformer library for 4-mic future (currently identical to 2-mic)"
  - "Added to both ENABLE_ALL_FFVA_PIPELINES and default pipeline lists"

patterns-established:
  - "Pattern 1: Firmware variant creation follows 3-step process: copy directory, add CMake library definition, add to FFVA_PIPELINES_INT"
  - "Pattern 2: Pipeline libraries follow naming convention: {variant}_aec_ic_ns_agc_{mic_count}mic_{ref_count}ref"
  - "Pattern 3: Alias naming follows fph::ffva::ap::{variant} for downstream consumption"

# Metrics
duration: 8min
completed: 2026-02-16
---

# Phase 1 Plan 01 Summary

**satellite1_firmware_beamformer variant created from fixed_delay baseline with identical 2-mic AEC+IC+NS+AGC pipeline, ready for 4-mic extension**

## Performance

- **Duration:** 8 min
- **Started:** 2026-02-16T01:40:55Z
- **Completed:** 2026-02-16T01:48:32Z
- **Tasks:** 4
- **Files modified:** 2

## Accomplishments

- Created beamformer audio pipeline directory cloned from fixed_delay baseline
- Added beamformer_aec_ic_ns_agc_4mic_2ref CMake INTERFACE library with proper dependencies
- Registered beamformer in firmware.cmake for automatic variant generation
- Verified build produces identical warnings (23) and memory usage to fixed_delay
- Successfully generated satellite1_firmware_beamformer.xe (7095240 bytes)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create beamformer pipeline directory and CMake definitions** - `7e5b4c1` (feat)
2. **Task 2: Add beamformer pipeline to CMakeLists.txt** - (Included in Task 1)
3. **Task 3: Add beamformer to firmware variant targets** - `12f2393` (feat)
4. **Task 4: Verify build** - `4f8f298` (test)

**Plan metadata:** `docs(01-01): complete clone beamformer variant plan`

## Files Created/Modified

- `satellite-xmos-firmware/audio_pipelines/reference/beamformer/` - Copied from fixed_delay, contains 4 C source files and 2 headers for 2-mic audio pipeline
- `satellite-xmos-firmware/audio_pipelines/reference/CMakeLists.txt` - Added beamformer library definition and fph::ffva::ap::beamformer alias
- `satellite-xmos-firmware/firmware.cmake` - Added beamformer to FFVA_PIPELINES_INT lists (both ENABLE_ALL and default)
- `.planning/phases/01-foundation/01-01-VERIFICATION.md` - Build test results with warning comparison

## Decisions Made

- Cloned fixed_delay baseline instead of creating from scratch - ensures all existing functionality works, provides known good starting point
- Named library beamformer_aec_ic_ns_agc_4mic_2ref for 4-mic future - clarifies intent while currently identical to 2-mic implementation
- Added to both ENABLE_ALL_FFVA_PIPELINES and default pipeline lists - ensures beamformer is available in all build configurations

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - all tasks completed without issues. Build verification showed identical warnings between fixed_delay and beamformer, confirming the clone was successful.

## User Setup Required

None - no external service configuration required. XMOS XTC 15.3.1 tools are required for building but are project infrastructure.

## Next Phase Readiness

Beamformer variant is ready for 4-mic configuration work. Plan 01-02 can proceed to add DOA (Direction of Arrival) library and modify pipeline for 4 microphone inputs. The beamformer variant compiles with identical behavior to fixed_delay, providing a safe foundation for parallel development.

## Self-Check: PASSED

Files verified:
- .planning/phases/01-foundation/01-01-SUMMARY.md - CREATED
- satellite-xmos-firmware/audio_pipelines/reference/beamformer/ - CREATED

Commits verified:
- 7e5b4c1 - feat(01-01): create beamformer pipeline directory and CMake definitions
- 12f2393 - feat(01-01): add beamformer to firmware variant targets
- 4f8f298 - test(01-01): verify beamformer build matches fixed_delay baseline

---
*Phase: 01-foundation*
*Completed: 2026-02-16*
