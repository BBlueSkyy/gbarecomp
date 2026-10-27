#!/usr/bin/env python3
"""Compare the same ROM-free SWI workload linked to two runtime revisions."""
import argparse
import json
import os
import re
import statistics
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('baseline', type=Path)
parser.add_argument('candidate', type=Path)
parser.add_argument('--pairs', type=int, default=12)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
if args.pairs < 1:
    parser.error('--pairs must be positive')
if hasattr(os, 'sched_getaffinity'):
    os.sched_setaffinity(0, {min(os.sched_getaffinity(0))})
paths = {'baseline': args.baseline.resolve(), 'candidate': args.candidate.resolve()}
raw = []
summary = {}
for mode in ['plain', 'pending_irq']:
    samples = {key: [] for key in paths}
    signatures = set()
    counts = {key: set() for key in paths}
    for pair in range(args.pairs):
        order = ['baseline', 'candidate'] if pair % 2 == 0 else ['candidate', 'baseline']
        for key in order:
            command = [str(paths[key])] + (['--pending-irq'] if mode == 'pending_irq' else [])
            result = subprocess.run(command, check=True, text=True, capture_output=True)
            fields = dict(re.findall(r'(\w+)=(\S+)', result.stdout))
            samples[key].append(float(fields['elapsed_ns']))
            counts[key].add(int(fields['interpreted']))
            signatures.add(tuple(fields[name] for name in
                                 ['cycles', 'final', 'r0', 'r1', 'depth', 'cpsr', 'state_fnv1a', 'bytes']))
            raw.append({'mode': mode, 'pair': pair, 'variant': key, 'stdout': result.stdout})
    if len(signatures) != 1:
        raise SystemExit(f'{mode}: nonmatching/unstable final guest states: {signatures}')
    summary[mode] = {
        'median_ns': {key: statistics.median(value) for key, value in samples.items()},
        'interpreted_instructions': {key: sorted(value) for key, value in counts.items()},
        'matching_state': list(next(iter(signatures))),
    }
args.output.write_text(json.dumps({'pairs': args.pairs, 'summary': summary, 'raw': raw}, indent=2) + '\n')
print(json.dumps(summary, indent=2))
