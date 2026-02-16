---
id: 01-04
phase: 01-foundation
plan: 04
type: module-structure
subsystem: spatial-audio
tags: [modules, build-system, tdd-infra]
autonomous: true
wave: 4
completed-date: 2026-02-16
duration: 4min 34s
---

# Phase 01 Plan 04: Establish Module Directory Structure

## Summary

Created complete directory structure for four new XMOS voice library modules (lib_doa, lib_dtoa, lib_beamforming, lib_postproc) following established patterns from lib_aec. Established API stubs, placeholder implementations, test infrastructure with Unity framework, and documentation for all modules. Registered modules in the voice build system for Phase 2-4 implementation.

## One-Liner

Established module directory structure for lib_doa, lib_dtoa, lib_beamforming, and lib_postproc with XMOS voice library patterns including API headers, placeholder implementations, Unity test infrastructure, and build system integration.

## Key Files Created/Modified

### Modules Created (in voice submodule)
- `/workspace/modules/voice/modules/lib_doa/` - DOA estimation module structure
- `/workspace/modules/voice/modules/lib_dtoa/` - TDOA calculation module structure
- `/workspace/modules/voice/modules/lib_beamforming/` - Beamforming module structure
- `/workspace/modules/voice/modules/lib_postproc/` - Post-processing module structure

### Key Files per Module

**lib_doa (12 files):**
- `api/doa_api.h` - Public API: doa_init(), doa_process_frame(), doa_get_azimuth()
- `api/doa_defines.h` - Constants: DOA_NUM_MICS (4), DOA_FRAME_SIZE (240)
- `api/doa_state.h` - State structures: doa_state_t, doa_algorithm_t enum
- `src/doa_impl.c` - Placeholder implementation
- `CMakeLists.txt` - Build configuration
- Tests: test_doa_init.c, test_dtoa_process.c
- Doc: README.md with API usage examples

**lib_dtoa (12 files):**
- `api/dtoa_api.h` - Public API: dtoa_init(), dtoa_process_frame(), dtoa_get_delay()
- `api/dtoa_defines.h` - Constants: DTOA_NUM_MICS (4), DTOA_NUM_PAIRS (6)
- `api/dtoa_state.h` - State structures: dtoa_state_t, dtoa_pair_delay_t
- `src/dtoa_impl.c` - Placeholder implementation
- `CMakeLists.txt` - Build configuration
- Tests: test_dtoa_init.c, test_dtoa_process.c
- Doc: README.md with API usage examples

**lib_beamforming (12 files):**
- `api/beamforming_api.h` - Public API: beamformer_init(), beamformer_add_beam(), beamformer_process_frame()
- `api/beamforming_defines.h` - Constants: BF_NUM_MICS (4), BF_MAX_BEAMS (8)
- `api/beamforming_state.h` - State structures: bf_state_t, bf_beam_t
- `src/beamforming_impl.c` - Placeholder implementation
- `CMakeLists.txt` - Build configuration
- Tests: test_beamformer_init.c, test_beamformer_process.c
- Doc: README.md with API usage examples

**lib_postproc (12 files):**
- `api/postproc_api.h` - Public API: postproc_init(), postproc_process_frame(), postproc_set_mode()
- `api/postproc_defines.h` - Constants: POSTPROC_MAX_CHANNELS (2)
- `api/postproc_state.h` - State structures: postproc_state_t, postproc_mode_t enum
- `src/postproc_impl.c` - Placeholder implementation
- `CMakeLists.txt` - Build configuration
- Tests: test_postproc_init.c, test_postproc_process.c
- Doc: README.md with API usage examples

### Build System Updates
- `/workspace/modules/voice/modules/CMakeLists.txt` - Added 4 new modules
- `/workspace/modules/voice/test/CMakeLists.txt` - Added 4 test directories

## Decisions Made

### Module Structure Pattern
- Followed lib_aec pattern exactly for consistency
- Each module has: api/, src/, doc/ subdirectories
- CMakeLists.txt follows lib_aec template (static library, xcore_math dependency)
- Public API in api/ directory, private headers in src/

### API Design Decisions
- All modules use 240-sample frames (15ms at 16kHz) for consistency
- DOA output: azimuth in degrees (0-360) with confidence (0-1000)
- TDOA output: delays in samples for all 6 mic pairs with confidence
- Beamforming supports up to 8 concurrent beams with configurable direction/width
- Postproc supports 3 modes: pass-through, voice-enhance, full

### Test Infrastructure
- Unity test framework for all modules
- Auto-generated test runners via Python scripts
- Stub tests for Phase 1 (placeholder APIs)
- Tests verify null safety and basic initialization

### Module Dependencies
- lib_doa depends on lib_xcore_math
- lib_dtoa depends on lib_xcore_math
- lib_beamforming depends on lib_xcore_math
- lib_postproc depends on lib_xcore_math

## Dependency Graph

### Requires
- None (standalone modules)

### Provides
- lib_doa: DOA estimation APIs for Phase 2
- lib_dtoa: TDOA calculation APIs for Phase 2
- lib_beamforming: Beamforming APIs for Phase 3
- lib_postproc: Post-processing APIs for Phase 4

### Affects
- Phase 2: lib_dtoa used by lib_doa for GCC-PHAT implementation
- Phase 3: lib_doa provides direction to lib_beamforming
- Phase 3: lib_beamforming output fed to lib_postproc
- Phase 4: lib_postproc enhances beamformer output

## Tech Stack

### Added
- **Patterns**: XMOS voice library module structure (api/, src/, doc/)
- **Build**: CMake static library pattern with xcore_math linkage
- **Test**: Unity framework with auto-generated runners
- **Doc**: Markdown README files with API usage examples

### Patterns Established
- Module CMakeLists.txt: STATIC library, PUBLIC api/ includes, xcore_math linkage
- Aliases: fwk_voice::<module_name> for dependency resolution
- Test structure: unit/src/ with generate_unity_runner.py scripts
- API headers: comprehensive Doxygen-style documentation

## Deviations from Plan

### Auto-fixed Issues

None - plan executed exactly as written.

## Auth Gates

None encountered.

## Metrics

- **Duration**: 4 minutes 34 seconds
- **Tasks Completed**: 8 implementation steps
- **Files Created**: 48 files (12 files per module)
- **Commits**: 5 commits (4 modules + 1 build system update)
- **Modules Added**: 4 (lib_doa, lib_dtoa, lib_beamforming, lib_postproc)

## Implementation Phase Status

### Phase 1: Foundation (This Plan)
- API stubs: Complete
- Test infrastructure: Complete
- Module structure: Complete
- Build system integration: Complete

### Phase 2: DOA Implementation (Next)
- lib_dtoa GCC-PHAT: Pending
- lib_doa GCC-PHAT: Pending
- Integration: Pending

### Phase 3: Beamforming (Future)
- Delay-sum beamformer: Pending
- MVDR beamformer: Pending

### Phase 4: Post-Processing (Future)
- Voice enhancement: Pending
- Noise reduction: Pending

## Verification Performed

### Module Structure Verification
- Verified all 4 modules created with api/, src/, doc/ directories
- Verified CMakeLists.txt follows lib_aec pattern
- Verified API headers contain proper documentation

### Build System Verification
- Verified modules/CMakeLists.txt includes all 4 new modules
- Verified test/CMakeLists.txt includes all 4 test directories

### Test Infrastructure Verification
- Verified Unity test structure matches lib_aec pattern
- Verified generate_unity_runner.py scripts created and executable

### Commit Verification
- 5 commits created in voice submodule
- 1 commit created in main repository for submodule update
- All commits follow conventional commit format

## Self-Check: PASSED

### Files Created Verification
```
[ -d "modules/voice/modules/lib_doa/api" ] && echo "FOUND: lib_doa/api"
[ -d "modules/voice/modules/lib_dtoa/api" ] && echo "FOUND: lib_dtoa/api"
[ -d "modules/voice/modules/lib_beamforming/api" ] && echo "FOUND: lib_beamforming/api"
[ -d "modules/voice/modules/lib_postproc/api" ] && echo "FOUND: lib_postproc/api"
```

### Commits Verification
```
git log --oneline --all | grep -q "ba00e17" && echo "FOUND: ba00e17 (main repo)"
```

### Files Created
- 48 module files (API headers, implementations, tests, docs)
- 4 CMakeLists.txt files
- 4 generate_unity_runner.py scripts
- 4 README.md documentation files
- 2 build system configuration files

### Commits Created
- `ba00e17`: feat(01-04): update voice submodule with new modules (main repo)
- Submodule commits (5 total in voice submodule):
  - `46fb6c42`: feat(01-04): register new modules in build system
  - `d446b51e`: feat(01-04): add lib_postproc module structure
  - `8eee6cc8`: feat(01-04): add lib_beamforming module structure
  - `512ca01b`: feat(01-04): add lib_dtoa module structure
  - `a18d92c1`: feat(01-04): add lib_doa module structure

## Next Steps

1. **Phase 2 Plan 01**: Implement lib_dtoa GCC-PHAT algorithm
2. **Phase 2 Plan 02**: Implement lib_doa GCC-PHAT algorithm using lib_dtoa
3. **Phase 2 Plan 03**: Integrate DOA into bypass_4mic variant
4. **Phase 2 Plan 04**: Test DOA with synthetic audio data

## Notes

- All modules are ready for implementation in subsequent phases
- Module structure allows for independent development
- Test infrastructure enables TDD approach for algorithm implementation
- Documentation provides clear API usage examples for integration
