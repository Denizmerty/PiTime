# Calculation design

The public API is `pitime::calculate_pi(decimal_digits, threads)` and returns a
fresh `std::string`. There is no stored expansion of pi and no result cache.

## Series and binary splitting

For `A = 13591409`, `B = 545140134`, and `C = 10939058860032000`, define
`p(k) = (6k-5)(2k-1)(6k-1)` and `q(k) = k^3 C` for `k > 0`.
The Chudnovsky series can be written as

```text
S = A + sum(k >= 1, (-1)^k (A + B k) product(j=1..k, p(j)/q(j)))
pi = 426880 sqrt(10005) / S
```

Each term provides roughly 14 decimal digits. Recursive binary splitting forms
exact integer `P`, `Q`, and `T` for adjacent ranges. Two halves combine as
`T = T_left Q_right + P_left T_right`, `Q = Q_left Q_right`, and
`P = P_left P_right`. The root does not need `P`; propagating this fact down the
right edge eliminates unused products, including the largest one.

GMP selects multiplication, division, square-root, and decimal-conversion
algorithms for the operand sizes. The split tree enables balanced large products,
instead of the original repeated decimal digit extraction over an ever-large
array. The [Bellard pi computation report](https://bellard.org/pi/pi2700e9/pipcrecord.pdf)
describes this family of binary-splitting techniques.

## Correct truncation

All production arithmetic is integer arithmetic. The square root is computed as
`floor(sqrt(10005 * 10^(2w)))`, where `w` includes guard digits, followed by an
integer quotient. The term count deliberately uses a conservative 14 digits per
term and additional margin.

The source documents bounds on both the alternating-series remainder and the
discarded square-root fraction. Together they put the unfloored scaled result
within one unit of the true scaled pi. Its integer floor therefore differs from
the true integer floor by at most one. Before discarding guards, the implementation
rejects an all-zero or all-nine guard tail, since those are the only tails where
that difference could change a requested digit. It retries with additional guard
digits and throws if certification still fails. It never silently returns an
uncertified truncation.

Tests use a separate identity, `pi = 16 atan(1/5) - 4 atan(1/239)`, implemented
with fixed-point integer arithmetic. They accumulate a bound on rounding and
series-tail errors and require the lower and upper bounds to truncate identically.
This detects errors that comparing two Chudnovsky implementations might miss.

## Parallel work, storage, and assembly

Independent subtrees own separate GMP integers. Their only shared arithmetic
input is an immutable constant. Worker budgets cap recursive asynchronous work;
the square root can overlap with the series at larger precisions. Small workloads
stay serial because worker creation costs more than it saves. Automatic execution
is bounded even on machines reporting many hardware threads.

The output is constructed directly in one decimal string. Reserving one leading
byte allows insertion of the decimal point without moving the whole expansion.
The CLI writes that string in one operation and measures calculation separately
from output.

Handwritten assembly is already used where it matters through GMP's low-level
arithmetic implementation. Its [assembly documentation](https://gmplib.org/manual/Assembly-Basics)
identifies multiplication and multiply-add kernels as central performance targets.
This project does not add a second custom assembly implementation: no measured
benefit has been established over those tuned kernels, and doing so would add
architecture and calling-convention maintenance. Algorithmic changes, eliminated
products, and measured parallel thresholds provide the practical gains here.

The implementation follows GMP's [thread-safety rules](https://gmplib.org/manual/Reentrancy.html)
and changes no process-wide GMP allocators. Arithmetic operands are released by
RAII. Large requested precisions can require substantial temporary memory; the
unused half-products, square root, and series denominator are released as soon
as their consumers finish, before decimal output allocation. The
100-million-digit API limit does not imply a constant-memory calculation.
