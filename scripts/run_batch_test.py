#!/usr/bin/env python3
"""Run batch DSP processing on XK-VOICE-SQ66 via xscope_fileio.

Reads input.wav from the host filesystem, the firmware processes it through
AEC -> IC -> NS -> AGC, and writes output WAVs back to the host.

Usage:
    # Build firmware first:
    # make -C build sq66_fileio_batch -j16

    # Run with hardware:
    python scripts/run_batch_test.py --adapter-id <your_adapter_id>

    # Run with simulator:
    python scripts/run_batch_test.py --xsim

    # Specify input file (must be in CWD or use --input):
    python scripts/run_batch_test.py --adapter-id <id> --input test_4ch.wav
"""

import argparse
import os
import sys
import subprocess

# Add xscope_fileio module to path
_script_dir = os.path.dirname(os.path.abspath(__file__))
_ws_root = os.path.dirname(_script_dir)
_xscope_fileio_path = os.path.join(_ws_root, 'modules', 'xscope_fileio', 'xscope_fileio')

if os.path.isdir(_xscope_fileio_path):
    sys.path.insert(0, _xscope_fileio_path)
else:
    print(f"ERROR: xscope_fileio module not found at {_xscope_fileio_path}")
    sys.exit(1)

try:
    from xscope_fileio import run_on_target
except ImportError:
    print("ERROR: Cannot import xscope_fileio. Ensure XMOS_TOOL_PATH is set.")
    sys.exit(1)


FIRMWARE_XE = os.path.join(_ws_root, 'build', 'sq66_fileio_batch.xe')
DEFAULT_INPUT = 'input.wav'


def main():
    parser = argparse.ArgumentParser(description='Run batch DSP processing via xscope_fileio')
    parser.add_argument('--adapter-id', default=None,
                        help='JTAG adapter ID (required unless --xsim)')
    parser.add_argument('--xsim', action='store_true',
                        help='Use XMOS simulator instead of hardware')
    parser.add_argument('--firmware', default=FIRMWARE_XE,
                        help=f'Path to firmware .xe (default: {FIRMWARE_XE})')
    parser.add_argument('--input', default=DEFAULT_INPUT,
                        help=f'Input WAV filename on host (default: {DEFAULT_INPUT})')
    args = parser.parse_args()

    if not args.xsim and args.adapter_id is None:
        parser.error('--adapter-id is required when not using --xsim')

    # Check firmware exists
    if not os.path.isfile(args.firmware):
        print(f"ERROR: Firmware not found: {args.firmware}")
        print("Build with: make -C build sq66_fileio_batch -j16")
        sys.exit(1)

    # Check input file exists
    if not os.path.isfile(args.input):
        print(f"ERROR: Input file not found: {args.input}")
        print("Place a 4-6 channel, 32-bit PCM, 16kHz WAV file in the working directory.")
        sys.exit(1)

    # The firmware reads "input.wav" by name. If the user specified a different
    # file, we need to symlink or copy it.
    input_basename = os.path.basename(args.input)
    if input_basename != DEFAULT_INPUT:
        if os.path.exists(DEFAULT_INPUT):
            os.remove(DEFAULT_INPUT)
        os.symlink(os.path.abspath(args.input), DEFAULT_INPUT)
        print(f"Linked {args.input} -> {DEFAULT_INPUT}")

    print(f"Firmware: {args.firmware}")
    print(f"Input:    {args.input}")
    print(f"Mode:     {'xsim' if args.xsim else 'hardware (' + args.adapter_id + ')'}")
    print()

    ret = run_on_target(args.adapter_id or '', args.firmware, use_xsim=args.xsim)

    # List output files
    print("\nOutput files:")
    for name in ['output_aec.wav', 'output_ic.wav', 'output_ns.wav', 'output_agc.wav']:
        path = os.path.join('.', name)
        if os.path.isfile(path):
            size = os.path.getsize(path)
            print(f"  {name} ({size:,} bytes)")
        else:
            print(f"  {name} NOT FOUND")

    return ret


if __name__ == '__main__':
    sys.exit(main())
