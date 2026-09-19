#pragma once

#include <cstddef>
#include <string>

namespace pitime
{

inline constexpr std::size_t default_digits = 10'000;
inline constexpr std::size_t max_digits = 100'000'000;
inline constexpr unsigned max_threads = 256;

// Return 3 followed by a decimal point and exactly decimal_digits digits.
// Digits are truncated, never rounded. threads == 0 selects a bounded automatic
// worker count; threads == 1 disables parallel calculation.
// Throws std::invalid_argument if decimal_digits exceeds max_digits or threads
// exceeds max_threads. Small calculations run serially to avoid task overhead.
[[nodiscard]] std::string calculate_pi(std::size_t decimal_digits, unsigned threads = 0);

} // namespace pitime
