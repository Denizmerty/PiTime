# Performance comparison

`pitime_benchmark` compiles three implementations into one executable using the
same build configuration:

| Name        | Source                | Timed operation                                                         |
| ----------- | --------------------- | ----------------------------------------------------------------------- |
| `upstream`  | `upstream/PiTime.cpp` | Original `calculatePiDigitsString`, returning a decimal string          |
| `local`     | `local/PiTime.cpp`    | Original checkout's `printPiDigits`, with its output captured in memory |
| `optimized` | `src/pi.cpp`          | Public `pitime::calculate_pi`, returning a decimal string               |

The snapshots retain the original implementations with whitespace-only changes
to follow BallisticsWorkbench's C++ formatting conventions. Every formatter
replacement was checked to remove and insert only whitespace. Each wrapper
renames the original `main` with a preprocessor definition, includes the snapshot,
and calls its original calculation function. No reference algorithm has been
rewritten, compiled without optimization, or burdened with terminal output during
timing.

The upstream snapshot is from
[`b3fd7ff94939756a131a63a217553639e8ded70d`](https://github.com/Denizmerty/PiTime/tree/b3fd7ff94939756a131a63a217553639e8ded70d).

CTest checks the SHA-256 hashes of the formatted files in this repository:

| Snapshot              | Formatted SHA-256                                                  |
| --------------------- | ------------------------------------------------------------------ |
| `upstream/PiTime.cpp` | `F5A6BF554E628ECCAA1CB30390BE679FC24FCD350159D8EC042537B60068AF2E` |
| `local/PiTime.cpp`    | `DA9C61BBE912C3AC65537A48631F74A5BAC73C12C180A76AFD93FF1027499829` |

The original bytes used for the recorded performance measurements had these
hashes, retained here for provenance:

| Snapshot              | Original SHA-256                                                   |
| --------------------- | ------------------------------------------------------------------ |
| `upstream/PiTime.cpp` | `EA16932AD72F14FD976F3AD7C7B8998B111AD1B8E266FE92AB97EDD5477B7AFF` |
| `local/PiTime.cpp`    | `DA3A79556758EC0DE56C04A19DAA8306D82992148744312D95CCA39AC1DD3034` |

## Running

Configure in Release mode using the repository's CMake instructions, then build
the optional benchmark with `cmake --build --preset release-benchmark`. Normal
application builds do not compile the benchmark. Run the benchmark from its
build output directory:

```powershell
.\pitime_benchmark.exe --digits 10000 --samples 7 --threads 1 --csv comparison.csv
```

On Linux or macOS, use `./pitime_benchmark` instead. Defaults are 10,000 decimal
digits, seven measured samples, and one thread for the optimized implementation.
`--threads 0` permits its automatic thread selection. The two original
implementations always use one thread.

For large inputs, avoid the quadratic reference calculations:

```powershell
.\pitime_benchmark.exe --optimized-only --digits 1000000 --samples 7 --threads 1 --csv serial.csv
.\pitime_benchmark.exe --optimized-only --digits 1000000 --samples 7 --threads 8 --csv parallel.csv
```

Run these commands sequentially on an otherwise idle machine. Record the CPU,
operating system, power conditions, complete configure/build commands and build
flags with results. The benchmark records the compiler version, GMP version and
limb size, reported hardware thread count, requested thread count and `NDEBUG`
setting. These details affect reproducibility; results on one machine are not a
universal speed guarantee.

## Measurement and validation

- All implementations receive the same number of decimal digits. Timing uses
  `std::chrono::steady_clock` and includes actual calculation, internal memory
  allocation and decimal string formatting.
- An untimed upstream calculation establishes the expected full output. Each
  implementation receives one discarded warmup call. Measured samples rotate
  execution order to reduce systematic ordering effects.
- Each sample accumulates at least 10 milliseconds of timed calls. Its first
  call estimates how many additional calls to batch. Every invocation recomputes
  its result from scratch. The reported per-call time is total measured time
  divided by the number of calls, including the initial calibration call.
- Batch result storage is reserved outside the timed region. Each newly returned
  string is moved into that storage during timing. Every string is compared in
  full against the expected output and hashed outside timing, then destroyed.
  Retained batch strings are limited to about 64 MiB, except when one result
  alone is larger. There is no calculation-result cache.
- For the local baseline, an RAII guard redirects `std::cout` to a custom
  `std::streambuf` before timing and restores it afterward, including on error.
  The original per-digit integer formatting and resulting string allocations
  remain inside timing. Installing the stream buffer, validation, hashing and
  terminal/file output are outside timing for every implementation.
- The benchmark exits with an error for any incorrect output, including a wrong
  length. Normal comparison mode also exits with code 2 if the optimized median
  is not faster than upstream. `--allow-slower` disables only that speed check,
  which is useful when inspecting tiny inputs or noisy environments.
- `--optimized-only` verifies every measured result against an untimed,
  single-threaded optimized calculation. This checks repeated and parallel
  consistency; it is **not** independent mathematical verification and makes no
  upstream speedup claim. The correctness test suite supplies independent
  reference checks.

The original spigot implementations can stop before flushing a pending run of
nines at some requested lengths (for example, near decimal digit 762). A normal
comparison intentionally fails when the original cannot supply the requested
length; it does not trim the optimized result or change the baseline to hide the
defect. Use `--optimized-only` for those lengths and run the correctness suite.

Each sample emits a CSV row with digit count, requested threads (`1` for either
baseline), sample number, implementation, call count, total measured
milliseconds, milliseconds per call and output FNV-1a hash. Metadata and summary
lines on stdout begin with `#`. `--csv PATH` additionally writes a clean CSV file
containing only its header and sample rows.

The summary reports each implementation's median and minimum per-call times.
Speedups compare medians with medians and minima with minima. Use the median
comparison as the primary result; do not mix best-case optimized timing with a
slow baseline outlier. Output hashes are reproducibility aids, while complete
string equality is the correctness check.
