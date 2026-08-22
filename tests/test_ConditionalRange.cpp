#include <ConditionalRange/ConditionalRange.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>


// ============================================================
// Test utilities
// ============================================================

namespace test {

// ------------------------------------------------------------
// Condition
// ------------------------------------------------------------

using Condition = ConditionalRange::Condition;


// ------------------------------------------------------------
// Positive modulo
//
// C++'s % may return a negative value for negative x.
// The actual condition uses the mathematical modulo [0, m).
// ------------------------------------------------------------

int64_t positive_mod(int64_t x, int64_t m)
{
    const int64_t r = x % m;
    return r < 0 ? r + m : r;
}


// ------------------------------------------------------------
// Naive condition check
//
// This is intentionally simple.
// Do NOT optimize this function.
// It is the oracle against which ConditionalRange is tested.
// ------------------------------------------------------------

bool satisfies(
    int64_t x,
    const Condition& condition
)
{
    const int64_t r = positive_mod(x, condition.modulus);

    return condition.allowed[
        static_cast<std::size_t>(r)
    ];
}


// ------------------------------------------------------------
// Naive implementation
//
// Enumerate every integer in [min, max] and check every
// condition directly.
//
// This is our ground truth.
// ------------------------------------------------------------

std::vector<int64_t> naive(
    int64_t min,
    int64_t max,
    const std::vector<Condition>& conditions
)
{
    std::vector<int64_t> result;

    if (min > max) {
        return result;
    }

    for (int64_t x = min;; ++x) {
        bool ok = true;

        for (const auto& condition : conditions) {
            if (!satisfies(x, condition)) {
                ok = false;
                break;
            }
        }

        if (ok) {
            result.push_back(x);
        }

        if (x == max) {
            break;
        }
    }

    return result;
}


// ------------------------------------------------------------
// Generate random B_p
//
// true_probability is in [0, 1].
// ------------------------------------------------------------

std::vector<bool> random_bits(
    int64_t modulus,
    double true_probability,
    std::mt19937_64& rng
)
{
    std::vector<bool> bits(
        static_cast<std::size_t>(modulus)
    );

    std::bernoulli_distribution distribution(
        true_probability
    );

    for (std::size_t i = 0; i < bits.size(); ++i) {
        bits[i] = distribution(rng);
    }

    return bits;
}


// ------------------------------------------------------------
// Random condition
// ------------------------------------------------------------

Condition random_condition(
    int64_t modulus,
    std::mt19937_64& rng
)
{
    /*
     * Generate a broad range of densities.
     *
     * Using a random probability rather than always 50%
     * is important because it tests:
     *
     *   - very sparse B
     *   - very dense B
     *   - roughly random B
     */
    std::uniform_real_distribution<double> probability(
        0.0,
        1.0
    );

    const double p = probability(rng);

    return Condition(
        modulus,
        random_bits(modulus, p, rng)
    );
}


// ------------------------------------------------------------
// Compare results
// ------------------------------------------------------------

void assert_equal(
    const std::vector<int64_t>& expected,
    const std::vector<int64_t>& actual,
    const std::string& context
)
{
    if (expected == actual) {
        return;
    }

    std::cerr << "\n";
    std::cerr << "TEST FAILURE\n";
    std::cerr << "Context: " << context << "\n";
    std::cerr << "Expected size: "
              << expected.size() << "\n";
    std::cerr << "Actual size:   "
              << actual.size() << "\n";

    const std::size_t common =
        std::min(expected.size(), actual.size());

    for (std::size_t i = 0; i < common; ++i) {
        if (expected[i] != actual[i]) {
            std::cerr
                << "First difference at index "
                << i << "\n"
                << "Expected: "
                << expected[i] << "\n"
                << "Actual:   "
                << actual[i] << "\n";

            break;
        }
    }

    if (expected.size() != actual.size()) {
        if (expected.size() > common) {
            std::cerr
                << "Expected has additional values starting at "
                << expected[common] << "\n";
        }

        if (actual.size() > common) {
            std::cerr
                << "Actual has additional values starting at "
                << actual[common] << "\n";
        }
    }

    std::abort();
}


// ------------------------------------------------------------
// Run one comparison
// ------------------------------------------------------------

void check_case(
    int64_t min,
    int64_t max,
    const std::vector<Condition>& conditions,
    const std::string& context
)
{
    const auto expected =
        naive(min, max, conditions);

    ConditionalRange range(
        min,
        max,
        conditions
    );

    std::vector<int64_t> actual;

    for (const int64_t x : range) {
        actual.push_back(x);
    }

    assert_equal(
        expected,
        actual,
        context
    );
}


// ============================================================
// Deterministic tests
// ============================================================

// ------------------------------------------------------------
// Empty range
// ------------------------------------------------------------

void test_empty_range()
{
    std::vector<Condition> conditions;

    check_case(
        10,
        0,
        conditions,
        "empty range: min > max"
    );
}


// ------------------------------------------------------------
// No conditions
//
// Every integer should be returned.
// ------------------------------------------------------------

void test_no_conditions()
{
    check_case(
        -100,
        100,
        {},
        "no conditions"
    );
}


// ------------------------------------------------------------
// One modulus
// ------------------------------------------------------------

void test_single_condition()
{
    std::vector<bool> b = {
        true,
        false,
        true
    };

    std::vector<Condition> conditions = {
        {3, b}
    };

    check_case(
        -100,
        100,
        conditions,
        "single modulus"
    );
}


// ------------------------------------------------------------
// All true
// ------------------------------------------------------------

void test_all_true()
{
    std::vector<bool> b(
        17,
        true
    );

    std::vector<Condition> conditions = {
        {17, b}
    };

    check_case(
        -100,
        100,
        conditions,
        "all true"
    );
}


// ------------------------------------------------------------
// All false
// ------------------------------------------------------------

void test_all_false()
{
    std::vector<bool> b(
        17,
        false
    );

    std::vector<Condition> conditions = {
        {17, b}
    };

    check_case(
        -100,
        100,
        conditions,
        "all false"
    );
}


// ------------------------------------------------------------
// Negative range
// ------------------------------------------------------------

void test_negative_range()
{
    std::vector<bool> b3 = {
        true,
        false,
        true
    };

    std::vector<bool> b5 = {
        false,
        true,
        true,
        false,
        true
    };

    std::vector<Condition> conditions = {
        {3, b3},
        {5, b5}
    };

    check_case(
        -1000,
        -1,
        conditions,
        "negative range"
    );
}


// ------------------------------------------------------------
// Range crossing zero
// ------------------------------------------------------------

void test_crossing_zero()
{
    std::vector<bool> b4 = {
        true,
        false,
        true,
        false
    };

    std::vector<Condition> conditions = {
        {4, b4}
    };

    check_case(
        -100,
        100,
        conditions,
        "range crossing zero"
    );
}


// ------------------------------------------------------------
// Power-of-prime / composite moduli
// ------------------------------------------------------------

void test_prime_powers()
{
    std::mt19937_64 rng(0x123456789ULL);

    const std::vector<int64_t> moduli = {
        4,
        8,
        9,
        16,
        25,
        27,
        32,
        49,
        64,
        81
    };

    std::vector<Condition> conditions;

    for (const int64_t modulus : moduli) {
        conditions.push_back(
            random_condition(
                modulus,
                rng
            )
        );
    }

    check_case(
        -5000,
        5000,
        conditions,
        "prime powers"
    );
}


// ------------------------------------------------------------
// Duplicate modulus
// ------------------------------------------------------------

void test_duplicate_modulus()
{
    std::vector<bool> b1 = {
        true,
        false,
        true,
        true,
        false
    };

    std::vector<bool> b2 = {
        false,
        true,
        true,
        false,
        true
    };

    std::vector<Condition> conditions = {
        {5, b1},
        {5, b2}
    };

    check_case(
        -1000,
        1000,
        conditions,
        "duplicate modulus"
    );
}


// ------------------------------------------------------------
// Boundary values
// ------------------------------------------------------------

void test_boundaries()
{
    std::vector<bool> b2 = {
        true,
        false
    };

    std::vector<bool> b3 = {
        false,
        true,
        true
    };

    std::vector<Condition> conditions = {
        {2, b2},
        {3, b3}
    };

    check_case(
        -10,
        10,
        conditions,
        "small boundary range"
    );

    check_case(
        -1,
        1,
        conditions,
        "range [-1, 1]"
    );

    check_case(
        0,
        0,
        conditions,
        "single value range"
    );

    check_case(
        1,
        1,
        conditions,
        "single positive value"
    );

    check_case(
        -1,
        -1,
        conditions,
        "single negative value"
    );
}


// ============================================================
// Random tests
// ============================================================

void random_test(
    std::uint64_t seed,
    int iteration
)
{
    std::mt19937_64 rng(seed);

    /*
     * Keep random ranges reasonably small because the naive
     * implementation intentionally examines every integer.
     */
    std::uniform_int_distribution<int64_t> center_dist(
        -10000,
        10000
    );

    std::uniform_int_distribution<int64_t> length_dist(
        0,
        20000
    );

    const int64_t min =
        center_dist(rng);

    const int64_t length =
        length_dist(rng);

    const int64_t max =
        min + length;

    /*
     * Include both prime moduli and prime powers / composites.
     */
    const std::vector<int64_t> possible_moduli = {
        2,
        3,
        4,
        5,
        7,
        8,
        9,
        11,
        13,
        16,
        17,
        19,
        23,
        25,
        27,
        29,
        31,
        32,
        37,
        41,
        43,
        47,
        49,
        64,
        81
    };

    /*
     * Sometimes use no conditions.
     * Sometimes use many.
     */
    std::uniform_int_distribution<std::size_t> count_dist(
        0,
        15
    );

    const std::size_t condition_count =
        count_dist(rng);

    std::uniform_int_distribution<std::size_t> modulus_dist(
        0,
        possible_moduli.size() - 1
    );

    std::vector<Condition> conditions;

    conditions.reserve(condition_count);

    for (std::size_t i = 0;
         i < condition_count;
         ++i)
    {
        const int64_t modulus =
            possible_moduli[
                modulus_dist(rng)
            ];

        conditions.push_back(
            random_condition(
                modulus,
                rng
            )
        );
    }

    const std::string context =
        "random test #" +
        std::to_string(iteration) +
        ", seed=" +
        std::to_string(seed) +
        ", range=[" +
        std::to_string(min) +
        ", " +
        std::to_string(max) +
        "]";

    check_case(
        min,
        max,
        conditions,
        context
    );
}


// ------------------------------------------------------------
// Run many random tests
// ------------------------------------------------------------

void test_random()
{
    /*
     * Fixed master seed:
     *
     * If a bug is found, the failure is reproducible.
     */
    constexpr std::uint64_t MASTER_SEED =
        0xC0FFEE123456789ULL;

    std::mt19937_64 master_rng(
        MASTER_SEED
    );

    constexpr int TEST_COUNT = 100;

    for (int i = 0; i < TEST_COUNT; ++i) {
        /*
         * Give every test its own seed.
         * This makes it easy to reproduce a specific failure.
         */
        const std::uint64_t seed =
            master_rng();

        random_test(
            seed,
            i
        );

        if ((i + 1) % 100 == 0) {
            std::cout
                << "Random tests: "
                << (i + 1)
                << " / "
                << TEST_COUNT
                << '\n';
        }
    }
}


// ============================================================
// main
// ============================================================

} // namespace test


int main()
{
    std::cout
        << "ConditionalRange test suite\n"
        << "============================\n";

    test::test_empty_range();
    std::cout << "[PASS] empty range\n";

    test::test_no_conditions();
    std::cout << "[PASS] no conditions\n";

    test::test_single_condition();
    std::cout << "[PASS] single condition\n";

    test::test_all_true();
    std::cout << "[PASS] all true\n";

    test::test_all_false();
    std::cout << "[PASS] all false\n";

    test::test_negative_range();
    std::cout << "[PASS] negative range\n";

    test::test_crossing_zero();
    std::cout << "[PASS] crossing zero\n";

    test::test_prime_powers();
    std::cout << "[PASS] prime powers\n";

    test::test_duplicate_modulus();
    std::cout << "[PASS] duplicate modulus\n";

    test::test_boundaries();
    std::cout << "[PASS] boundaries\n";

    test::test_random();
    std::cout << "[PASS] 5000 random tests\n";

    std::cout
        << "\nALL TESTS PASSED\n";

    return 0;
}