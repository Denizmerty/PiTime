# Performance verification

Measured on 2026-09-20, Windows build 26200, AMD Ryzen 7 4800H, 16 reported
hardware threads, Balanced power plan. GCC 14.2.0 (MSYS2 MinGW x64) and GMP 6.3.0
with 64-bit limbs were used. Power/thermal conditions and unrelated OS work were
not controlled; these are local measurements, not universal guarantees.

## Default workload: 10,000 decimal places

All three implementations were built into the same Release executable, with
`-O3 -DNDEBUG -flto=auto -fno-fat-lto-objects`. Seven samples per implementation
followed warmup, with rotating order. The new implementation used one thread.

| Implementation                 | Median per calculation | Minimum per calculation |
| ------------------------------ | ---------------------: | ----------------------: |
| GitHub PiTime, pinned upstream |           1,222.883 ms |            1,213.599 ms |
| Original local copy            |           1,267.265 ms |            1,261.321 ms |
| New PiTime                     |           **0.913 ms** |            **0.890 ms** |

The median improvement is **1,340.1× over upstream** and **1,388.7× over the local
copy**. Every measured result matched all 10,000 digits exactly. The independent
Machin oracle also verified those digits, so the claim does not rely solely on
agreement with the original implementation.

The measurement includes calculation, memory allocation, and decimal formatting.
It excludes process startup, output, validation, and hashing. It is a calculation
speedup, not an end-to-end terminal rendering speedup. Each fast sample batches
fresh calculations until it accumulates at least 10 ms. No precomputed digits or
cached calculations are used. Full methodology is in the
[benchmark guide](../benchmarks/README.md).

[Raw comparison samples](benchmarks/windows-ryzen4800h-10000.csv) preserve all
timings and the output fingerprint. The source snapshots retain the measured
implementations with verified whitespace-only formatting changes; CTest checks
their formatted SHA-256 hashes. The [benchmark guide](../benchmarks/README.md)
records both original and formatted hashes. Upstream is commit
[`b3fd7ff94939756a131a63a217553639e8ded70d`](https://github.com/Denizmerty/PiTime/tree/b3fd7ff94939756a131a63a217553639e8ded70d).
The local source differs from upstream and is therefore measured separately.

## Larger calculations

Seven samples per mode, using the portable Release build:

| Decimal places | Serial median | Automatic median | Serial / automatic |
| -------------- | ------------: | ---------------: | -----------------: |
| 100,000        |     21.110 ms |        15.294 ms |              1.38× |
| 1,000,000      |    402.724 ms |       223.483 ms |              1.80× |

Automatic execution uses at most eight workers. Parallel work starts at 100,000
digits, and splitting below 4,096 terms stays serial. The worker setting is an
upper bound, not a promise to create that many threads.

These scaling runs check every result against an untimed serial calculation.
They are consistency checks, not independent mathematical verification of every
million-digit result, and do not run the quadratic upstream at these sizes.
Raw samples: [100k serial](benchmarks/windows-ryzen4800h-100000-threads1.csv),
[100k automatic](benchmarks/windows-ryzen4800h-100000-threads0.csv),
[1m serial](benchmarks/windows-ryzen4800h-1000000-threads1.csv), and
[1m automatic](benchmarks/windows-ryzen4800h-1000000-threads0.csv).

## Tuning decisions

- Binary splitting and GMP replace hundreds of millions of dependent small
  divisions with a much smaller set of large integer operations.
- Unneeded products along the right edge of the split tree are skipped.
- Consumed half-products, the square root, and the series denominator are freed
  before decimal conversion to reduce the live memory footprint.
- Large jobs overlap the square root and independent series subtrees. Small
  jobs avoid thread startup entirely.
- Decimal output is built in one string and written in a single operation.
- GMP supplies the assembly arithmetic. The installed library contains CPU
  variants of `addmul_1` and `mul_basecase`; a second custom assembly kernel was
  not justified by measured alternatives.
- Exploratory native 64-bit leaf construction with `mpz_import` or direct GMP
  limb access was slower: representative 10k medians were 0.937–0.998 ms versus
  0.899–0.916 ms for the simpler GMP operations. Those variants were discarded.
- Truncating final-division operands saved only about 1% at one million digits
  in exploratory measurements and added approximate arithmetic/error analysis.
  Exact operands were retained.
- CPU-specific compilation was also checked. In separate 21-sample 10k runs,
  `-march=native` measured 0.873 ms versus portable 0.879 ms, a small difference
  that does not establish a robust benefit given run-order and OS noise.
  Portable Release remains the default; `native` is available for local use.
  [Native samples](benchmarks/windows-ryzen4800h-10000-native.csv) and
  [portable samples](benchmarks/windows-ryzen4800h-10000-portable.csv).

## Reproduce

With CMake/Ninja/GCC on PATH, the local configure command was equivalent to:

```powershell
cmake --preset release -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe -DGMP_ROOT=C:/msys64/mingw64
cmake --build --preset release --parallel 4
cmake --build --preset release-tests
ctest --preset release
cmake --build --preset release-benchmark
.\build\release\pitime_benchmark.exe --digits 10000 --samples 7 --threads 1 --csv build/comparison.csv
.\build\release\pitime_benchmark.exe --digits 1000000 --samples 7 --threads 1 --optimized-only --csv build/serial.csv
.\build\release\pitime_benchmark.exe --digits 1000000 --samples 7 --threads 0 --optimized-only --csv build/automatic.csv
```

Replace `release` with `native` to configure/build the CPU-specific variant, build
`native-benchmark`, then run it with `--digits 10000 --samples 21 --threads 1 --optimized-only`.
Run benchmark commands sequentially after compilation and other test work finish.

Local validation passed in Release, Debug, and native Release: the numeric suite
(162 exact comparisons plus 100k parallel parity), baseline integrity, and CLI
contracts. The only build warning comes from a signed/unsigned comparison in the
upstream implementation. Linux/macOS/Windows CI is configured, including Linux
sanitizers. Local results above do not include remote CI runs. MSVC and the
100-million-digit resource ceiling have not been benchmarked here.

The original Visual Studio project files and build artifacts were preserved
locally under ignored `build/legacy/`. They are not included in the new source
layout or proposed commit.
