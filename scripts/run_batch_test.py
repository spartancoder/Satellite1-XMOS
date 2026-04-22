#!/usr/bin/env python3
"""Run batch DSP processing on XK-VOICE-SQ66 via xscope_fileio.

Reads an input WAV, optionally remaps channels, the firmware processes it
through AEC -> IC -> NS -> AGC -> DOA, and writes output files back to the host.

Usage:
    # Build firmware first:
    # make -C build sq66_fileio_batch -j16

    # Run with hardware (default channel layout: ref_0, ref_1, mic_0..3):
    python scripts/run_batch_test.py --adapter-id <your_adapter_id>

    # 4-channel file where all channels are mics (no ref):
    python scripts/run_batch_test.py --adapter-id X --input recording.wav --mic-channels 0,1,2,3

    # Non-standard order (mics first, then refs):
    python scripts/run_batch_test.py --adapter-id X --input rec.wav --ref-channels 4,5 --mic-channels 0,1,2,3
"""

import argparse
import os
import struct
import sys
import wave

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

# Firmware expects: [ref_0, ref_1, mic_0, mic_1, mic_2, mic_3]
NUM_REF = 2
NUM_MIC = 4
OUTPUT_CHANNELS = NUM_REF + NUM_MIC  # 6


def parse_channel_list(s):
    """Parse a comma-separated list of 0-based channel indices."""
    if not s:
        return []
    return [int(x.strip()) for x in s.split(',')]


def remap_wav(src_path, dst_path, ref_channels, mic_channels):
    """Read src WAV, remap channels, write dst WAV in firmware layout.

    Output layout: [ref_0, ref_1, mic_0, mic_1, mic_2, mic_3]
    Channels not specified are zero-filled.
    """
    with wave.open(src_path, 'rb') as wf:
        n_channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        framerate = wf.getframerate()
        n_frames = wf.getnframes()
        raw = wf.readframes(n_frames)

    if sampwidth == 2:
        fmt = f'<{n_channels * n_frames}h'
        dtype = 'int16'
    elif sampwidth == 4:
        fmt = f'<{n_channels * n_frames}i'
        dtype = 'int32'
    else:
        print(f"ERROR: Unsupported sample width {sampwidth} bytes (need 2 or 4)")
        sys.exit(1)

    import numpy as np
    samples = np.array(struct.unpack(fmt, raw), dtype=dtype)
    samples = samples.reshape(n_frames, n_channels)

    # Build output: [ref_0, ref_1, mic_0, mic_1, mic_2, mic_3]
    out = np.zeros((n_frames, OUTPUT_CHANNELS), dtype=np.int32)

    # Map ref channels
    for dst_ch, src_ch in enumerate(ref_channels[:NUM_REF]):
        if src_ch < n_channels:
            out[:, dst_ch] = samples[:, src_ch].astype(np.int32)

    # Map mic channels
    for dst_ch, src_ch in enumerate(mic_channels[:NUM_MIC]):
        if src_ch < n_channels:
            out[:, NUM_REF + dst_ch] = samples[:, src_ch].astype(np.int32)

    # Scale 16-bit to 32-bit
    if sampwidth == 2:
        out = out << 16

    # Write as 32-bit PCM WAV using raw binary (matching firmware's wav_header_t)
    data_bytes = n_frames * OUTPUT_CHANNELS * 4
    byte_rate = framerate * OUTPUT_CHANNELS * 4
    sample_alignment = OUTPUT_CHANNELS * 4
    header = struct.pack('<4sI4s', b'RIFF', data_bytes + 36, b'WAVE')
    header += struct.pack('<4sIhhIIhh', b'fmt ', 16, 1, OUTPUT_CHANNELS,
                          framerate, byte_rate, sample_alignment, 32)
    header += struct.pack('<4sI', b'data', data_bytes)
    with open(dst_path, 'wb') as f:
        f.write(header)
        f.write(out.tobytes())

    # Report mapping
    mapped = []
    for dst_ch, src_ch in enumerate(ref_channels[:NUM_REF]):
        mapped.append(f"ref_{dst_ch}<-ch{src_ch}")
    for i in range(NUM_REF):
        if i >= len(ref_channels):
            mapped.append(f"ref_{i}<-zero")
    for dst_ch, src_ch in enumerate(mic_channels[:NUM_MIC]):
        mapped.append(f"mic_{dst_ch}<-ch{src_ch}")
    for i in range(NUM_MIC):
        if i >= len(mic_channels):
            mapped.append(f"mic_{i}<-zero")
    print(f"Channel map: {', '.join(mapped)}")
    print(f"Wrote {dst_path}: {OUTPUT_CHANNELS} channels, {framerate} Hz, 32-bit, {n_frames} frames")


def main():
    parser = argparse.ArgumentParser(
        description='Run batch DSP processing via xscope_fileio',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Channel mapping (firmware expects [ref_0, ref_1, mic_0, mic_1, mic_2, mic_3]):
  --ref-channels 0,1    Source channels for ref_0, ref_1 (0-based)
  --mic-channels 2,3,4,5  Source channels for mic_0..mic_3 (0-based)

Examples:
  # All-mic input (no reference), 4-channel WAV:
  --input recording.wav --mic-channels 0,1,2,3

  # Non-standard order (mics first, then refs):
  --input rec.wav --ref-channels 4,5 --mic-channels 0,1,2,3
""")
    parser.add_argument('--adapter-id', default=None,
                        help='JTAG adapter ID (required unless --xsim)')
    parser.add_argument('--xsim', action='store_true',
                        help='Use XMOS simulator instead of hardware')
    parser.add_argument('--firmware', default=FIRMWARE_XE,
                        help=f'Path to firmware .xe (default: {FIRMWARE_XE})')
    parser.add_argument('--input', default=DEFAULT_INPUT,
                        help=f'Input WAV filename on host (default: {DEFAULT_INPUT})')
    parser.add_argument('--ref-channels', default=None,
                        help='Comma-separated source channel indices for ref_0, ref_1 (0-based)')
    parser.add_argument('--mic-channels', default=None,
                        help='Comma-separated source channel indices for mic_0..mic_3 (0-based)')
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

    # Channel mapping
    ref_channels = parse_channel_list(args.ref_channels)
    mic_channels = parse_channel_list(args.mic_channels)
    needs_remap = bool(ref_channels or mic_channels)

    if needs_remap:
        remap_wav(args.input, DEFAULT_INPUT, ref_channels, mic_channels)
    else:
        # Default behavior: symlink if filename differs
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

    # Convert DOA binary to CSV
    import numpy as np
    for doa_bin, doa_csv, label in [
        ('output_doa.bin', 'output_doa.csv', 'DOA (AEC)'),
        ('output_doa_raw.bin', 'output_doa_raw.csv', 'DOA (raw mic)'),
    ]:
        if os.path.isfile(doa_bin):
            angles = np.fromfile(doa_bin, dtype=np.float32)
            with open(doa_csv, 'w') as f:
                f.write('frame,angle_rad,angle_deg\n')
                for i, a in enumerate(angles):
                    f.write(f'{i},{a:.6f},{np.degrees(a):.2f}\n')
            print(f"  {doa_csv} ({label}, {len(angles)} frames)")
        else:
            print(f"  {doa_bin} NOT FOUND")

    return ret


if __name__ == '__main__':
    sys.exit(main())
