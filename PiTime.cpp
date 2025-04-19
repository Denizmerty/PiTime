/*
* =======================================================================================
* Program Explanation and Spigot Algorithm for Pi
* =======================================================================================
*
* Program Overview:
* -----------------
* This C++ program calculates a specified number of decimal digits of Pi using a
* "Spigot" algorithm. The main calculation logic is encapsulated within the
* `calculatePiDigitsString` function, which takes the desired number of digits `n`
*(after the decimal point) as input and returns a string representation of Pi
*("3." followed by n digits).
*
* Spigot Algorithm for Pi:
* ------------------------
* A Spigot algorithm is a type of algorithm designed to compute digits of a mathematical
* constant(like Pi or e) sequentially, producing one or a few digits at each step,
* much like water dripping from a spigot(tap). This is in contrast to algorithms
* that require computing a high-precision approximation of the entire number before
* any digits can be reliably extracted.
*
* Key Concepts:
* 1.  Representation: Spigot algorithms often rely on representing Pi(or a related
*     value) as an infinite series or a similar mathematical construct. This
*     particular implementation is based on a formula that can be represented in a
*     mixed-radix system.
* 2.  Mixed-Radix Representation: Instead of a standard base(like base 10 or base 2),
*     the algorithm implicitly uses a mixed-radix system where the 'place values'
*     are related to the denominators in the underlying series formula. The `a`
*     array in the code holds the 'digits' in this mixed-radix system. The base
*     for the digit `a[i]` is related to `(2*i + 1)`.
* 3.  Digit Extraction: Each iteration of the main loop(the `j` loop) effectively
*     simulates multiplying the current approximation of Pi by 10(to shift the
*     next decimal digit into the integer part). The inner loop(the `i` loop)
*     performs this multiplication and normalization within the mixed-radix system.
* 4.  Normalization and Carry: The inner loop processes the `a` array from right to
*     left(less significant to more significant terms).
*     - `num = a[i] * 10 + carry`: Multiplies the current 'digit' `a[i]` by 10 and
*       adds the carry from the previous position(to the right).
*     - `a[i] = num %(2*i + 1)`: Updates the digit `a[i]` by taking the remainder
*       when divided by its base `(2*i + 1)`. This is the normalization step,
*       keeping the digit within its valid range.
*     - `carry =(num /(2*i + 1)) * i`: Calculates the amount to be carried over
*       to the next position(to the left). The division `num /(2*i + 1)` finds
*       how many times the base fits into `num`, and this is scaled by `i` according
*       to the structure of the specific Pi formula being used.
* 5.  Output Digit(`q`): After processing the entire `a` array, the final carry
*     is added to `a[0] * 10`. The integer part of this result divided by 10(`q`)
*     is the candidate for the next decimal digit of Pi. The remainder updates `a[0]`.
* 6.  Handling Nines(Buffering): A common issue in streaming digit algorithms is
*     that a sequence like `...4999...` might later need to become `...5000...` if
*     a carry propagates from further calculations. The algorithm cannot be sure
*     whether a '9' is final or will be incremented. This implementation handles
*     this using `predigit` and `nines`:
*     - `predigit`: Stores the last digit that was *not* a 9.
*     - `nines`: Counts consecutive 9s encountered after `predigit`.
*     - When a digit `q` less than 9 arrives: The stored `predigit` is outputted,
*       followed by all the counted `nines`(as actual 9s). `predigit` is updated
*       to `q`, and `nines` is reset.
*     - When a digit `q` equal to 10 arrives(represented as 10 initially, before
*       handling): The stored `predigit` is incremented by 1 and outputted,
*       followed by the counted `nines`(as 0s). `predigit` is reset(often to 0),
*       and `nines` is reset.
*     - When a digit `q` is 9: The `nines` counter is simply incremented.
*     This buffering mechanism ensures correct output by delaying the printing of
*     potentially ambiguous 9s until the next non-9 digit resolves them.
*
* Code Implementation Details(`calculatePiDigitsString`):
* -----------------------------------------------------
* - `n`: Number of decimal digits requested after "3.".
* - `len`: Calculated size of the state array `a`. The formula `floor(10.0*n/3.0)+3`
*   is an empirical value ensuring enough terms are included for the required
*   precision. Since roughly log10(3) ~ 3.3 digits are produced per term, 10*n/3
*   terms are needed. The `+3` provides a safety margin.
* - `a`: The vector representing the state of the calculation in the mixed-radix
*   system. It's initialized with 2s, which is specific to the starting state of
*   the formula variant used here.
* - `calculated_digits`: Stores the computed decimal digits(after the decimal point)
*   once they are confirmed(i.e., after handling the nines buffering).
* - `nines`, `predigit`: Variables for the output buffering mechanism described above.
* - Outer loop(`j`): Iterates approximately `n` times, aiming to produce one
*   confirmed digit(or resolve buffered nines) per iteration. The `n+3` limit
*   provides buffer iterations.
* - Inner loop(`i`): Implements the core Spigot calculation step(multiply by 10,
*   normalize, propagate carry) across the state array `a`, from right to left.
* - Final Digit Extraction(`q`): Gets the candidate next digit after the inner loop.
* - Output Logic: Implements the buffering mechanism using `q`, `predigit`, and `nines`
*   to append confirmed digits to `calculated_digits`.
* - Termination: The loop breaks early if enough digits(`n+1`) have been stored
*   in `calculated_digits`.
* - Formatting: Uses `stringstream` to build the final output string "3." followed by
*   the required number of digits from `calculated_digits`. Ensures no more than `n`
*   digits are appended after the ".".
*
*/
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <string>
#include <sstream>
#include <algorithm>

std::string calculatePiDigitsString(int n) {
    if (n <= 0) {
        return "3.";
    }

    int len = static_cast<int>(std::floor(10.0 * n / 3.0)) + 3;
    std::vector<int> a(len);
    for (int i = 0; i < len; ++i) {
        a[i] = 2;
    }

    std::vector<int> calculated_digits;
    calculated_digits.reserve(n + 5);

    int nines = 0;
    int predigit = 0;

    for (int j = 0; j < n + 3; ++j) {
        long long carry = 0;
        for (int i = len - 1; i > 0; --i) {
            long long num = (long long)a[i] * 10 + carry;
            a[i] = static_cast<int>(num % (2 * i + 1));
            carry = num / (2 * i + 1) * i;
        }
        long long final_num = (long long)a[0] * 10 + carry;
        int q = static_cast<int>(final_num / 10);
        a[0] = static_cast<int>(final_num % 10);

        if (q >= 10) {
            q = 10;
        }

        if (j > 0) {
            if (q < 9) {
                calculated_digits.push_back(predigit);
                for (int k = 0; k < nines; ++k) {
                    calculated_digits.push_back(9);
                }
            }
            else if (q == 10) {
                calculated_digits.push_back(predigit + 1);
                for (int k = 0; k < nines; ++k) {
                    calculated_digits.push_back(0);
                }
            }
        }

        if (q < 9) {
            predigit = q;
            if (j > 0) nines = 0;
        }
        else if (q == 9) {
            if (j > 0) nines++;
            else predigit = q;
        }
        else { 
            predigit = 0;
            if (j > 0) nines = 0;
        }

        if (calculated_digits.size() >= n + 1) {
            break;
        }
    }

    std::stringstream pi_stream;
    pi_stream << "3.";

    int num_decimal_digits_available = (calculated_digits.size() > 0) ? calculated_digits.size() - 1 : 0;
    int digits_to_print = std::min(n, num_decimal_digits_available);

    for (int i = 0; i < digits_to_print; ++i) {
        pi_stream << calculated_digits[i + 1];
    }

    return pi_stream.str();
}


int main() {
    const int N = 10000;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::string pi_digits = calculatePiDigitsString(N);

    auto end_time = std::chrono::high_resolution_clock::now();

    std::cout << pi_digits << std::endl;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Calculation took " << duration.count() << " milliseconds." << std::endl;

    return 0;
}