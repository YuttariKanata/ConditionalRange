#include <ConditionalRange/ConditionalRange.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Condition = ConditionalRange::Condition;

volatile std::int64_t benchmark_sink = 0;

// ============================================================
// Helpers
// ============================================================

Condition make_condition(
    std::int64_t modulus,
    std::initializer_list<std::int64_t> allowed_residues
)
{
    std::vector<bool> allowed(
        static_cast<std::size_t>(modulus),
        false
    );

    for (const auto residue : allowed_residues) {
        allowed[static_cast<std::size_t>(residue)] = true;
    }

    return Condition(modulus, std::move(allowed));
}

Condition make_all_true_condition(std::int64_t modulus)
{
    return Condition(
        modulus,
        std::vector<bool>(
            static_cast<std::size_t>(modulus),
            true
        )
    );
}

Condition make_all_false_condition(std::int64_t modulus)
{
    return Condition(
        modulus,
        std::vector<bool>(
            static_cast<std::size_t>(modulus),
            false
        )
    );
}

// ============================================================
// Naive implementation
// ============================================================

bool naive_satisfies(
    std::int64_t value,
    const std::vector<Condition>& conditions
)
{
    for (const auto& condition : conditions) {
        std::int64_t residue = value % condition.modulus;

        if (residue < 0) {
            residue += condition.modulus;
        }

        if (!condition.allowed[
            static_cast<std::size_t>(residue)
        ]) {
            return false;
        }
    }

    return true;
}

std::int64_t run_naive(
    std::int64_t min,
    std::int64_t max,
    const std::vector<Condition>& conditions
)
{
    std::int64_t sum = 0;

    for (std::int64_t x = min; x <= max; ++x) {
        if (naive_satisfies(x, conditions)) {
            sum += x;
        }

        if (x == std::numeric_limits<std::int64_t>::max()) {
            break;
        }
    }

    return sum;
}

std::int64_t run_conditional_range(
    std::int64_t min,
    std::int64_t max,
    const std::vector<Condition>& conditions
)
{
    std::int64_t sum = 0;

    ConditionalRange range(min, max, conditions);

    for (const auto x : range) {
        sum += x;
    }

    return sum;
}

// ============================================================
// Benchmark
// ============================================================

template <typename Function>
double measure(
    Function&& function,
    std::int64_t& result,
    int repetitions
)
{
    double total_ms = 0.0;

    for (int i = 0; i < repetitions; ++i) {
        const auto begin = Clock::now();

        result = function();

        const auto end = Clock::now();

        total_ms += std::chrono::duration<double, std::milli>(
            end - begin
        ).count();
    }

    benchmark_sink += result;

    return total_ms / repetitions;
}

void print_result(
    const std::string& name,
    double naive_ms,
    double conditional_ms,
    std::int64_t naive_result,
    std::int64_t conditional_result
)
{
    std::cout
        << std::left
        << std::setw(32)
        << name
        << std::right
        << std::setw(12)
        << std::fixed
        << std::setprecision(3)
        << naive_ms
        << std::setw(14)
        << conditional_ms
        << std::setw(12)
        << (naive_ms / conditional_ms)
        << "x";

    if (naive_result != conditional_result) {
        std::cout << "  !!! RESULT MISMATCH !!!";
    }

    std::cout << '\n';
}

void benchmark_case(
    const std::string& name,
    std::int64_t min,
    std::int64_t max,
    const std::vector<Condition>& conditions,
    int repetitions = 5
)
{
    std::int64_t naive_result = 0;
    std::int64_t conditional_result = 0;

    const double naive_ms = measure(
        [&] {
            return run_naive(min, max, conditions);
        },
        naive_result,
        repetitions
    );

    const double conditional_ms = measure(
        [&] {
            return run_conditional_range(
                min,
                max,
                conditions
            );
        },
        conditional_result,
        repetitions
    );

    print_result(
        name,
        naive_ms,
        conditional_ms,
        naive_result,
        conditional_result
    );
}

// ============================================================
// Benchmark groups
// ============================================================

void benchmark_basic()
{
    std::cout << "\n[Basic]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(12)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(12)
        << "Speedup"
        << '\n';

    std::cout << std::string(70, '-') << '\n';

    benchmark_case(
        "no condition",
        -100'000,
        100'000,
        {}
    );

    benchmark_case(
        "always true",
        -100'000,
        100'000,
        {
            make_all_true_condition(7),
            make_all_true_condition(11)
        }
    );

    benchmark_case(
        "always false",
        -100'000,
        100'000,
        {
            make_all_false_condition(7)
        }
    );
}

void benchmark_density()
{
    std::cout << "\n[Density]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(12)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(12)
        << "Speedup"
        << '\n';

    std::cout << std::string(70, '-') << '\n';

    // 1 / 100
    benchmark_case(
        "sparse 1%",
        -100'000,
        100'000,
        {
            make_condition(
                100,
                {0}
            )
        }
    );

    // 10 / 100
    benchmark_case(
        "sparse 10%",
        -100'000,
        100'000,
        {
            make_condition(
                100,
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
            )
        }
    );

    // 50 / 100
    benchmark_case(
        "dense 50%",
        -100'000,
        100'000,
        {
            make_condition(
                2,
                {0}
            )
        }
    );

    // 90 / 100
    benchmark_case(
        "dense 90%",
        -100'000,
        100'000,
        {
            make_condition(
                10,
                {0, 1, 2, 3, 4, 5, 6, 7, 8}
            )
        }
    );
}

void benchmark_wheel()
{
    std::cout << "\n[Wheel]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(12)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(12)
        << "Speedup"
        << '\n';

    std::cout << std::string(70, '-') << '\n';

    // Period 6: residues 0, 2, 4.
    benchmark_case(
        "wheel period 6",
        -500'000,
        500'000,
        {
            make_condition(
                2,
                {0}
            ),
            make_condition(
                3,
                {0, 2}
            )
        }
    );

    // Period 30.
    benchmark_case(
        "wheel period 30",
        -500'000,
        500'000,
        {
            make_condition(
                2,
                {0, 1}
            ),
            make_condition(
                3,
                {0, 1}
            ),
            make_condition(
                5,
                {0, 1, 2, 3}
            )
        }
    );

    // A larger wheel.
    benchmark_case(
        "wheel period 210",
        -500'000,
        500'000,
        {
            make_condition(2,   {0}),
            make_condition(3,   {0, 1}),
            make_condition(5,   {0, 1, 2, 3}),
            make_condition(7,   {0, 1, 2, 3, 4, 5})
        }
    );
}

void benchmark_remaining_conditions()
{
    std::cout << "\n[Remaining conditions]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(12)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(12)
        << "Speedup"
        << '\n';

    std::cout << std::string(70, '-') << '\n';

    /*
     * The first conditions fit into the wheel.
     * The large modulus prevents the full combined period from
     * being incorporated.
     */
    benchmark_case(
        "wheel + remaining",
        -500'000,
        500'000,
        {
            make_condition(2, {0}),
            make_condition(3, {0, 1}),
            make_condition(5, {0, 1, 2, 3}),
            make_condition(7, {0, 1, 2, 3, 4, 5}),
            make_condition(11, {0, 1, 2, 3, 4}),
            make_condition(13, {0, 1, 2, 3, 4}),
            make_condition(17, {0, 1, 2, 3, 4}),
            make_condition(19, {0, 1, 2, 3, 4}),
            make_condition(23, {0, 1, 2, 3, 4}),
            make_condition(29, {0, 1, 2, 3, 4})
        }
    );
}

void benchmark_large_range()
{
    std::cout << "\n[Large range]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(12)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(12)
        << "Speedup"
        << '\n';

    std::cout << std::string(70, '-') << '\n';

    benchmark_case(
        "1,000,001 values",
        -500'000,
        500'000,
        {
            make_condition(3, {0, 1}),
            make_condition(5, {0, 1, 2, 3})
        }
    );

    benchmark_case(
        "10,000,001 values",
        -5'000'000,
        5'000'000,
        {
            make_condition(3, {0, 1}),
            make_condition(5, {0, 1, 2, 3})
        },
        3
    );
}

} // namespace

int main()
{
    std::cout
        << "ConditionalRange benchmark\n"
        << "===========================\n"
        << "version = "
        << ConditionalRange::VERSION
        << "\n";

    benchmark_basic();
    benchmark_density();
    benchmark_wheel();
    benchmark_remaining_conditions();
    benchmark_large_range();

    /*
     * Prevent the compiler from removing all benchmark work.
     */
    std::cout
        << "\nbenchmark checksum = "
        << benchmark_sink
        << '\n';

    return 0;
}