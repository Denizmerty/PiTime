# PiTime

Calculate pi with exact integer Chudnovsky binary splitting and GMP. The default
is **10,000 decimal places**, truncated rather than rounded. Precision is a runtime
option, so changing it does not require recompiling.

The original quadratic decimal spigot has been replaced with a rapidly converging
series, fast multiprecision arithmetic, bounded parallel work, and a single output
buffer. GMP already implements its critical arithmetic kernels in optimized
assembly. See [performance results and methodology](docs/performance.md) and the
[algorithm notes](docs/algorithm.md).

## Build

### Visual Studio: open and run

Open **[PiTime.sln](PiTime.sln)** in Visual Studio 2022 (17.6+) or Visual Studio
2026 and press the green **Local Windows Debugger** button (F5). The solution has
one startup project, PiTime. It builds the application and launches it directly.
Choose **Release | x64** when measuring performance; **Debug | x64** supports
normal debugging.

The installed **Desktop development with C++** workload and its bundled vcpkg
component are used automatically. The first build restores the pinned GMP
dependency and its build tools over the internet; later builds reuse the cache.
This first restore can take several minutes.
There is no manual GMP installation, CMake generation, PATH editing, or
`vcpkg integrate install` step. The required GMP DLL is copied next to the
executable automatically.

The application is written to
`build/visual-studio/x64/<Debug-or-Release>/PiTime.exe`. Dependencies and generated
files stay under ignored `build/`, with vcpkg also using its normal user caches.
The solution contains no test or benchmark project: **building or running PiTime
never requires building tests**.

### PowerShell: interactive build

Run **[Build.ps1](Build.ps1)** from PowerShell 5.1 or later:

```powershell
.\Build.ps1
```

Choose **Release** or **Debug**, then **Build**, **Rebuild**, or **BuildAndRun**.
Press Enter to accept a default, or Q to cancel. The script finds Visual Studio
automatically and builds the same application-only solution described above.
Rebuild recompiles the application while retaining the dependency cache.
You can invoke the script from any working directory using its full path.

For automation or specific run settings, skip the menus with `-NonInteractive`:

```powershell
.\Build.ps1 -NonInteractive -Configuration Release
.\Build.ps1 -NonInteractive -Configuration Debug -Action Rebuild
.\Build.ps1 -NonInteractive -Action BuildAndRun -Digits 1000000 -Threads 0 -Quiet
```

BuildAndRun calculates 10,000 decimal places by default. `-Digits`, `-Threads`,
and `-Quiet` control the application when running it. Failures return a nonzero
exit code, and a failed build never launches an older executable. Tests and
benchmarks remain separate opt-in CMake targets.

### CMake: other platforms and optional tools

Requires a C++17 compiler, CMake 3.21+, Ninja, and GMP development headers/library.
Use a GMP build compatible with your compiler and architecture.

**Windows, MSYS2 MINGW64 terminal:**

```sh
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gmp
cmake --preset release
cmake --build --preset release
./build/release/pitime.exe --digits 10000
```

Keep `C:\msys64\mingw64\bin` on `PATH` when running this build from PowerShell;
it supplies GMP and the compiler's runtime DLLs. This is the optional MinGW build;
the Visual Studio solution above uses native MSVC and restores its own compatible
GMP library. Do not mix the two compiler toolchains' libraries.

**Ubuntu/Debian:**

```sh
sudo apt install g++ cmake ninja-build libgmp-dev
cmake --preset release
cmake --build --preset release
```

**macOS:**

```sh
brew install cmake ninja gmp
cmake --preset release -DGMP_ROOT="$(brew --prefix gmp)"
cmake --build --preset release
```

For a custom installation, pass `-DGMP_ROOT=/path/to/prefix`, or set
`GMP_INCLUDE_DIR` and `GMP_LIBRARY` explicitly. `release` enables compiler
optimization and link-time optimization when supported. `debug` is for debugging;
`native` additionally permits GCC/Clang to tune instructions for the build machine.
Native binaries may require that CPU. No fast-math flags are used.
The default CMake build also builds only the application and its core library.
Tests and benchmarks are separate opt-in targets, including when CMake generates
a Visual Studio solution.

## Run

```sh
./build/release/pitime                         # 10,000 decimal places
./build/release/pitime --digits 1000000 --quiet
./build/release/pitime --digits 1000000 --output pi.txt
./build/release/pitime --digits 1000000 --threads 1 --quiet
```

Digits go to stdout, or to `--output FILE` (replacing that file). Timing goes to
stderr and includes calculation, allocation, and decimal conversion, excluding
terminal/file I/O. `--quiet` suppresses stdout digits but still calculates the
entire result. `--threads 0` selects an automatic worker limit; small jobs stay
serial to avoid launch overhead. `--threads 1` forces serial execution. The worker
limit accepts 0–256 and precision accepts 0–100,000,000. Zero digits returns `3.`.
The upper limit is a resource guard, not a promise that every machine has enough
memory for that size; GMP's allocation failure behavior applies.

## Verify and benchmark

```sh
cmake --build --preset release-tests
ctest --preset release
cmake --build --preset release-benchmark
./build/release/pitime_benchmark --digits 10000 --samples 7 --threads 1 --csv build/comparison.csv
./build/release/pitime_benchmark --digits 1000000 --samples 7 --threads 0 --optimized-only
```

Correctness tests independently compute all 10,000 digits with Machin's formula
and check precision boundaries, long runs of nines, argument validation, file
output, and parallel consistency. The benchmark compiles both preserved originals
with the same Release options, checks their entire result against the new core,
and measures in-process calculation without console or process-start overhead.
See [benchmark details](benchmarks/README.md) for interpretation and limitations.
Equivalent `debug-tests`, `native-tests`, `debug-benchmark`, and
`native-benchmark` build presets are available after configuring those presets.

## Layout

```text
include/pitime/   Public calculation API
src/             Calculation implementation and command-line application
tests/           Independent numeric oracle and CLI contract checks
benchmarks/      Harness and original baselines with formatting changes only
cmake/           Dependency discovery
ide/visual-studio/ Native Visual Studio application project
docs/            Algorithm rationale and measured performance
build/           Generated files and local experiments (ignored)
```

The dependency is the [GNU MP library](https://gmplib.org/), distributed separately
under its own license. No GMP source or binaries are vendored. When redistributing
binaries, follow the licensing requirements of the GMP build you distribute.
