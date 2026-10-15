"""Run a controlled matrix-size/thread sweep and retain every timing sample."""

import argparse
import csv
import hashlib
import io
import json
import os
import platform
import statistics
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def positive_list(value):
    numbers = [int(part) for part in value.split(',')]
    if not numbers or min(numbers) <= 0:
        raise argparse.ArgumentTypeError('expected comma-separated positive integers')
    return numbers


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sizes', type=positive_list, default=[256, 1024, 2048])
    parser.add_argument('--threads', type=positive_list, default=[1, 4])
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--repetitions', type=int, default=15)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--compile', action='store_true', help='build with the recorded GCC flags')
    parser.add_argument('--binary', type=Path,
                        default=Path('build/loop_benchmark.exe' if os.name == 'nt' else 'build/loop_benchmark'))
    parser.add_argument('--output', type=Path, default=Path('results/cpu_sweep.json'))
    args = parser.parse_args()
    if args.rounds <= 0 or args.repetitions <= 0 or max(args.sizes) > 10000:
        parser.error('rounds/repetitions must be positive and sizes <=10000')
    flags = ['-std=c++17', '-O3', '-Wall', '-Wextra', '-Wpedantic', '-fopenmp']
    if args.compile:
        args.binary.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([args.compiler, *flags, 'workloads/loop_benchmark.cpp',
                        '-o', str(args.binary)], check=True)
    version = subprocess.run([args.compiler, '--version'], capture_output=True,
                             text=True, check=True).stdout.splitlines()[0]
    rows = []
    for round_index in range(args.rounds):
        for threads in args.threads:
            for size in args.sizes:
                env = {**os.environ, 'OMP_NUM_THREADS': str(threads), 'OMP_DYNAMIC': 'FALSE'}
                command = [str(args.binary.resolve()), str(size), str(args.repetitions)]
                result = subprocess.run(command, env=env, capture_output=True,
                                        text=True, check=True, timeout=180)
                for row in csv.DictReader(io.StringIO(result.stdout)):
                    if row['check'] != 'PASS':
                        raise RuntimeError(f'correctness failure: {row}')
                    rows.append({'round': round_index + 1, 'threads': threads, 'n': size,
                                 'kernel': row['kernel'], 'median_ms': float(row['median_ms']),
                                 'mad_ms': float(row['mad_ms']), 'checksum': float(row['checksum']),
                                 'samples_ms': [float(v) for v in row['samples_ms'].split(';')],
                                 'check': row['check'], 'runtime_info': result.stderr.strip()})
                print(f'Completed round {round_index + 1}: n={size}, threads={threads}')
    report = {'created_at_utc': datetime.now(timezone.utc).isoformat(),
              'environment': {'os': platform.platform(), 'processor': platform.processor(),
                              'python': platform.python_version(), 'compiler': version},
              'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
              'build_flags': flags if args.compile else 'externally built; inspect build command',
              'experiment': {'sizes': args.sizes, 'threads': args.threads,
                             'rounds': args.rounds, 'repetitions': args.repetitions,
                             'warmup_per_variant': 1, 'seed': 2026,
                             'timing': 'kernel only; every timed checksum is validated'},
              'rows': rows}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    lines = ['# CPU matrix traversal sweep', '',
             f"Run: {report['created_at_utc']}", f'Compiler: {version}',
             f"Host: {report['environment']['processor']} / {report['environment']['os']}", '',
             'Each value is the median of independent round medians. All per-iteration samples,',
             'checksums, MAD values, build flags and binary hash are in the companion JSON.',
             'Speedup uses column-major traversal as the baseline at the same size/thread setting.',
             'All configurations are retained, including slowdowns. This is a host-specific',
             'memory-traversal experiment; the LLVM plugin is analysis-only and does not cause these gains.', '',
             '| Size | Threads | Variant | Median ms | Round range ms | Speedup vs column-major |',
             '|---:|---:|---|---:|---:|---:|']
    for threads in args.threads:
        for size in args.sizes:
            subset = [r for r in rows if r['threads'] == threads and r['n'] == size]
            baseline = statistics.median(r['median_ms'] for r in subset if r['kernel'] == 'column_major')
            for kernel in dict.fromkeys(r['kernel'] for r in subset):
                values = [r['median_ms'] for r in subset if r['kernel'] == kernel]
                center = statistics.median(values)
                lines.append(f'| {size} | {threads} | {kernel} | {center:.4f} | '
                             f'{min(values):.4f}–{max(values):.4f} | {baseline / center:.2f}× |')
    args.output.with_suffix('.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'Wrote {args.output} and {args.output.with_suffix(".md")}')


if __name__ == '__main__':
    main()
