#!/usr/bin/env python3
"""Minimal xscope test — bypasses all custom code, uses base Endpoint directly."""

import os, sys, time, threading

_script_dir = os.path.dirname(os.path.abspath(__file__))
_ws_root = os.path.dirname(_script_dir)
sys.path.insert(0, os.path.join(_ws_root, 'modules', 'io', 'modules', 'mic_array', 'script'))

from mic_array.xscope import Endpoint, QueueConsumer

got_data = threading.Event()
data_count = [0]

def on_data(timestamp, probe_name, value):
    data_count[0] += 1
    if data_count[0] <= 5:
        print(f"  GOT DATA: probe={probe_name} ts={timestamp} val={value}")
    got_data.set()

ep = Endpoint()
port = sys.argv[1] if len(sys.argv) > 1 else '10234'

print(f"Connecting to localhost:{port}...")
if ep.connect('localhost', port):
    print("ERROR: connect failed")
    sys.exit(1)

print("Connected. Subscribing to all probes via consume()...")
ep.consume(on_data)  # wildcard consume

# Also try QueueConsumer as backup
try:
    qc = QueueConsumer(ep, '*', probe_timeout=3.0)
    has_queueconsumer = True
except:
    has_queueconsumer = False

print(f"Waiting 10s for ANY data... (QueueConsumer: {has_queueconsumer})")

if got_data.wait(timeout=10.0):
    time.sleep(2)  # collect a bit more
    print(f"\nReceived {data_count[0]} data events total")
else:
    print(f"\nNO data received via consume callback.")
    if has_queueconsumer:
        print("Checking QueueConsumer...")
        try:
            val = qc.queue.get_nowait()
            print(f"  QueueConsumer HAS data: {val}")
        except:
            print("  QueueConsumer also empty.")

ep.disconnect()
