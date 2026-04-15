#!/usr/bin/env python3
"""xSCOPE 4-mic recording script for XK-VOICE-SQ66.

Records audio observation points streamed from the firmware via xTAG4.
Uses mic_array.xscope.Endpoint for transport.

Usage:
    # Start firmware first:
    xrun --xscope-port localhost:10234 <firmware.xe>

    # Record all observation points for 5 seconds:
    python test_xscope_recorder.py --port 10234 --duration 5

    # Record only raw and gain mics:
    python test_xscope_recorder.py --port 10234 --probes mic_raw mic_gain
"""

import argparse
import os
import sys
import time
import signal
import struct
import threading
from collections import defaultdict

import numpy as np
from scipy.io import wavfile

# Add mic_array module to path
sys.path.insert(0, os.path.join(
    os.environ.get('XMOS_TOOL_PATH', ''), '..', 'workspace',
    'modules', 'io', 'modules', 'mic_array', 'script'
))

try:
    from mic_array.xscope import Endpoint
except ImportError:
    print("ERROR: Cannot import mic_array.xscope. Ensure XMOS_TOOL_PATH is set.")
    print("       The mic_array module is at modules/io/modules/mic_array/script/")
    sys.exit(1)


# Probe names matching config-xscope-4mic.xscope
AUDIO_PROBES = [
    'mic_raw_0', 'mic_raw_1', 'mic_raw_2', 'mic_raw_3',
    'mic_gain_0', 'mic_gain_1', 'mic_gain_2', 'mic_gain_3',
    'mic_aec_0', 'mic_aec_1', 'mic_aec_2', 'mic_aec_3',
    'aec_residual_0', 'aec_residual_1', 'aec_residual_2', 'aec_residual_3',
    'ic_out_0', 'ic_out_1',
    'ic_residual_0', 'ic_residual_1',
    'ns_out_0', 'ns_out_1',
    'agc_out_0', 'agc_out_1',
    'beam_0', 'beam_1', 'beam_2',
]

METADATA_PROBES = [
    'vnr_value', 'agc_gain',
    'doa_angle', 'doa_angle_raw', 'doa_confidence',
    'vad_beam_0', 'vad_beam_1', 'vad_beam_2',
    'beam_selection', 'input_source',
]

FRAME_ADVANCE = 240
SAMPLE_RATE = 16000
BYTES_PER_SAMPLE = 4
XSCOPE_DATA_TYPE_FLOAT = 3  # Matches XSCOPE_FLOAT in xscope.h


def parse_args():
    parser = argparse.ArgumentParser(description='xSCOPE 4-mic recorder')
    parser.add_argument('--host', default='localhost', help='xSCOPE server hostname')
    parser.add_argument('--port', default='10234', help='xSCOPE server port')
    parser.add_argument('--duration', type=float, default=5.0, help='Recording duration in seconds')
    parser.add_argument('--output-dir', default='recordings', help='Output directory for WAV files')
    parser.add_argument('--probes', nargs='*', default=None,
                        help='Probe name prefixes to record (e.g. mic_raw mic_gain). Records all if omitted.')
    return parser.parse_args()


class XscopeRecorder(Endpoint):
    """Extended Endpoint that captures xscope_bytes audio data."""

    def __init__(self, probe_filter=None):
        super().__init__()
        self._audio_buffers = defaultdict(list)
        self._metadata_values = defaultdict(list)
        self._metadata_timestamps = defaultdict(list)
        self._lock = threading.Lock()
        self._start_time = None
        self._probe_filter = probe_filter  # list of prefixes, or None for all

    def _should_capture(self, probe_name):
        if self._probe_filter is None:
            return True
        return any(probe_name.startswith(prefix) for prefix in self._probe_filter)

    def on_record(self, id_, timestamp, length, data_val, data_bytes):
        probe_info = self._probe_info.get(id_)
        if probe_info is None:
            return

        probe_name = probe_info['name']

        if not self._should_capture(probe_name):
            return

        if self._start_time is None:
            self._start_time = time.time()

        if probe_name in METADATA_PROBES:
            # Scalar value from xscope_float() or xscope_int()
            if probe_info.get('data_type') == XSCOPE_DATA_TYPE_FLOAT:
                val = struct.unpack('f', struct.pack('I', data_val & 0xFFFFFFFF))[0]
            else:
                val = int(data_val & 0xFFFFFFFF)
                if val >= 0x80000000:
                    val -= 0x100000000

            with self._lock:
                self._metadata_values[probe_name].append(val)
                self._metadata_timestamps[probe_name].append(timestamp)
        else:
            # Byte array from xscope_bytes()
            num_samples = length // BYTES_PER_SAMPLE
            if num_samples > 0 and data_bytes:
                samples = np.frombuffer(data_bytes[:length], dtype=np.int32)
                with self._lock:
                    self._audio_buffers[probe_name].append(samples.copy())

    def get_audio_data(self, probe_name):
        """Get accumulated audio samples for a probe as a numpy array."""
        with self._lock:
            chunks = self._audio_buffers.get(probe_name, [])
            if not chunks:
                return np.array([], dtype=np.int32)
            return np.concatenate(chunks)

    def get_metadata(self, probe_name):
        """Get accumulated metadata values for a probe."""
        with self._lock:
            return list(self._metadata_values.get(probe_name, []))

    def elapsed(self):
        if self._start_time is None:
            return 0.0
        return time.time() - self._start_time


def group_probes_by_prefix(probe_names):
    """Group probe names by their prefix (e.g. mic_raw_0..3 -> mic_raw)."""
    groups = defaultdict(list)
    for name in probe_names:
        parts = name.rsplit('_', 1)
        if len(parts) == 2 and parts[1].isdigit():
            prefix = parts[0]
        else:
            prefix = name
        groups[prefix].append(name)
    return dict(groups)


def write_audio_wav(output_dir, group_name, probe_names, recorder):
    """Write a multi-channel WAV file for a group of audio probes."""
    channels = []
    max_len = 0
    for name in sorted(probe_names):
        data = recorder.get_audio_data(name)
        if len(data) > max_len:
            max_len = len(data)
        channels.append(data)

    if max_len == 0:
        print(f"  No data for {group_name}, skipping")
        return

    # Pad shorter channels with zeros
    padded = []
    for ch in channels:
        if len(ch) < max_len:
            padded.append(np.pad(ch, (0, max_len - len(ch))))
        else:
            padded.append(ch)

    # Interleave channels: shape (max_len, num_channels)
    interleaved = np.column_stack(padded)

    # Normalize int32 to int16 for WAV
    wav_data = (interleaved >> 16).astype(np.int16)

    filepath = os.path.join(output_dir, f"{group_name}.wav")
    wavfile.write(filepath, SAMPLE_RATE, wav_data)
    print(f"  Written {filepath} ({max_len} samples, {len(padded)} channels, "
          f"{max_len / SAMPLE_RATE:.2f}s)")


def write_metadata_csv(output_dir, probe_name, recorder):
    """Write metadata values to a CSV file."""
    values = recorder.get_metadata(probe_name)
    if not values:
        return

    filepath = os.path.join(output_dir, f"{probe_name}.csv")
    with open(filepath, 'w') as f:
        if probe_name == 'beam_selection':
            # Unpack bit-packed beam_selection: beam = bits 0-15, criteria = bits 16-31
            f.write("frame,selected_beam,criteria\n")
            for i, val in enumerate(values):
                if isinstance(val, int):
                    beam = val & 0xFFFF
                    criteria = (val >> 16) & 0xFFFF
                    f.write(f"{i},{beam},{criteria}\n")
                else:
                    f.write(f"{i},{val},\n")
        else:
            f.write("frame,value\n")
            for i, val in enumerate(values):
                f.write(f"{i},{val}\n")
    print(f"  Written {filepath} ({len(values)} values)")


def main():
    args = parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    print(f"Connecting to xSCOPE at {args.host}:{args.port}...")
    recorder = XscopeRecorder(probe_filter=args.probes)

    if recorder.connect(args.host, args.port):
        print("ERROR: Failed to connect")
        sys.exit(1)

    print("Connected. Recording...")
    print(f"  Duration: {args.duration}s")
    print(f"  Probe filter: {args.probes or 'all'}")
    print("  Press Ctrl+C to stop early")

    try:
        time.sleep(args.duration)
    except KeyboardInterrupt:
        print("\nStopped by user")
    finally:
        elapsed = recorder.elapsed()
        print(f"\nRecording complete ({elapsed:.2f}s)")
        print("Saving...")

        # Save audio probes
        captured_audio = [p for p in AUDIO_PROBES if recorder.get_audio_data(p).size > 0]
        groups = group_probes_by_prefix(captured_audio)
        for group_name, probe_names in sorted(groups.items()):
            write_audio_wav(args.output_dir, group_name, probe_names, recorder)

        # Save metadata
        for probe_name in METADATA_PROBES:
            values = recorder.get_metadata(probe_name)
            if values:
                write_metadata_csv(args.output_dir, probe_name, recorder)

        recorder.disconnect()
        print(f"\nOutput saved to {args.output_dir}/")


if __name__ == '__main__':
    main()
