#!/usr/bin/env python3
"""Quick validation that xscope probes are streaming.

Usage:
    # Terminal 1: Start firmware with xscope
    xrun --xscope-port localhost:10234 <firmware.xe>

    # Terminal 2: Run this script
    python scripts/test_xscope_connection.py
"""

import os, sys, time, struct, threading
from collections import defaultdict

# Find mic_array module relative to this script's location
_script_dir = os.path.dirname(os.path.abspath(__file__))
_ws_root = os.path.dirname(_script_dir)  # scripts/ -> workspace/
_mic_array_path = os.path.join(_ws_root, 'modules', 'io', 'modules', 'mic_array', 'script')
if os.path.isdir(_mic_array_path):
    sys.path.insert(0, _mic_array_path)
else:
    print(f"ERROR: mic_array module not found at {_mic_array_path}")
    sys.exit(1)

from mic_array.xscope import Endpoint

SAMPLE_RATE = 16000
FRAME_ADVANCE = 240
TIMEOUT = 10  # seconds to wait for data

class ProbeWatcher(Endpoint):
    def __init__(self):
        super().__init__()
        self._probe_counts = defaultdict(int)
        self._probe_bytes = defaultdict(int)
        self._first_data = defaultdict(lambda: None)
        self._last_data = defaultdict(lambda: None)
        self._lock = threading.Lock()
        self._start = None
        self._got_any = threading.Event()

    def on_record(self, id_, timestamp, length, data_val, data_bytes):
        info = self._probe_info.get(id_)
        if info is None:
            return

        name = info['name']
        with self._lock:
            self._probe_counts[name] += 1
            self._probe_bytes[name] += length
            now = time.time()
            if self._first_data[name] is None:
                self._first_data[name] = now
                self._last_data[name] = now
                print(f"  FIRST: {name} (id={id_}, len={length})", flush=True)
            else:
                self._last_data[name] = now
            self._got_any.set()

    def report(self, elapsed):
        with self._lock:
            if not self._probe_counts:
                print("\nNo probe data received!")
                return

            print(f"\n{'Probe':<25} {'Frames':>8} {'Bytes':>10} {'Rate (fps)':>12}")
            print("-" * 60)
            for name in sorted(self._probe_counts):
                count = self._probe_counts[name]
                total_bytes = self._probe_bytes[name]
                fps = count / elapsed if elapsed > 0 else 0
                print(f"{name:<25} {count:>8} {total_bytes:>10} {fps:>12.1f}")

            # Expected frame rate: 16000 / 240 = ~66.7 fps
            expected_fps = SAMPLE_RATE / FRAME_ADVANCE
            audio_probes = [n for n in self._probe_counts
                           if any(n.startswith(p) for p in
                                  ['mic_raw', 'mic_gain', 'mic_aec', 'ic_out', 'ns_out', 'agc_out'])]
            if audio_probes:
                sample = audio_probes[0]
                actual_fps = self._probe_counts[sample] / elapsed
                print(f"\nExpected frame rate: {expected_fps:.1f} fps")
                print(f"Actual ({sample}):   {actual_fps:.1f} fps")
                if abs(actual_fps - expected_fps) < 5:
                    print("PASS: Frame rate matches expected ~66.7 fps")
                else:
                    print(f"WARNING: Frame rate off by {abs(actual_fps - expected_fps):.1f} fps")


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else 'localhost'
    port = sys.argv[2] if len(sys.argv) > 2 else '10234'

    print(f"Connecting to {host}:{port}...")
    watcher = ProbeWatcher()

    if watcher.connect(host, port):
        print("ERROR: Failed to connect. Is firmware running with --xscope-port?")
        print("  Start with: xrun --xscope-port localhost:10234 <firmware.xe>")
        sys.exit(1)

    print("Connected. Waiting for probe data...\n")
    print(f"  (Watching for {TIMEOUT}s. Also checking if freertos_trace on probe 0 produces data.)\n")

    watcher.consume(lambda ts, name, val: None)

    # Wait for data
    if not watcher._got_any.wait(timeout=TIMEOUT):
        print(f"\nNo data received within {TIMEOUT}s. Possible issues:")
        print("  - Firmware not streaming probes")
        print("  - PDM mics not producing data")
        print("  - xscope connection issue")
        watcher.disconnect()
        sys.exit(1)

    # Collect for a few more seconds
    duration = 3.0
    print(f"\nGot data! Collecting for {duration}s...", flush=True)
    time.sleep(duration)

    watcher.report(duration)
    watcher.disconnect()


if __name__ == '__main__':
    main()
