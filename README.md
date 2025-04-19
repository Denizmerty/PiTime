# Pi Digit Calculator(Spigot Algorithm)

This repository contains a C++ program(`PiTime.cpp`) that calculates a specified number of digits of Pi(π) using an efficient **Spigot algorithm**. It prints the result("3." followed by the requested digits) to the console along with the calculation time.

## Features

* Calculates Pi to a user-defined number of decimal places(hardcoded as `N` in `main`).
* Uses an efficient Spigot algorithm for sequential digit generation.
* Outputs the calculated digits of Pi(starting with "3.") to standard output.
* Reports the calculation time in milliseconds.

## How to Compile and Run

1. **Prerequisites:** You need a C++ compiler that supports C++11 or later(due to usage of `<chrono>`, `<vector>`, `<string>`, etc.). `g++`(part of the GCC) or `clang++` are common choices. Git should also be installed if you want to clone the repository.

2. **Get the Code:**
   * Clone the repository:
     ```bash
     git clone https://github.com/YourUsername/PiTime.git
     cd PiTime
     ```

3. **Compile:** Open a terminal or command prompt in the directory containing `PiTime.cpp` and run:
   ```bash
   # Using g++
   g++ PiTime.cpp -o PiTime -std=c++11 -O2

   # Using clang++
   clang++ PiTime.cpp -o PiTime -std=c++11 -O2
   ```
   * `-o PiTime`: Specifies the output executable file name as `PiTime`(or `PiTime.exe` on Windows).  
   * `-std=c++11`: Ensures C++11 features are enabled.  
   * `-O2`: Enables optimizations, which can significantly speed up the calculation.

4. **Run:** Execute the compiled program:
   ```bash
   # On Linux/macOS/Git Bash
   ./PiTime

   # On Windows Command Prompt/PowerShell
   .\PiTime.exe
   ```
   The program will then print the digits of Pi and the time taken.

5. **Modify Number of Digits:** To change the number of decimal digits calculated, edit the following line within the `main` function in `PiTime.cpp`:
   ```c++
   const int N = 10000; // Change 10000 to your desired number of digits
   ```
   After modifying `N`, recompile the program using the command from step 3.

## How it Works: The Spigot Algorithm

This program implements a **Spigot algorithm** to calculate the digits of Pi. Key characteristics of this approach include:

* **Sequential Digit Generation:** Unlike algorithms that compute a full high-precision value first, Spigot algorithms generate digits one by one(or in small batches), like water dripping from a tap(spigot). This makes them memory-efficient for calculating a large number of digits.  
* **Mixed-Radix Representation:** The algorithm internally represents the state of the calculation using an array(`a` in the code) that functions as digits in a mixed-radix number system. The base for each 'digit' `a[i]` is related to `(2*i + 1)`. This representation is derived from a specific infinite series for Pi.  
* **Iteration = Digit Extraction:** Each main loop iteration effectively simulates multiplying the current Pi approximation by 10(to shift the next decimal digit). It then normalizes the mixed-radix representation by processing the `a` array, calculating carries, and finally extracting the next potential decimal digit(`q`).  
* **Nines Buffering:** A critical feature is handling sequences of '9's correctly. Since a later carry operation might turn a sequence like `...4999...` into `...5000...`, the algorithm cannot immediately output a '9'. It buffers the last non-nine digit(`predigit`) and counts consecutive nines(`nines`). The output of these buffered digits is delayed until a digit *other* than 9 arrives, which resolves whether the buffered nines should be printed as `9`s or `0`s(if a carry propagated through them).

For a detailed step-by-step explanation of the algorithm's logic, the specific formula used, and the implementation details, please refer to the comments within the `PiTime.cpp` source file itself.

## Example Output

```bash
3.1415926535...5779458151
Calculation took 1234 milliseconds.
```

*Note: The exact digits shown above are truncated for brevity. The calculation time will vary depending on the value of `N` and the performance of the system running the code.*  
