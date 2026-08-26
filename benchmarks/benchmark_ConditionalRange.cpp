// This file is [benchmark_ConditionalRange.cpp]

#include <ConditionalRange/ConditionalRange.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Condition = cr::Condition;

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

Condition make_density_condition(
    std::int64_t modulus,
    std::int64_t allowed_count
)
{
    std::vector<bool> allowed(
        static_cast<std::size_t>(modulus),
        false
    );

    for (std::int64_t i = 0; i < allowed_count; ++i) {
        allowed[static_cast<std::size_t>(i)] = true;
    }

    return Condition(
        modulus,
        std::move(allowed)
    );
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

std::int64_t lcm_int64(
    std::int64_t a,
    std::int64_t b
)
{
    return std::lcm(a,b);
}

/*
 * Calculate the exact selectivity of a set of conditions by
 * enumerating one complete combined period.
 *
 * This is intentionally independent from ConditionalRange.
 * The benchmark must not use the implementation under test
 * to calculate its expected selectivity.
 */
double calculate_composite_selectivity(
    const std::vector<Condition>& conditions
)
{
    if (conditions.empty()) {
        return 1.0;
    }

    std::int64_t period = 1;

    for (const auto& condition : conditions) {
        period = lcm_int64(period, condition.modulus);
    }

    std::int64_t accepted = 0;

    for (std::int64_t x = 0; x < period; ++x) {
        bool satisfies = true;

        for (const auto& condition : conditions) {
            std::int64_t residue = x % condition.modulus;

            if (residue < 0) {
                residue += condition.modulus;
            }

            if (!condition.allowed[
                static_cast<std::size_t>(residue)
            ]) {
                satisfies = false;
                break;
            }
        }

        if (satisfies) {
            ++accepted;
        }
    }

    return static_cast<double>(accepted)
         / static_cast<double>(period);
}

/*
 * Calculate the naive "independent probability product".
 *
 * This is useful for comparison, but it is NOT used as the
 * authoritative answer when moduli are not pairwise coprime.
 */
double calculate_independent_product(
    const std::vector<Condition>& conditions
)
{
    double result = 1.0;

    for (const auto& condition : conditions) {
        const auto allowed_count =
            static_cast<std::int64_t>(
                std::count(
                    condition.allowed.begin(),
                    condition.allowed.end(),
                    true
                )
            );

        result *=
            static_cast<double>(allowed_count)
            / static_cast<double>(condition.modulus);
    }

    return result;
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

void benchmark_composite_case(
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

    const double selectivity =
        calculate_composite_selectivity(conditions);

    const double independent =
        calculate_independent_product(conditions);

    std::cout
        << std::left
        << std::setw(32)
        << name
        << std::right
        << std::setw(11)
        << std::fixed
        << std::setprecision(6)
        << (selectivity * 100.0)
        << "%"
        << std::setw(15)
        << (independent * 100.0)
        << "%"
        << std::setw(13)
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
            make_condition(2,  {0}),
            make_condition(3,  {0, 1}),
            make_condition(5,  {0, 1, 2, 3}),
            make_condition(7,  {0, 1, 2, 3, 4, 5}),
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

void benchmark_selectivity()
{
    std::cout << "\n[Selectivity]\n";

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

    constexpr std::int64_t min = -100'000;
    constexpr std::int64_t max = 100'000;
    constexpr std::int64_t modulus = 10'000;

    const std::vector<std::pair<const char*, std::int64_t>> cases{
        {"0%",       0},
        {"0.01%",    1},
        {"0.1%",    10},
        {"0.5%",    50},
        {"1%",      100},
        {"2%",      200},
        {"3%",      300},
        {"5%",      500},
        {"7%",      700},
        {"10%",    1000},
        {"20%",    2000},
        {"40%",    4000},
        {"100%",  10000},
    };

    for (const auto& [name, allowed_count] : cases) {
        benchmark_case(
            name,
            min,
            max,
            {
                make_density_condition(
                    modulus,
                    allowed_count
                )
            }
        );
    }
}

// ============================================================
// Composite selectivity
// ============================================================

void benchmark_composite_selectivity()
{
    std::cout << "\n[Composite selectivity]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(12)
        << "Selectivity"
        << std::setw(16)
        << "Independent"
        << std::setw(13)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(13)
        << "Speedup"
        << '\n';

    std::cout << std::string(82, '-') << '\n';

    constexpr std::int64_t min = -500'000;
    constexpr std::int64_t max = 500'000;

    struct TestCase {
        const char* name;
        std::vector<Condition> conditions;
    };

    const std::vector<TestCase> cases{

        /*
         * 1/2 * 2/3 * 1/5 * 5/7
         *
         * Since all moduli are pairwise coprime,
         * the exact selectivity is 1/21 = 4.7619%.
         */
        {
            "2,3,5,7",
            {
                make_condition(2, {0}),
                make_condition(3, {0, 1}),
                make_condition(5, {0}),
                make_condition(7, {0, 1, 2, 3, 4})
            }
        },

        /*
         * Approximately 1%.
         *
         * 1/2 * 1/3 * 1/5 * 1/7 * 3/11
         * = 1/770 ≈ 0.12987%
         *
         * The important point is that the program calculates
         * the actual value rather than relying on this comment.
         */
        {
            "very sparse",
            {
                make_condition(2, {0}),
                make_condition(3, {0}),
                make_condition(5, {0}),
                make_condition(7, {0}),
                make_condition(11, {0, 1, 2})
            }
        },

        /*
         * A roughly 10% composite condition.
         */
        {
            "roughly 10%",
            {
                make_condition(2, {0}),
                make_condition(3, {0, 1}),
                make_condition(5, {0, 1, 2})
            }
        },

        /*
         * A roughly 1% condition with more factors.
         */
        {
            "many conditions",
            {
                make_condition(2, {0}),
                make_condition(3, {0}),
                make_condition(5, {0, 1}),
                make_condition(7, {0, 1}),
                make_condition(11, {0, 1, 2, 3})
            }
        },

        /*
         * Non-coprime moduli.
         *
         * This case is particularly important:
         *
         * modulus 4 : 2/4
         * modulus 6 : 3/6
         *
         * Their events are NOT necessarily independent.
         */
        {
            "non-coprime 4,6",
            {
                make_condition(4, {0, 1}),
                make_condition(6, {0, 1, 2})
            }
        },

        /*
         * Another non-coprime case.
         */
        {
            "non-coprime 6,9,15",
            {
                make_condition(6, {0, 1}),
                make_condition(9, {0, 1, 2}),
                make_condition(15, {0, 1, 2, 3, 4})
            }
        },

        // The ones I made by hand
        {"case1     1/2 1/3 1/5 1/7 1/11", {
            make_condition(2,  {0}),
            make_condition(3,  {0}),
            make_condition(5,  {0}),
            make_condition(7,  {0}),
            make_condition(11, {0})
        }},
        {"case2     1/2 1/3 1/5 1/7", {
            make_condition(2,  {0}),
            make_condition(3,  {0}),
            make_condition(5,  {0}),
            make_condition(7,  {0})
        }},
        {"case3     1/2 1/3 1/5 2/7", {
            make_condition(2,  {0}),
            make_condition(3,  {0}),
            make_condition(5,  {0}),
            make_condition(7,  {0, 1})
        }},
        {"case4     1/2 1/3 2/5 1/7", {
            make_condition(2, {0}),
            make_condition(3, {0}),
            make_condition(5, {0, 1}),
            make_condition(7, {0})
        }},
        {"case5     1/2 2/3 1/5 1/7", {
            make_condition(2, {0}),
            make_condition(3, {0, 1}),
            make_condition(5, {0}),
            make_condition(7, {0})
        }},
        {"case6     1/2 2/3 1/5 5/7", {
            make_condition(2, {0}),
            make_condition(3, {0, 1}),
            make_condition(5, {0}),
            make_condition(7, {0, 1, 2, 3, 4})
        }},
        {"case7     1/2 2/3 1/5 6/7", {
            make_condition(2, {0}),
            make_condition(3, {0, 1}),
            make_condition(5, {0}),
            make_condition(7, {0, 1, 2, 3, 4, 5})
        }},
        {"case8     1/2 1/3 3/5", {
            make_condition(2, {0}),
            make_condition(3, {0}),
            make_condition(5, {0, 1, 2})
        }},
        {"case9     2/3 1/5", {
            make_condition(3, {0, 1}),
            make_condition(5, {0})
        }},
        {"case10    1/2 2/3 3/5", {
            make_condition(2, {0}),
            make_condition(3, {0, 1}),
            make_condition(5, {0, 1, 2})
        }},
        {"case11    2/3 2/5", {
            make_condition(3, {0, 1}),
            make_condition(5, {0, 1})
        }},
        {"case12    1/2 3/5", {
            make_condition(2, {0}),
            make_condition(5, {0, 1, 2})
        }},
        {"case13    2/3 3/5", {
            make_condition(3, {0, 1}),
            make_condition(5, {0, 1, 2})
        }},
        {"case14    2/3 7/10", {
            make_condition(3,  {0, 1}),
            make_condition(10, {0, 1, 2, 3, 4, 5, 6})
        }},
        {"case15    2/3 9/10", {
            make_condition(3,  {0, 1}),
            make_condition(10, {0, 1, 2, 3, 4, 5, 6, 7, 8})
        }},
        {"case16    3/3 9/10", {
            make_condition(3,  {0, 1, 2}),
            make_condition(10, {0, 1, 2, 3, 4, 5, 6, 7, 8})
        }},
        {"case17    2/2 4/5", {
            make_condition(2, {0, 1}),
            make_condition(5, {0, 1, 2, 3})
        }},
        {"case18    3/3 9/10", {
            make_condition(3, {0, 1, 2}),
            make_condition(10, {0, 1, 2, 3, 4, 5, 6, 7, 8})
        }},
        {"case19    7/11 8/13", {
            make_condition(11, {0, 1, 2, 3, 4, 5, 6}),
            make_condition(13, {0, 1, 2, 3, 4, 5, 6, 7})
        }},
        {"case20    16/31 21/41", {
            make_condition(31, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}),
            make_condition(41, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20})
        }},
        {"case21    21/41 21/97", {
            make_condition(41, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}),
            make_condition(97, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20})
        }},
        {"case22    21/97 21/103", {
            make_condition(97,  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}),
            make_condition(103, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20})
        }},
        {"case23    21/997 21/996", {
            make_condition(997, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}),
            make_condition(996, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20})
        }},
    };

    for (const auto& test : cases) {
        benchmark_composite_case(
            test.name,
            min,
            max,
            test.conditions
        );
    }
}

void benchmark_same_selectivity_structure()
{
    std::cout << "\n[Same selectivity / Different structure]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(13)
        << "Selectivity"
        << std::setw(16)
        << "Independent"
        << std::setw(13)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(13)
        << "Speedup"
        << '\n';

    std::cout << std::string(101, '-') << '\n';

    constexpr std::int64_t min = -500'000;
    constexpr std::int64_t max = 500'000;

    struct TestCase {
        const char* name;
        std::vector<Condition> conditions;
    };

    /*
     * All cases below have exactly 20% selectivity,
     * but their condition structures are different.
     */

    const std::vector<TestCase> cases{
        {"1/5", {
            make_condition(5, {0})
        }},
        {"1/2 * 2/5",{
            make_condition(2, {0}),
            make_condition(5, {0, 1})
        }},
        {"2/3 * 3/10",{
            make_condition(3, {0, 1}),
            make_condition(10, {0, 1, 2})
        }},
        {"4/5 * 1/4",{
            make_condition(5, {0, 1, 2, 3}),
            make_condition(4, {0})
        }},
        {"4/5 * 1/2 * 1/2",{
            make_condition(5, {0, 1, 2, 3}),
            make_condition(2, {0}),
            make_condition(4, {0, 1})
        }},
        {"2/5 * 1/2 * 1",{
            make_condition(5, {0, 1}),
            make_condition(2, {0}),
            make_all_true_condition(7)
        }}
    };

    for (const auto& test : cases) {
        benchmark_composite_case(
            test.name,
            min,
            max,
            test.conditions
        );
    }
}

void benchmark_large_modulus_composite()
{
    std::cout << "\n[Large modulus composite]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(13)
        << "Selectivity"
        << std::setw(16)
        << "Independent"
        << std::setw(13)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(13)
        << "Speedup"
        << '\n';

    std::cout << std::string(101, '-') << '\n';

    constexpr std::int64_t min = -500'000;
    constexpr std::int64_t max = 500'000;

    struct TestCase {
        const char* name;
        std::vector<Condition> conditions;
    };

    const std::vector<TestCase> cases{
        {"6/11 * 6/13",{
            make_density_condition(11, 6),
            make_density_condition(13, 6)
        }},
        {"16/31 * 20/41",{
            make_density_condition(31, 16),
            make_density_condition(41, 20)
        }},
        {"21/41 * 48/97",{
            make_density_condition(41, 21),
            make_density_condition(97, 48)
        }},
        {"49/97 * 51/103",{
            make_density_condition(97, 49),
            make_density_condition(103, 51)
        }},
        {"21/41 * 21/97",{
            make_density_condition(41, 21),
            make_density_condition(97, 21)
        }},
        {"21/97 * 21/103",{
            make_density_condition(97, 21),
            make_density_condition(103, 21)
        }},
        {"21/997 * 21/996",{
            make_density_condition(997, 21),
            make_density_condition(996, 21)
        }},
        {"21/97 * 21/96 * 21/95",{
            make_density_condition(97, 21),
            make_density_condition(96, 21),
            make_density_condition(95, 21)
        }},
    };

    for (const auto& test : cases) {
        benchmark_composite_case(
            test.name,
            min,
            max,
            test.conditions
        );
    }
}

void benchmark_condition_ordering()
{
    std::cout << "\n[Condition ordering]\n";

    std::cout
        << std::left
        << std::setw(32)
        << "Case"
        << std::right
        << std::setw(13)
        << "Selectivity"
        << std::setw(16)
        << "Independent"
        << std::setw(13)
        << "Naive ms"
        << std::setw(14)
        << "CR ms"
        << std::setw(13)
        << "Speedup"
        << '\n';

    std::cout << std::string(101, '-') << '\n';

    constexpr std::int64_t min = -500'000;
    constexpr std::int64_t max = 500'000;

    /*
     * Same logical condition set:
     *
     *   A: 1/2
     *   B: 1/5
     *   C: 1/7
     *
     * Overall selectivity = 1/70.
     *
     * Only the order is changed.
     */

    const Condition A = make_condition(2,  {0});
    const Condition B = make_condition(5,  {0});
    const Condition C = make_condition(7,  {0});
    const Condition D = make_condition(11, {0});
    const Condition E = make_condition(13, {0});

    struct TestCase {
        const char* name;
        std::vector<Condition> conditions;
    };

    const std::vector<TestCase> cases{
        {
            "1/2 -> 1/5 -> 1/7 -> 1/11",
            {A, B, C, D, E}
        },
        {
            "1/11 -> 1/7 -> 1/5 -> 1/2",
            {E, D, C, B, A}
        }
    };

    for (const auto& test : cases) {
        benchmark_composite_case(
            test.name,
            min,
            max,
            test.conditions
        );
    }
}

} // namespace

int main()
{
    std::cout
        << "ConditionalRange benchmark\n"
        << "===========================\n"
        << "version = "
        << ConditionalRange::VERSION_STRING
        << "\n";

    benchmark_basic();
    benchmark_density();
    benchmark_selectivity();
    benchmark_composite_selectivity();

    benchmark_same_selectivity_structure();
    benchmark_large_modulus_composite();
    benchmark_condition_ordering();

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