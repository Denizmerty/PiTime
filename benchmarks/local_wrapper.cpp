// Keep the original checkout's implementation intact, including its formatting.
#define main pitime_local_original_main
#include "local/PiTime.cpp"
#undef main

#include "baseline_wrappers.hpp"

void pitime::benchmarks::local_print_pi(
    int digits
)
{
    printPiDigits(digits);
}
