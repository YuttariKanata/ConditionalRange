# Planning benchmark

Build, prebuilt iteration and full-operation timings are reported in microseconds:
median of seven calibrated batches after warm-up, with an unsigned digest and
compiler barrier.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/benchmark_planning
./build/benchmark_planning --checksums
```

Multi-configuration builds use `build/Release`. Compare revisions using the
same harness, compiler and flags. The explicit-large-cap case raises the cap
for both versions. Checksums supplement the oracle tests; performance varies
by workload and compiler.
