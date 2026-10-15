# CPU Compiler and Workload Analysis

A small LLVM New Pass Manager plugin that reports loop structure and basic memory-operation counts, paired with controlled C++ workload experiments. The pass is analysis-only: it observes LLVM IR and preserves the input function without changing generated behavior.

The project is a practical starting point for studying how source loops are represented in IR and how memory layout and loop ordering relate to measured CPU behavior. It includes row-major, column-major, blocked, and optional OpenMP sum variants with correctness checks and repeated median/MAD timing. It does not claim to implement a compiler optimization or predict cycle counts from loop statistics.

## Requirements and build

- LLVM 18 development package, including `opt`, `llvm-config`, and CMake package files
- CMake 3.24+ and Ninja or another supported generator
- A C++17 compiler compatible with the installed LLVM build

```bash
cmake -S . -B build -G Ninja \
  -DLLVM_DIR="$(llvm-config --cmakedir)" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The fixture check loads the built plugin into `opt`, then checks reports for a fixed-trip loop and a loop with a runtime bound. CMake also builds the standalone workload benchmark.

## Use the pass

```bash
opt -load-pass-plugin=build/LoopReport.so \
  -passes='function(loop-report)' -disable-output tests/fixtures/loops.ll
```

On Windows the plugin has a `.dll` suffix; on macOS it usually has `.dylib`. The pass writes one tabular line per loop to standard error, for example:

```text
LOOP_REPORT function=fixed_loop depth=1 blocks=1 instructions=7 loads=1 stores=1 calls=0 constant_trip_count=16
```

`constant_trip_count=unknown` means ScalarEvolution did not derive a small constant count. It is not a claim that the loop cannot be analyzed or bounded.

## Workload experiments

Use a separate optimized C++ benchmark to compare loop variants. Keep input, compiler version, flags, thread count, and CPU state fixed. A useful sequence is:

1. Compile a scalar reference and check output equivalence.
2. Ask Clang for vectorization remarks (`-Rpass=loop-vectorize -Rpass-missed=loop-vectorize`).
3. Inspect the generated assembly for vector loads/stores and loop structure.
4. Measure repeated runs, report median and spread, then use `perf stat` where available to inspect cycles, instructions, cache misses, and branch behavior.

The LLVM plugin emits analysis facts; the separate workload executable provides a small CPU timing experiment. It does not provide profiler counters by itself. Add any local measurements with the exact CPU/compiler/command and do not infer a speedup from the IR report alone.

Run the workload benchmark after the build:

```bash
./build/loop_benchmark 1024 15 > results.csv
```

The size and repetition count are arguments. It prints a correctness-checked CSV and reports median plus median absolute deviation (MAD). `OpenMP` appears only when CMake finds it. For compiler remarks, compile the workload with Clang and `-Rpass=loop-vectorize -Rpass-missed=loop-vectorize`; for hardware counters, run the binary under Linux `perf stat`. Record compiler, flags, CPU, thread count, and power settings with any measurements.

A measured Windows/AMD CPU sample, its command, and its limits are in [results/sample_amd64_2026-10-15.md](results/sample_amd64_2026-10-15.md). It is included to show actual output, not as a cross-machine performance claim.

A broader [matrix-size/thread sweep](results/amd64_sweep_2026-10-15.md) retains all configurations, including slowdowns. The [companion JSON](results/amd64_sweep_2026-10-15.json) includes every iteration's timing, correctness checks, compiler flags, host details and the executable hash. Reproduce it from the repository root with Python 3.10+ and GCC/OpenMP:

In the recorded Windows/AMD run, a single-thread 2048×2048 sum took 3.9138 ms in row-major order versus 33.4464 ms in column-major order (8.55×). These are medians across three round medians, each with fifteen timed iterations. Every one of the sweep's 1,080 timed checksums passed. The report also retains cases where blocking or extra threads made execution slower.

```bash
python tools/run_sweep.py --compile --sizes 256,1024,2048 --threads 1,4 --rounds 3 --repetitions 15 --output results/cpu_sweep.json
```

The sweep runs cases sequentially and reports medians across rounds. Keep other CPU workloads idle when reproducing it. Matrix traversal changes and thread count explain the comparison; the LLVM analysis pass does not optimize these kernels. CI separately publishes its loop reports and CPU sweep as downloadable artifacts.

## Scope and limitations

The pass counts instructions, loads, stores, and calls in each loop's blocks and asks ScalarEvolution for a small constant trip count. Nested-loop body blocks can appear in both a child loop's report and its parents' aggregate. The CPU benchmark is a focused experiment and does not model cache misses, aliasing costs, vectorization profitability, or GPU execution. The supplied IR fixture is for validating the pass's reporting contract, not a performance result.

## License

MIT. See [LICENSE](LICENSE).
