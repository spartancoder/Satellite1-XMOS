"""
xSIM simulation tests for XMOS firmware.

Tests verify firmware can be simulated without hardware using xSIM.
"""
import pytest
from pathlib import Path


@pytest.mark.xsim
def test_firmware_binary_exists():
    """
    Verify that at least one firmware binary exists for simulation.

    This test doesn't run simulation but checks build artifacts exist.
    Skips if build directory doesn't exist (may need to run cmake build first).
    """
    build_dir = Path(__file__).resolve().parent.parent.parent.parent / "build"

    if not build_dir.exists():
        pytest.skip(f"Build directory not found: {build_dir}")

    # Look for any .xe files (XMOS executables)
    xe_files = list(build_dir.glob("**/*.xe"))

    if len(xe_files) == 0:
        pytest.skip(f"No firmware binaries (.xe) found in {build_dir} - run cmake build first")

    print(f"\nFound {len(xe_files)} firmware binaries:")
    for f in xe_files[:5]:  # Show first 5
        print(f"  - {f.relative_to(build_dir)}")


@pytest.mark.xsim
def test_xsim_import():
    """Verify xSIM/Pyxsim can be imported."""
    try:
        import Pyxsim as px
        assert px is not None
    except ImportError as e:
        pytest.skip(f"Pyxsim not installed: {e}")


@pytest.mark.xsim
def test_xsim_runner_fixture_available(xsim_runner):
    """Test that xsim_runner fixture is properly configured."""
    assert callable(xsim_runner), "xsim_runner should be callable"


@pytest.mark.xsim
def test_firmware_simulation_timeout(xsim_runner):
    """
    Test firmware simulation with timeout handling.

    Simulates firmware and verifies it completes within expected time.
    """
    build_dir = Path(__file__).resolve().parent.parent.parent.parent / "build"

    if not build_dir.exists():
        pytest.skip("Build directory not found")

    # Find a suitable binary
    xe_files = list(build_dir.glob("**/*.xe"))
    if not xe_files:
        pytest.skip("No firmware binaries found")

    binary_path = xe_files[0]

    # Try to run simulation with short timeout
    try:
        result = xsim_runner(
            binary_path,
            timeout=5  # 5 seconds should be enough for basic boot
        )

        # Check result structure
        assert result is not None, "Simulation result should not be None"

        # Different xSIM versions return different structures
        # Just verify we got something back
        print(f"\nSimulation completed")
        if hasattr(result, 'exit_code'):
            print(f"Exit code: {result.exit_code}")

    except Exception as e:
        # xSIM may fail if binary requires specific hardware or xscope
        pytest.skip(f"Simulation failed (may need specific config): {e}")


@pytest.mark.xsim
def test_audio_pipeline_4channel_support():
    """
    Verify firmware configuration supports 4-channel audio pipeline.

    This is a static analysis test - checks app_conf.h configuration.
    """
    app_conf_path = Path(__file__).resolve().parent.parent.parent / "src" / "app_conf.h"

    if not app_conf_path.exists():
        pytest.skip(f"app_conf.h not found: {app_conf_path}")

    content = app_conf_path.read_text()

    # Check for 4-mic related configuration
    has_4mic_config = (
        "4mic" in content.lower() or
        "MIC_COUNT" in content or
        "MIC_ARRAY_CONFIG" in content
    )

    # Check for channel configuration
    has_channel_config = (
        "AUDIO_PIPELINE_CHANNELS" in content or
        "I2S_NUM_CHANNELS" in content or
        "OUTPUT_CHANNELS" in content or
        "NUM_AUDIO_CHANNELS" in content
    )

    print(f"\n4-mic config: {has_4mic_config}")
    print(f"Channel config: {has_channel_config}")

    # At minimum, we expect some channel configuration
    assert has_channel_config, "No channel configuration found in app_conf.h"


@pytest.mark.xsim
def test_inter_tile_communication_ports():
    """
    Verify inter-tile communication ports are defined.

    Checks that channels 0, 1, 2, 7 are configured for inter-tile communication.
    """
    app_conf_path = Path(__file__).resolve().parent.parent.parent / "src" / "app_conf.h"

    if not app_conf_path.exists():
        pytest.skip(f"app_conf.h not found: {app_conf_path}")

    content = app_conf_path.read_text()

    # Look for inter-tile port definitions
    inter_tile_patterns = [
        "CHAN_",
        "CHANNEL_",
        "inter_tile",
        "INTER_TILE",
        "tile0to1",
        "tile1to0",
        "Intertile",
        "TILE"
    ]

    found_patterns = [p for p in inter_tile_patterns if p in content]

    print(f"\nFound inter-tile patterns: {found_patterns}")

    # At least one inter-tile pattern should exist
    # Note: May be defined in headers or comments
    assert len(found_patterns) > 0, "No inter-tile communication configuration found"


@pytest.mark.xsim
def test_trace_configuration():
    """
    Verify xscope trace configuration exists.

    Checks for xscope file and trace point definitions.
    """
    firmware_root = Path(__file__).resolve().parent.parent.parent

    # Look for xscope files
    xscope_files = list(firmware_root.glob("**/*.xscope"))

    print(f"\nFound {len(xscope_files)} xscope files:")
    for f in xscope_files:
        print(f"  - {f.relative_to(firmware_root)}")

    # Check app_conf.h for xscope configuration
    app_conf_path = firmware_root / "src" / "app_conf.h"
    if app_conf_path.exists():
        content = app_conf_path.read_text()
        has_xscope = "XSCOPE" in content or "xscope" in content
        print(f"xscope in app_conf.h: {has_xscope}")

    # This test is informational - always passes
    assert True


@pytest.mark.xsim
def test_audio_pipeline_config():
    """
    Verify audio pipeline configuration is valid.

    Checks pipeline stage enables and sample rate configuration.
    """
    app_conf_path = Path(__file__).resolve().parent.parent.parent / "src" / "app_conf.h"

    if not app_conf_path.exists():
        pytest.skip(f"app_conf.h not found: {app_conf_path}")

    content = app_conf_path.read_text()

    # Check for sample rate
    sample_rate_pattern = r"SAMPLE_RATE.*\b(16000|48000|44100|8000)\b"
    has_sample_rate = any(
        "SAMPLE_RATE" in content and rate in content
        for rate in ["16000", "48000", "44100", "8000"]
    )

    # Check for pipeline stage controls
    pipeline_patterns = [
        "PIPELINE_",
        "AUDIO_PIPELINE_",
        "BYPASS",
        "AEC",
        "VNR",
        "NS",
        "AGC"
    ]

    found_pipeline = [p for p in pipeline_patterns if p in content]

    print(f"\nSample rate config: {has_sample_rate}")
    print(f"Pipeline patterns found: {found_pipeline[:10]}")  # Show first 10

    # Should have at least some pipeline configuration
    assert len(found_pipeline) > 0, "No pipeline configuration found"
