#include "pitime/pi.hpp"

#include <gmp.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

// Keep the reference algorithm independent of the production Chudnovsky series.
// Only GMP's integer operations are shared with the implementation.
class Integer
{
  public:
    Integer()
    {
        mpz_init(value);
    }
    ~Integer()
    {
        mpz_clear(value);
    }
    Integer(const Integer&) = delete;
    Integer& operator=(const Integer&) = delete;

    mpz_t value;
};

void require(
    bool condition,
    const std::string& message
)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

// Return a fixed-point atan(1 / inverse). Each included term is rounded toward
// zero with error < 1 fixed-point unit. Once power is zero, the alternating
// series remainder is also < 1 unit. The caller can therefore bound the total
// absolute error by terms + 1 without assuming floating-point precision.
unsigned long arctangent(
    mpz_ptr result,
    mpz_srcptr scale,
    unsigned long inverse
)
{
    Integer power;
    Integer term;
    mpz_fdiv_q_ui(power.value, scale, inverse);
    mpz_set_ui(result, 0);
    unsigned long terms = 0;
    while (mpz_sgn(power.value) != 0)
    {
        mpz_fdiv_q_ui(term.value, power.value, 2 * terms + 1);
        if (terms % 2 == 0)
        {
            mpz_add(result, result, term.value);
        }
        else
        {
            mpz_sub(result, result, term.value);
        }
        mpz_fdiv_q_ui(power.value, power.value, inverse * inverse);
        ++terms;
    }
    return terms + 1;
}

std::string reference_pi(
    std::size_t digits
)
{
    constexpr unsigned long guard_digits = 32;
    Integer scale;
    Integer atan5;
    Integer atan239;
    Integer approximate;
    Integer lower;
    Integer upper;
    Integer guard_scale;
    mpz_ui_pow_ui(scale.value, 10, static_cast<unsigned long>(digits) + guard_digits);
    const auto error5 = arctangent(atan5.value, scale.value, 5);
    const auto error239 = arctangent(atan239.value, scale.value, 239);

    // Machin's identity: pi = 16 atan(1/5) - 4 atan(1/239).
    mpz_mul_ui(approximate.value, atan5.value, 16);
    mpz_submul_ui(approximate.value, atan239.value, 4);
    const auto error = 16 * error5 + 4 * error239;
    mpz_sub_ui(lower.value, approximate.value, error);
    mpz_add_ui(upper.value, approximate.value, error);
    mpz_ui_pow_ui(guard_scale.value, 10, guard_digits);
    mpz_fdiv_q(lower.value, lower.value, guard_scale.value);
    mpz_fdiv_q(upper.value, upper.value, guard_scale.value);
    require(
        mpz_cmp(lower.value, upper.value) == 0,
        "Independent oracle has insufficient guard digits"
    );

    std::vector<char> buffer(mpz_sizeinbase(lower.value, 10) + 2);
    mpz_get_str(buffer.data(), 10, lower.value);
    std::string result(buffer.data());
    result.insert(1, ".");
    require(result.size() == digits + 2, "Independent oracle has incorrect length");
    return result;
}

template <typename Operation>
void require_rejected(
    Operation operation,
    const std::string& label
)
{
    try
    {
        operation();
    }
    catch (const std::invalid_argument&)
    {
        return;
    }
    catch (const std::out_of_range&)
    {
        return;
    }
    throw std::runtime_error(label + " must reject an out-of-range argument");
}

} // namespace

int main()
{
    try
    {
        static_assert(pitime::default_digits == 10000, "Default digit count changed");
        static_assert(pitime::max_digits == 100000000, "Maximum digit count changed");

        const auto reference = reference_pi(10000);
        const std::string known100 =
            "3.14159265358979323846264338327950288419716939937510"
            "58209749445923078164062862089986280348253421170679";
        require(known100.size() == 102, "The 100-digit fixture has incorrect length");
        require(
            reference.substr(0, known100.size()) == known100,
            "Independent oracle disagrees with the known first 100 digits"
        );
        require(
            reference.substr(763, 6) == "999999",
            "Independent oracle disagrees with the six nines starting at digit 762"
        );

        std::vector<std::size_t> sizes {
            0,   1,   2,   3,   5,   9,   10,  15,  16,  17,  31,  32,  33,  99,   100,  101,  255,
            256, 257, 760, 761, 762, 763, 764, 765, 766, 767, 768, 769, 999, 1000, 1001, 10000
        };
        // A fixed generator makes failures reproducible on every standard library.
        unsigned long state = 0x50495449;
        for (unsigned i = 0; i < 24; ++i)
        {
            state = (1664525UL * state + 1013904223UL) & 0xffffffffUL;
            sizes.push_back(static_cast<std::size_t>(state % 2049));
        }
        std::sort(sizes.begin(), sizes.end());
        sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

        std::size_t comparisons = 0;
        for (const auto digits : sizes)
        {
            const auto expected = reference.substr(0, digits + 2);
            for (const unsigned threads : { 1U, 2U, 0U })
            {
                const auto actual = pitime::calculate_pi(digits, threads);
                require(
                    actual == expected,
                    "Wrong digits or output length for digits=" + std::to_string(digits) +
                        ", threads=" + std::to_string(threads)
                );
                ++comparisons;
            }
        }

        require(
            pitime::calculate_pi(100) == known100,
            "The default thread argument produces incorrect digits"
        );
        // Production deliberately runs small requests serially. This size also
        // exercises the square-root task and parallel binary-splitting branches.
        const auto large_serial = pitime::calculate_pi(100000, 1);
        require(
            large_serial.size() == 100002 &&
                large_serial.compare(0, reference.size(), reference) == 0,
            "The large serial calculation has incorrect length or prefix"
        );
        for (const unsigned threads : { 2U, 4U, 0U })
        {
            require(
                pitime::calculate_pi(100000, threads) == large_serial,
                "The parallel 100000-digit calculation differs from the serial result"
            );
        }
        require_rejected(
            [] { (void)pitime::calculate_pi(pitime::max_digits + 1, 1); },
            "Digit count above the supported maximum"
        );
        require_rejected(
            [] { (void)pitime::calculate_pi(std::numeric_limits<std::size_t>::max(), 1); },
            "Maximum representable digit count"
        );
        require_rejected(
            [] { (void)pitime::calculate_pi(1, 257); },
            "Thread count above the supported maximum"
        );
        require_rejected(
            [] { (void)pitime::calculate_pi(1, std::numeric_limits<unsigned>::max()); },
            "Maximum representable thread count"
        );

        std::cout
            << "Passed " << comparisons
            << " exact digit comparisons across serial, two-thread, and automatic modes; "
               "all 10000 digits independently verified, plus parallel parity at 100000 digits.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
