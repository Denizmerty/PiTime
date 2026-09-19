// Compile the pinned, unmodified source with the same settings as the new code.
#define main pitime_upstream_original_main
#include "upstream/PiTime.cpp"
#undef main

#include "baseline_wrappers.hpp"

std::string pitime::benchmarks::upstream_calculate_pi(
    int digits
)
{
    return calculatePiDigitsString(digits);
}
