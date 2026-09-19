#include <iostream>
#include <chrono>

void printPiDigits(
    int n
)
{
    if (n <= 0)
    {
        std::cout << "3.";
        return;
    }

    int len = 10 * n / 3 + 3;

    int* a = nullptr;
    int* calculated_digits = nullptr;
    try
    {
        a = new int[len];
        calculated_digits = new int[n + 5];
    }
    catch (const std::bad_alloc& e)
    {
        std::cerr << "Error: Memory allocation failed. " << e.what() << std::endl;
        delete[] a;
        return;
    }

    for (int i = 0; i < len; ++i)
    {
        a[i] = 2;
    }

    int digits_count = 0;
    int nines = 0;
    int predigit = 0;

    for (int j = 0; j < n + 3; ++j)
    {
        long long carry = 0;
        for (int i = len - 1; i > 0; --i)
        {
            long long num = (long long)a[i] * 10 + carry;
            a[i] = static_cast<int>(num % (2 * i + 1));
            carry = num / (2 * i + 1) * i;
        }
        long long final_num = (long long)a[0] * 10 + carry;
        int q = static_cast<int>(final_num / 10);
        a[0] = static_cast<int>(final_num % 10);

        if (q >= 10)
        {
            q = 10;
        }

        if (j > 0)
        {
            if (q < 9)
            {
                if (digits_count < n + 5)
                    calculated_digits[digits_count++] = predigit;
                for (int k = 0; k < nines; ++k)
                {
                    if (digits_count < n + 5)
                        calculated_digits[digits_count++] = 9;
                    else
                        break;
                }
                predigit = q;
                nines = 0;
            }
            else if (q == 9)
            {
                nines++;
            }
            else
            {
                if (digits_count < n + 5)
                    calculated_digits[digits_count++] = predigit + 1;
                for (int k = 0; k < nines; ++k)
                {
                    if (digits_count < n + 5)
                        calculated_digits[digits_count++] = 0;
                    else
                        break;
                }
                predigit = 0;
                nines = 0;
            }
        }
        else
        {
            predigit = q;
        }

        if (digits_count >= n + 1)
        {
            break;
        }
    }

    if (digits_count > 0)
    {
        std::cout << calculated_digits[0] << ".";
    }
    else
    {
        std::cout << "3.";
    }

    int num_decimal_digits_available = (digits_count > 1) ? digits_count - 1 : 0;
    int digits_to_print = (n < num_decimal_digits_available) ? n : num_decimal_digits_available;

    for (int i = 0; i < digits_to_print; ++i)
    {
        std::cout << calculated_digits[i + 1];
    }

    delete[] a;
    delete[] calculated_digits;
}

int main()
{
    const int N = 10000;

    auto start_time = std::chrono::high_resolution_clock::now();

    printPiDigits(N);
    std::cout << std::endl;

    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Calculation took " << duration.count() << " milliseconds." << std::endl;

    return 0;
}
