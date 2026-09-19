#include "pitime/pi.hpp"

#include <gmp.h>

#include <algorithm>
#include <future>
#include <stdexcept>
#include <thread>
#include <utility>

namespace pitime
{
namespace
{

// Keep ownership separate from GMP's arithmetic. Every calculation owns its
// operands, so concurrent callers and worker tasks share no mutable GMP state.
class Integer
{
  public:
    Integer()
    {
        mpz_init(value_);
    }
    ~Integer()
    {
        mpz_clear(value_);
    }
    Integer(const Integer&) = delete;
    Integer& operator=(const Integer&) = delete;
    Integer(
        Integer&& other
    ) noexcept
        : Integer()
    {
        mpz_swap(value_, other.value_);
    }
    Integer& operator=(
        Integer&& other
    ) noexcept
    {
        mpz_swap(value_, other.value_);
        return *this;
    }
    operator mpz_ptr()
    {
        return value_;
    }
    operator mpz_srcptr() const
    {
        return value_;
    }

  private:
    mpz_t value_;
};

struct Series
{
    Integer p;
    Integer q;
    Integer t;
};

constexpr unsigned long a_coefficient = 13'591'409;
constexpr unsigned long b_coefficient = 545'140'134;
constexpr unsigned long split_parallel_threshold = 4'096;
constexpr unsigned max_auto_threads = 8;

// For [a,b), P = product(p_k), Q = product(q_k), and T/Q is the
// Chudnovsky sum with its common prefix removed. The term factors are
// p_k = (6k-5)(2k-1)(6k-1), q_k = k^3 * 640320^3/24.
// Combining two adjacent ranges requires only three large multiplications
// for T and Q, plus one for P when the caller needs it.
Series split(
    unsigned long a,
    unsigned long b,
    bool need_p,
    unsigned workers,
    mpz_srcptr c_cubed_over_24
)
{
    if (b - a == 1)
    {
        Series result;
        if (a == 0)
        {
            if (need_p)
                mpz_set_ui(result.p, 1);
            mpz_set_ui(result.q, 1);
            mpz_set_ui(result.t, a_coefficient);
        }
        else
        {
            mpz_set_ui(result.p, 6 * a - 5);
            mpz_mul_ui(result.p, result.p, 2 * a - 1);
            mpz_mul_ui(result.p, result.p, 6 * a - 1);
            mpz_ui_pow_ui(result.q, a, 3);
            mpz_mul(result.q, result.q, c_cubed_over_24);
            mpz_set_ui(result.t, a);
            mpz_mul_ui(result.t, result.t, b_coefficient);
            mpz_add_ui(result.t, result.t, a_coefficient);
            mpz_mul(result.t, result.t, result.p);
            if ((a & 1) != 0)
                mpz_neg(result.t, result.t);
        }
        return result;
    }

    const auto middle = a + (b - a) / 2;
    Series left;
    Series right;
    if (workers > 1 && b - a >= split_parallel_threshold)
    {
        const auto left_workers = workers / 2;
        auto future = std::async(
            std::launch::async,
            [=] { return split(a, middle, true, left_workers, c_cubed_over_24); }
        );
        right = split(middle, b, need_p, workers - left_workers, c_cubed_over_24);
        left = future.get();
    }
    else
    {
        left = split(a, middle, true, 1, c_cubed_over_24);
        right = split(middle, b, need_p, 1, c_cubed_over_24);
    }

    mpz_mul(left.t, left.t, right.q);
    mpz_addmul(left.t, left.p, right.t);
    mpz_mul(left.q, left.q, right.q);
    // The root's P is unused. Propagating that fact down the right edge
    // avoids its largest product and several smaller products entirely.
    if (need_p)
        mpz_mul(left.p, left.p, right.p);
    else
        left.p = Integer {}; // Release the consumed half-product before returning.
    return left;
}

Integer scaled_square_root(
    unsigned long working_digits
)
{
    Integer square;
    Integer root;
    mpz_ui_pow_ui(square, 10, 2 * working_digits);
    mpz_mul_ui(square, square, 10'005);
    mpz_sqrt(root, square);
    return root;
}

unsigned worker_count(
    unsigned requested
)
{
    if (requested != 0)
        return requested;
    return std::max(1u, std::min(std::thread::hardware_concurrency(), max_auto_threads));
}

} // namespace

std::string calculate_pi(
    std::size_t decimal_digits,
    unsigned threads
)
{
    if (decimal_digits > max_digits)
    {
        throw std::invalid_argument("requested precision exceeds 100,000,000 decimal digits");
    }
    if (threads > max_threads)
    {
        throw std::invalid_argument("requested worker count exceeds 256 threads");
    }
    if (decimal_digits == 0)
        return "3.";

    Integer c_cubed_over_24;
    // The constant does not fit unsigned long on Windows (LLP64).
    mpz_set_str(c_cubed_over_24, "10939058860032000", 10);
    const auto workers = worker_count(threads);

    for (unsigned long guard = 16; guard <= 64; guard += 16)
    {
        const auto working_digits = static_cast<unsigned long>(decimal_digits) + guard;
        const auto terms = (working_digits + 20) / 14 + 1;
        Series series;
        Integer root;
        if (workers > 1 && decimal_digits >= 100'000)
        {
            auto future =
                std::async(std::launch::async, [=] { return scaled_square_root(working_digits); });
            series = split(0, terms, false, workers - 1, c_cubed_over_24);
            root = future.get();
        }
        else
        {
            series = split(0, terms, false, 1, c_cubed_over_24);
            root = scaled_square_root(working_digits);
        }

        // pi = 426880 * sqrt(10005) * Q / T. Everything is exact integer
        // arithmetic except the explicitly downward-rounded square root
        // and final quotient. GMP supplies tuned assembly and subquadratic
        // multiplication, division, square root, and decimal conversion.
        mpz_mul(series.q, series.q, root);
        mpz_mul_ui(series.q, series.q, 426'880);
        mpz_tdiv_q(series.q, series.q, series.t);
        // Release large, consumed operands before allocating decimal output.
        series.t = Integer {};
        root = Integer {};

        // Certification: p_k/q_k < 10^-14, so the alternating-series
        // remainder after N terms is < (A+B*N)*10^(-14*N). With N chosen
        // above, N < 8,000,000 and A+B*N < 5*10^15 even at max_digits.
        // Since S and its partial sum exceed 10^7 and 426880*sqrt(10005)
        // is below 5*10^7, the series error at scale 10^working_digits
        // is < 2.5*10^-11. Flooring the square root contributes < 0.043.
        // Therefore the quotient before flooring is within 1 of pi at
        // the working scale, and their integer floors differ by at most 1.
        // Discarding a tail other than all zeros/all nines certifies the
        // requested digits.
        std::string result(working_digits + 3, '\0');
        mpz_get_str(result.data() + 1, 10, series.q);
        const auto guard_begin = result.begin() + static_cast<std::ptrdiff_t>(decimal_digits + 2);
        const auto guard_end = result.begin() + static_cast<std::ptrdiff_t>(working_digits + 2);
        const bool near_lower_boundary =
            std::all_of(guard_begin, guard_end, [](char digit) { return digit == '0'; });
        const bool near_upper_boundary =
            std::all_of(guard_begin, guard_end, [](char digit) { return digit == '9'; });
        if (near_lower_boundary || near_upper_boundary)
            continue;

        // Reserving the leading byte lets us add the point without moving
        // millions of digits or allocating a second output buffer.
        result[0] = result[1];
        result[1] = '.';
        result.resize(decimal_digits + 2);
        return result;
    }
    throw std::runtime_error("could not certify the requested digits; increase guard precision");
}

} // namespace pitime
