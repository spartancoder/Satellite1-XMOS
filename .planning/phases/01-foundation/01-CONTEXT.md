# Phase 1: Foundation - Context

**Gathered:** 2026-02-15
**Status:** Ready for planning

## Phase Boundary

Establish new firmware variant `satellite1_firmware_beamformer`, enable 4-mic PDM capture, create testing infrastructure (xSIM + synthetic tests), and establish module directory structure following XMOS voice library patterns. This phase creates the scaffolding that enables future module development (DOA, DTOA, beamforming, post-processing) without modifying external XMOS framework submodules.

## Implementation Decisions

### Module Location
- New modules (lib_doa, lib_dtoa, lib_beamforming, lib_postproc) live in `/workspace/modules/` alongside existing XMOS framework modules
- Each module follows XMOS voice library structure: `api/`, `src/`, `tests/`, `doc/`
- XMOS framework submodules in `/workspace/modules/` are external git submodules and must NOT be modified
- Modules referenced from firmware via CMakeLists.txt dependency links
- Modules structured for eventual extraction to separate git repos (clean boundaries, minimal external dependencies)

### Test Framework
- Hybrid approach: Unity framework for C unit tests (already used by fwk_voice), pytest for synthetic scenarios
- xSIM data trace extraction enabled via `xsim -t` flag for offline analysis
- xSIM traces integrated as pytest artifacts for organized test output
- Synthetic audio generation uses pyroomacoustics Python library

### Test Scenarios
- Full spectrum of synthetic audio scenarios for comprehensive coverage:
  - Controlled speech patterns from different angles and elevations
  - Multi-source scenarios with multiple sound sources
  - Ambient noise and reflections
  - Single speaker, single noise source (clean baseline)

### Test Data Format
- Both raw PDM samples and processed frames for different analysis needs
- Raw PDM: captures microphone inputs before any processing
- Processed frames: audio frames ready for DOA algorithm input
- Supports both algorithm development and integration testing

### Test Validation
- Multi-method validation approach for confidence:
  - Tolerance check: pass if within X degrees of expected azimuth/elevation
  - Golden reference: compare against known-good output vectors
  - State validation: assert internal states (covariance matrix, DOA estimate valid)

### Test Data Storage
- Per-module storage: `tests/data/` within each module (e.g., `modules/lib_doa/tests/data/`)
- Keeps test data co-located with module code
- Supports module extraction to separate repos without breaking tests

### xSIM Trace Organization
- xSIM trace files organized as pytest artifacts
- Integrated into pytest output workflow for consistency
- Traces include both raw and processed formats

### Claude's Discretion
- Exact pytest configuration and file naming patterns
- Unity test runner setup and assertion macros
- pyroomacoustics simulation parameters (room size, mic positions, source locations)
- xSIM trace format details (binary vs JSON, sampling rate)
- Test execution automation (CI integration, result reporting)

## Specific Ideas

- "XMOS framework modules are submodules - don't break them"
- "Use pyroomacoustics for room simulation, it's the standard in the research"
- "Modules should be extractable to separate repos later"
- "Both raw and processed test data for flexibility"

## Deferred Ideas

None — discussion stayed within phase scope.

---
*Phase: 01-foundation*
*Context gathered: 2026-02-15*
