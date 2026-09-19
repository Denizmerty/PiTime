#include "baseline_wrappers.hpp"
#include "pitime/pi.hpp"

#include <gmp.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;
constexpr double minimum_sample_seconds = 0.010;
constexpr std::size_t batch_memory_budget = 64U * 1024U * 1024U;
volatile std::uint64_t consumed_hash = 0;

struct Options
{
    std::size_t digits = 10000;
    unsigned samples = 7;
    unsigned threads = 1;
    bool optimized_only = false;
    bool allow_slower = false;
    std::string csv_path;
};

std::size_t parse_number(
    std::string_view value,
    std::string_view option
)
{
    std::size_t number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc {} || result.ptr != value.data() + value.size())
    {
        throw std::invalid_argument("Invalid nonnegative integer for " + std::string(option));
    }
    return number;
}

void print_help()
{
    std::cout
        << "Usage: pitime_benchmark [--digits N] [--samples N] [--threads N]\n"
           "                        [--optimized-only] [--allow-slower] [--csv PATH]\n"
           "Defaults: 10000 decimal digits, 7 samples, 1 optimized thread.\n"
           "Every sample contains at least 10 ms of measured calculation.\n"
           "--threads 0 permits automatic thread selection.\n"
           "--optimized-only skips the quadratic reference implementations.\n"
           "--allow-slower disables the upstream median speedup failure.\n"
           "CSV rows go to stdout; metadata/summary lines start with '#'.\n"
           "--csv also writes just the CSV header and data rows to PATH.\n";
}

Options parse_options(
    int argc,
    char** argv
)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view option(argv[i]);
        if (option == "--optimized-only")
        {
            options.optimized_only = true;
        }
        else if (option == "--allow-slower")
        {
            options.allow_slower = true;
        }
        else if (
            option == "--digits" || option == "--samples" || option == "--threads" ||
            option == "--csv"
        )
        {
            if (++i == argc)
            {
                throw std::invalid_argument("Missing value for " + std::string(option));
            }
            if (option == "--csv")
            {
                options.csv_path = argv[i];
                continue;
            }
            const auto number = parse_number(argv[i], option);
            if (option == "--digits")
            {
                options.digits = number;
            }
            else
            {
                if (number > std::numeric_limits<unsigned>::max() ||
                    (option == "--samples" && number == 0))
                {
                    throw std::invalid_argument("Out-of-range value for " + std::string(option));
                }
                if (option == "--samples")
                {
                    options.samples = static_cast<unsigned>(number);
                }
                else
                {
                    options.threads = static_cast<unsigned>(number);
                }
            }
        }
        else
        {
            throw std::invalid_argument("Unknown option: " + std::string(option));
        }
    }
    // Validate before invoking a potentially expensive quadratic baseline.
    if (options.digits > pitime::max_digits || options.threads > pitime::max_threads)
    {
        throw std::invalid_argument("Digits or threads exceed the calculation API limits");
    }
    // The untouched local source computes 10*n using signed int arithmetic.
    if (!options.optimized_only &&
        options.digits > static_cast<std::size_t>(std::numeric_limits<int>::max() / 10))
    {
        throw std::invalid_argument("Digit count exceeds the original implementation's safe range");
    }
    return options;
}

std::uint64_t hash_output(
    std::string_view value
)
{
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char ch : value)
    {
        hash = (hash ^ ch) * UINT64_C(1099511628211);
    }
    return hash;
}

void validate_output(
    std::string_view output,
    std::string_view expected,
    std::string_view implementation
)
{
    if (output != expected)
    {
        const auto mismatch =
            std::mismatch(output.begin(), output.end(), expected.begin(), expected.end());
        throw std::runtime_error(
            std::string(implementation) + " output mismatch at byte " +
            std::to_string(mismatch.first - output.begin()) + " (actual length " +
            std::to_string(output.size()) + ", expected " + std::to_string(expected.size()) + ")"
        );
    }
    consumed_hash = consumed_hash * UINT64_C(1099511628211) + hash_output(output);
}

// The old checkout writes digits one at a time to cout. Capture those writes in
// memory so it pays for integer formatting/string allocation, not a terminal.
class StringSink final : public std::streambuf
{
  public:
    std::string take()
    {
        std::string result;
        result.swap(output_);
        return result;
    }

  protected:
    int_type overflow(
        int_type ch
    ) override
    {
        if (!traits_type::eq_int_type(ch, traits_type::eof()))
        {
            output_.push_back(traits_type::to_char_type(ch));
        }
        return traits_type::not_eof(ch);
    }

    std::streamsize xsputn(
        const char* data,
        std::streamsize count
    ) override
    {
        output_.append(data, static_cast<std::size_t>(count));
        return count;
    }

  private:
    std::string output_;
};

class CoutCapture final
{
  public:
    explicit CoutCapture(
        std::streambuf* buffer
    )
        : original_(std::cout.rdbuf(buffer))
    {
    }
    ~CoutCapture()
    {
        std::cout.rdbuf(original_);
    }
    CoutCapture(const CoutCapture&) = delete;
    CoutCapture& operator=(const CoutCapture&) = delete;

  private:
    std::streambuf* original_;
};

struct Measurement
{
    double seconds = 0;
    std::size_t calls = 0;

    double milliseconds_per_call() const
    {
        return 1000.0 * seconds / static_cast<double>(calls);
    }
};

template <class Calculate>
Measurement measure(
    Calculate&& calculate,
    std::string_view expected,
    std::string_view name,
    bool warmup = false
)
{
    Measurement measurement;
    std::size_t batch_calls = 1;
    // Limit retained strings for high digit counts. At least one fresh result is
    // necessary regardless of its size; no digit results are cached or reused.
    const auto bytes_per_result =
        expected.size() > std::numeric_limits<std::size_t>::max() - sizeof(std::string)
        ? std::numeric_limits<std::size_t>::max()
        : expected.size() + sizeof(std::string);
    const auto maximum_batch_calls =
        std::max<std::size_t>(1, batch_memory_budget / std::max<std::size_t>(1, bytes_per_result));

    do
    {
        std::vector<std::string> outputs;
        outputs.reserve(batch_calls);
        const auto start = Clock::now();
        for (std::size_t call = 0; call < batch_calls; ++call)
        {
            outputs.emplace_back(calculate());
        }
        const auto finish = Clock::now();
        const double elapsed = std::chrono::duration<double>(finish - start).count();
        measurement.seconds += elapsed;
        measurement.calls += batch_calls;

        // Every result is compared in full and consumed, outside the timer.
        for (const auto& output : outputs)
        {
            validate_output(output, expected, name);
        }
        if (warmup || measurement.seconds >= minimum_sample_seconds)
        {
            break;
        }
        const auto seconds_per_call = measurement.seconds / static_cast<double>(measurement.calls);
        const auto remaining_calls = std::ceil(
            (minimum_sample_seconds - measurement.seconds) / std::max(seconds_per_call, 1e-9)
        );
        batch_calls = static_cast<std::size_t>(
            std::min(std::max(1.0, remaining_calls), static_cast<double>(maximum_batch_calls))
        );
    }
    while (true);
    return measurement;
}

Measurement measure_implementation(
    unsigned implementation,
    const Options& options,
    std::string_view expected,
    bool warmup = false
)
{
    if (implementation == 0)
    {
        return measure(
            [&]
            { return pitime::benchmarks::upstream_calculate_pi(static_cast<int>(options.digits)); },
            expected,
            "upstream",
            warmup
        );
    }
    if (implementation == 1)
    {
        StringSink sink;
        // Installing/restoring the streambuf is outside every timed region.
        CoutCapture capture(&sink);
        return measure(
            [&]
            {
                pitime::benchmarks::local_print_pi(static_cast<int>(options.digits));
                return sink.take();
            },
            expected,
            "local",
            warmup
        );
    }
    return measure(
        [&] { return pitime::calculate_pi(options.digits, options.threads); },
        expected,
        "optimized",
        warmup
    );
}

double median(
    std::vector<double> values
)
{
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) / 2 : values[middle];
}

void print_metadata(
    const Options& options
)
{
    std::cout
        << "# PiTime benchmark; steady_clock; timing includes calculation, allocation, and decimal "
           "formatting\n"
        << "# digits=" << options.digits << " samples=" << options.samples << " requested_threads="
        << options.threads << " hardware_threads=" << std::thread::hardware_concurrency() << '\n'
        << "# GMP=" << gmp_version << " limb_bits=" << GMP_LIMB_BITS << '\n';
#if defined(__clang__)
    std::cout << "# compiler=Clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
    std::cout << "# compiler=GCC " << __VERSION__ << '\n';
#elif defined(_MSC_VER)
    std::cout << "# compiler=MSVC " << _MSC_FULL_VER << '\n';
#else
    std::cout << "# compiler=unknown\n";
#endif
#ifdef NDEBUG
    std::cout << "# NDEBUG=1\n";
#else
    std::cout << "# NDEBUG=0; use a Release build for performance comparisons\n";
#endif
    if (options.optimized_only)
    {
        std::cout
            << "# validation=full output equality against an untimed optimized single-thread run; "
               "no independent reference in scaling mode\n";
    }
    else
    {
        std::cout
            << "# upstream_commit=b3fd7ff94939756a131a63a217553639e8ded70d\n"
               "# "
               "upstream_original_sha256="
               "EA16932AD72F14FD976F3AD7C7B8998B111AD1B8E266FE92AB97EDD5477B7AFF\n"
               "# "
               "local_original_sha256="
               "DA3A79556758EC0DE56C04A19DAA8306D82992148744312D95CCA39AC1DD3034\n"
               "# baseline_sources=original implementations with whitespace-only formatting "
               "changes\n"
               "# validation=full output equality against the upstream algorithm for every "
               "measured "
               "call\n"
               "# local cout writes are captured in memory; terminal I/O is excluded for all "
               "implementations\n";
    }
}

int run(
    const Options& options
)
{
    std::ofstream csv;
    if (!options.csv_path.empty())
    {
        csv.open(options.csv_path, std::ios::out | std::ios::trunc);
        if (!csv)
        {
            throw std::runtime_error("Cannot open CSV output: " + options.csv_path);
        }
    }
    print_metadata(options);
    const auto expected = options.optimized_only
        ? pitime::calculate_pi(options.digits, 1)
        : pitime::benchmarks::upstream_calculate_pi(static_cast<int>(options.digits));
    if (expected.size() != options.digits + 2 || expected.substr(0, 2) != "3.")
    {
        throw std::runtime_error("Reference did not return the requested number of decimal digits");
    }
    const auto expected_hash = hash_output(expected);
    constexpr std::array<std::string_view, 3> names = { "upstream", "local", "optimized" };
    const unsigned first = options.optimized_only ? 2U : 0U;
    for (unsigned implementation = first; implementation < 3; ++implementation)
    {
        measure_implementation(implementation, options, expected, true);
    }
    std::cout
        << "# warmup=one discarded call per implementation; samples rotate implementation order\n";
    constexpr std::string_view header =
        "digits,threads,sample,implementation,calls,total_ms,ms_per_call,output_fnv1a64\n";
    std::cout << header << std::setprecision(12);
    if (csv)
    {
        csv << header << std::setprecision(12);
    }
    std::array<std::vector<double>, 3> timings;
    for (unsigned sample = 0; sample < options.samples; ++sample)
    {
        for (unsigned position = first; position < 3; ++position)
        {
            const auto implementation = options.optimized_only ? 2U : (sample + position) % 3;
            const auto result = measure_implementation(implementation, options, expected);
            timings[implementation].push_back(result.milliseconds_per_call());
            const auto row = [&](std::ostream& stream)
            {
                stream << options.digits << ',' << (implementation == 2 ? options.threads : 1U)
                       << ',' << sample + 1 << ',' << names[implementation] << ',' << result.calls
                       << ',' << result.seconds * 1000.0 << ',' << result.milliseconds_per_call()
                       << ',' << std::hex << expected_hash << std::dec << '\n';
            };
            row(std::cout);
            if (csv)
            {
                row(csv);
            }
        }
    }
    if (csv.is_open())
    {
        csv.flush();
        if (!csv)
        {
            throw std::runtime_error("Failed to write CSV output: " + options.csv_path);
        }
    }
    std::array<double, 3> medians {};
    std::array<double, 3> minima {};
    for (unsigned implementation = first; implementation < 3; ++implementation)
    {
        medians[implementation] = median(timings[implementation]);
        minima[implementation] =
            *std::min_element(timings[implementation].begin(), timings[implementation].end());
        std::cout << "# " << names[implementation] << " median_ms=" << medians[implementation]
                  << " min_ms=" << minima[implementation] << '\n';
    }
    if (!options.optimized_only)
    {
        std::cout << "# speedup_vs_upstream_median=" << medians[0] / medians[2]
                  << " speedup_vs_upstream_min=" << minima[0] / minima[2] << '\n'
                  << "# speedup_vs_local_median=" << medians[1] / medians[2]
                  << " speedup_vs_local_min=" << minima[1] / minima[2] << '\n';
        if (medians[2] >= medians[0] && !options.allow_slower)
        {
            std::cerr << "Optimized median did not beat upstream; benchmark failed.\n";
            return 2;
        }
    }
    std::cout << "# validation=passed consumed_hash=" << std::hex << consumed_hash << std::dec
              << '\n';
    return 0;
}

} // namespace

int main(
    int argc,
    char** argv
)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--help")
        {
            print_help();
            return 0;
        }
        return run(parse_options(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 1;
    }
}
