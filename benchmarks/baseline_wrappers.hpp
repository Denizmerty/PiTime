#pragma once

#include <string>

namespace pitime::benchmarks
{

std::string upstream_calculate_pi(int digits);
void local_print_pi(int digits);

} // namespace pitime::benchmarks
