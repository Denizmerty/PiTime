#include "pitime/pi.hpp"

#include <charconv>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
std::size_t parse_number(
    std::string_view text,
    std::string_view option
)
{
    std::size_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc {} || result.ptr != text.data() + text.size())
    {
        throw std::invalid_argument(std::string(option) + " requires a nonnegative integer");
    }
    return value;
}

void usage()
{
    std::cout
        << "PiTime " PITIME_VERSION "\n"
           "Usage: pitime [--digits N] [--threads N] [--quiet] [--output FILE]\n"
           "  --digits N    Decimal places, 0..100000000 (default 10000)\n"
           "  --threads N   Worker limit, 0..256 (0 selects automatically)\n"
           "  --quiet       Suppress digits on stdout; still calculate every digit\n"
           "  --output FILE Write digits to a file instead of stdout\n"
           "  --help        Show this help\n"
           "  --version     Show version\n"
           "Digits are truncated, not rounded. Calculation timing goes to stderr.\n";
}
} // namespace

int main(
    int argc,
    char** argv
)
{
    std::ios::sync_with_stdio(false);
    try
    {
        std::size_t digits = pitime::default_digits;
        unsigned threads = 0;
        bool quiet = false;
        std::string output_path;
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view option(argv[i]);
            if (option == "--help")
            {
                usage();
                return 0;
            }
            if (option == "--version")
            {
                std::cout << "PiTime " PITIME_VERSION "\n";
                return 0;
            }
            if (option == "--quiet")
            {
                quiet = true;
                continue;
            }
            if (option != "--digits" && option != "--threads" && option != "--output")
            {
                throw std::invalid_argument("unknown option: " + std::string(option));
            }
            if (++i >= argc)
            {
                throw std::invalid_argument("missing value for " + std::string(option));
            }
            const std::string_view value(argv[i]);
            if (option == "--digits")
            {
                digits = parse_number(value, option);
                if (digits > pitime::max_digits)
                {
                    throw std::invalid_argument("--digits exceeds 100000000");
                }
            }
            else if (option == "--threads")
            {
                const auto count = parse_number(value, option);
                if (count > pitime::max_threads)
                {
                    throw std::invalid_argument("--threads exceeds 256");
                }
                threads = static_cast<unsigned>(count);
            }
            else
            {
                if (value.empty())
                {
                    throw std::invalid_argument("--output requires a nonempty path");
                }
                output_path = value;
            }
        }

        const auto start = std::chrono::steady_clock::now();
        const std::string pi = pitime::calculate_pi(digits, threads);
        const auto elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();

        if (!output_path.empty())
        {
            std::ofstream file(output_path, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("cannot open output file: " + output_path);
            }
            file.write(pi.data(), static_cast<std::streamsize>(pi.size()));
            file.put('\n');
            file.close();
            if (!file)
            {
                throw std::runtime_error("cannot write output file: " + output_path);
            }
        }
        else if (!quiet)
        {
            std::cout.write(pi.data(), static_cast<std::streamsize>(pi.size()));
            std::cout.put('\n');
            std::cout.flush();
            if (!std::cout)
            {
                throw std::runtime_error("cannot write digits to stdout");
            }
        }
        std::cerr << "Calculation took " << std::fixed << std::setprecision(3) << elapsed
                  << " milliseconds (" << digits << " decimal places).\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "PiTime: " << error.what() << '\n';
        return 1;
    }
}
