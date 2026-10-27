#!/usr/bin/env python3
"""Compare two already-built ROM-free runtime_hotpath_bench executables."""
import argparse
import collections
import json
import os
import statistics
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('baseline', type=Path)
parser.add_argument('candidate', type=Path)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--pairs', type=int, default=8)
args = parser.parse_args()
if args.pairs < 1:
    parser.error('--pairs must be positive')
if hasattr(os, 'sched_getaffinity'):
    os.sched_setaffinity(0, {min(os.sched_getaffinity(0))})
paths = {'baseline': args.baseline.resolve(), 'candidate': args.candidate.resolve()}
values = {key: collections.defaultdict(list) for key in paths}
states = {key: set() for key in paths}
raw = []
for pair in range(args.pairs):
    order = ['baseline', 'candidate'] if pair % 2 == 0 else ['candidate', 'baseline']
    for key in order:
        output = subprocess.check_output([str(paths[key])], text=True)
        raw.append({'pair': pair, 'variant': key, 'stdout': output})
        for line in output.splitlines():
            if ',' in line:
                name, value = line.split(',')
                values[key][name].append(float(value))
            else:
                states[key].add(line)
if states['baseline'] != states['candidate'] or len(states['baseline']) != 2:
    raise SystemExit(f'Nonmatching or unstable fixture state: {states}')
if values['baseline'].keys() != values['candidate'].keys():
    raise SystemExit('Nonmatching benchmark paths')
summary = {}
for name in values['baseline']:
    base = statistics.median(values['baseline'][name])
    candidate = statistics.median(values['candidate'][name])
    summary[name] = {'baseline_ns': base, 'candidate_ns': candidate,
                     'reduction_percent': 100 * (1 - candidate / base)}
result = {'paths': {k: str(v) for k, v in paths.items()},
          'samples_per_path': args.pairs * 7,
          'state': sorted(states['baseline']), 'summary': summary, 'raw': raw}
args.output.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'summary': summary, 'state': result['state']}, indent=2))
