#include <ConditionalRange/ConditionalRange.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>


namespace test2 {

using Condition = cr::Condition;

constexpr int64_t I64_MIN =
    std::numeric_limits<int64_t>::min();

constexpr int64_t I64_MAX =
    std::numeric_limits<int64_t>::max();


// ============================================================
// Utilities
// ============================================================

void require(
    bool condition,
    const char* message
)
{
    if (!condition) {
        std::cerr
            << "TEST FAILURE: "
            << message
            << '\n';

        std::abort();
    }
}


void require_equal(
    int64_t actual,
    int64_t expected,
    const char* message
)
{
    if (actual != expected) {
        std::cerr
            << "TEST FAILURE: "
            << message
            << "\nExpected: "
            << expected
            << "\nActual:   "
            << actual
            << '\n';

        std::abort();
    }
}


void require_sequence(
    const std::vector<int64_t>& actual,
    const std::vector<int64_t>& expected,
    const char* message
)
{
    if (actual == expected) {
        return;
    }

    std::cerr
        << "TEST FAILURE: "
        << message
        << '\n';

    std::cerr
        << "Expected: ";

    for (const auto x : expected) {
        std::cerr << x << ' ';
    }

    std::cerr << "\nActual:   ";

    for (const auto x : actual) {
        std::cerr << x << ' ';
    }

    std::cerr << '\n';

    std::abort();
}


// ============================================================
// 1. INT64_MIN boundary
// ============================================================

void test_int64_min_boundary()
{
    ConditionalRange range(
        I64_MIN,
        I64_MIN + 5,
        {}
    );

    const std::vector<int64_t> expected = {
        I64_MIN,
        I64_MIN + 1,
        I64_MIN + 2,
        I64_MIN + 3,
        I64_MIN + 4,
        I64_MIN + 5
    };

    std::vector<int64_t> actual;

    for (const auto x : range) {
        actual.push_back(x);
    }

    require_sequence(
        actual,
        expected,
        "INT64_MIN boundary"
    );
}


// ============================================================
// 2. INT64_MAX boundary
// ============================================================

void test_int64_max_boundary()
{
    ConditionalRange range(
        I64_MAX - 5,
        I64_MAX,
        {}
    );

    const std::vector<int64_t> expected = {
        I64_MAX - 5,
        I64_MAX - 4,
        I64_MAX - 3,
        I64_MAX - 2,
        I64_MAX - 1,
        I64_MAX
    };

    std::vector<int64_t> actual;

    for (const auto x : range) {
        actual.push_back(x);
    }

    require_sequence(
        actual,
        expected,
        "INT64_MAX boundary"
    );
}


// ============================================================
// 3. Incrementing an iterator at INT64_MAX
// ============================================================

void test_increment_at_int64_max()
{
    ConditionalRange range(
        I64_MAX,
        I64_MAX,
        {}
    );

    auto it = range.begin();
    auto last = range.end();

    require(
        it != last,
        "begin must not equal end at INT64_MAX"
    );

    require_equal(
        *it,
        I64_MAX,
        "iterator value at INT64_MAX"
    );

    ++it;

    require(
        it == last,
        "incrementing INT64_MAX must produce end"
    );
}


// ============================================================
// 4. INT64_MAX with wheel candidate requiring delta
// ============================================================

void test_int64_max_wheel_overflow()
{
    /*
     * Only residue 0 is allowed.
     *
     * INT64_MAX % 2 == 1, so the first candidate would normally
     * be INT64_MAX + 1, which is not representable.
     *
     * The iterator must therefore become end.
     */
    std::vector<bool> allowed = {
        true,
        false
    };

    std::vector<Condition> conditions = {
        {2, allowed}
    };

    ConditionalRange range(
        I64_MAX,
        I64_MAX,
        conditions
    );

    require(
        range.begin() == range.end(),
        "INT64_MAX + required delta must not overflow"
    );
}


// ============================================================
// 5. INT64_MAX where current value itself is valid
// ============================================================

void test_int64_max_valid_wheel_value()
{
    /*
     * Residue 1 is allowed.
     *
     * INT64_MAX % 2 == 1.
     */
    std::vector<bool> allowed = {
        false,
        true
    };

    std::vector<Condition> conditions = {
        {2, allowed}
    };

    ConditionalRange range(
        I64_MAX,
        I64_MAX,
        conditions
    );

    auto it = range.begin();

    require(
        it != range.end(),
        "INT64_MAX should be accepted"
    );

    require_equal(
        *it,
        I64_MAX,
        "INT64_MAX wheel value"
    );

    ++it;

    require(
        it == range.end(),
        "iterator must end after INT64_MAX"
    );
}


// ============================================================
// 6. INT64_MIN with wheel condition
// ============================================================

void test_int64_min_wheel()
{
    /*
     * Test negative modulo at the absolute minimum value.
     */
    std::vector<bool> allowed = {
        true,
        false,
        false
    };

    std::vector<Condition> conditions = {
        {3, allowed}
    };

    ConditionalRange range(
        I64_MIN,
        I64_MIN + 10,
        conditions
    );

    std::vector<int64_t> actual;

    for (const auto x : range) {
        actual.push_back(x);
    }

    /*
     * INT64_MIN % 3 == -2 in C++,
     * mathematical modulo is therefore 1.
     *
     * Accepted values are x % 3 == 0 mathematically.
     */
    const std::vector<int64_t> expected = {
        I64_MIN + 2,
        I64_MIN + 5,
        I64_MIN + 8
    };

    require_sequence(
        actual,
        expected,
        "INT64_MIN negative modulo"
    );
}


// ============================================================
// 7. modulus == 1
// ============================================================

void test_modulus_one()
{
    {
        std::vector<bool> allowed = {
            true
        };

        ConditionalRange range(
            -10,
            10,
            {
                {1, allowed}
            }
        );

        std::vector<int64_t> actual;

        for (const auto x : range) {
            actual.push_back(x);
        }

        require_equal(
            range.wheel_period(),
            1,
            "modulus 1 period"
        );

        require_equal(
            static_cast<int64_t>(actual.size()),
            21,
            "modulus 1 all true"
        );
    }

    {
        std::vector<bool> allowed = {
            false
        };

        ConditionalRange range(
            -10,
            10,
            {
                {1, allowed}
            }
        );

        require(
            range.empty(),
            "modulus 1 all false must be empty"
        );

        require(
            range.begin() == range.end(),
            "modulus 1 all false begin/end"
        );
    }
}


// ============================================================
// 8. Constructor validation
// ============================================================

void test_invalid_modulus()
{
    bool thrown = false;

    try {
        Condition condition(
            0,
            {}
        );

        (void)condition;
    }
    catch (const std::invalid_argument&) {
        thrown = true;
    }

    require(
        thrown,
        "modulus == 0 must throw"
    );


    thrown = false;

    try {
        Condition condition(
            -1,
            {}
        );

        (void)condition;
    }
    catch (const std::invalid_argument&) {
        thrown = true;
    }

    require(
        thrown,
        "negative modulus must throw"
    );
}


void test_invalid_allowed_size()
{
    bool thrown = false;

    try {
        Condition condition(
            3,
            {
                true,
                false
            }
        );

        (void)condition;
    }
    catch (const std::invalid_argument&) {
        thrown = true;
    }

    require(
        thrown,
        "allowed.size() != modulus must throw"
    );
}


// ============================================================
// 9. LCM overflow / remaining_conditions_ path
// ============================================================

void test_remaining_conditions()
{
    // The full LCM exceeds the cap; the deferred subset is plan-dependent.

    const std::vector<int64_t> moduli = {
        2, 3, 5, 7, 11, 13, 17, 19
    };

    std::vector<Condition> conditions;

    for (const int64_t modulus : moduli) {
        std::vector<bool> allowed(
            static_cast<std::size_t>(modulus),
            true
        );
        // 0 のみ禁止し、p-1 個の true にすることで trivial 化を防止
        allowed[0] = false;

        conditions.emplace_back(
            modulus,
            std::move(allowed)
        );
    }

    /*
     * modulus 23 は 余り 7 のみ許可
     */
    std::vector<bool> final_allowed(23, false);
    final_allowed[7] = true;

    conditions.emplace_back(
        23,
        std::move(final_allowed)
    );

    ConditionalRange range(
        0,
        200,
        conditions
    );

    require(
        !range.remaining_conditions().empty(),
        "conditions exceeding the wheel budget must be deferred"
    );

    require(
        range.wheel_period() <= ConditionalRange::DEFAULT_MAX_WHEEL_PERIOD,
        "wheel period must respect the configured cap"
    );

    /*
     * 範囲内 [0, 200] で各条件を満たす値をイテレート検証
     * x % p != 0 (p in {2..19}) 且つ x % 23 == 7
     */
    std::vector<int64_t> actual;
    for (const auto x : range) {
        actual.push_back(x);
    }

    /*
     * ナイーブに全条件を満たす解をフィルタリングして expected を動的に生成する
     */
    std::vector<int64_t> expected;
    for (int64_t x = 0; x <= 200; ++x) {
        bool ok = true;

        // 1. moduli {2, 3, 5, 7, 11, 13, 17, 19} について x % p != 0 を確認
        for (const int64_t modulus : moduli) {
            if (x % modulus == 0) {
                ok = false;
                break;
            }
        }

        // 2. modulus 23 について x % 23 == 7 を確認
        if (ok && (x % 23 != 7)) {
            ok = false;
        }

        if (ok) {
            expected.push_back(x);
        }
    }

    require_sequence(
        actual,
        expected,
        "remaining condition integration test"
    );
}

// オール true (Trivial 条件) がホイールを拡張せずスキップされるかのテスト
void test_all_true_trivial_conditions()
{
    /*
     * すべての要素が true の条件はホイール周期を増加させず
     * 破棄されることを確認する。
     */
    const std::vector<int64_t> moduli = { 2, 3, 5 };
    std::vector<Condition> conditions;

    for (const int64_t modulus : moduli) {
        std::vector<bool> allowed(static_cast<std::size_t>(modulus), true);
        conditions.emplace_back(modulus, std::move(allowed));
    }

    ConditionalRange range(0, 10, conditions);

    // ホイール周期が 1 のままであること
    require_equal(
        range.wheel_period(),
        1,
        "all-true conditions should not expand wheel_period"
    );

    // remaining_conditions も空であること
    require(
        range.remaining_conditions().empty(),
        "all-true conditions should be discarded, not stored in remaining"
    );
}


// ============================================================
// 10. Wheel + remaining condition around INT64_MAX
// ============================================================

void test_remaining_condition_near_int64_max()
{
    /*
     * Wheel condition:
     *
     *   x % 2 == 1
     *
     * Remaining condition:
     *
     *   x % 3 == 0
     *
     * The intersection is x == 3 (mod 6).
     *
     * The range is chosen specifically around INT64_MAX.
     */

    std::vector<bool> odd = {
        false,
        true
    };

    std::vector<bool> divisible_by_3 = {
        true,
        false,
        false
    };

    std::vector<Condition> conditions = {
        {2, odd},
        {3, divisible_by_3}
    };

    ConditionalRange range(
        I64_MAX - 10,
        I64_MAX,
        conditions
    );

    std::vector<int64_t> actual;

    for (const auto x : range) {
        actual.push_back(x);
    }

    /*
     * INT64_MAX == 7 (mod 6).
     *
     * The last value <= INT64_MAX with residue 3 mod 6 is:
     *
     *   INT64_MAX - 4
     */
    const std::vector<int64_t> expected = {
        I64_MAX - 10,
        I64_MAX - 4
    };

    require_sequence(
        actual,
        expected,
        "wheel + remaining condition near INT64_MAX"
    );
}


// ============================================================
// 11. Multiple conditions that leave no result
// ============================================================

void test_nonempty_range_but_no_solution()
{
    std::vector<bool> even = {
        true,
        false
    };

    std::vector<bool> odd = {
        false,
        true
    };

    ConditionalRange range(
        -100,
        100,
        {
            {2, even},
            {2, odd}
        }
    );

    /*
     * The range itself is not empty, but the conditions are
     * mutually exclusive.
     */
    require(
        range.empty(),
        "contradictory conditions must produce empty range"
    );

    require(
        range.begin() == range.end(),
        "contradictory conditions begin/end"
    );
}


// ============================================================
// 12. Prefix/suffix iterator semantics
// ============================================================

void test_iterator_increment_semantics()
{
    std::vector<bool> allowed = {
        true,
        false,
        true
    };

    ConditionalRange range(
        0,
        10,
        {
            {3, allowed}
        }
    );

    auto it = range.begin();

    require_equal(
        *it,
        0,
        "initial iterator value"
    );

    auto old = it++;

    require_equal(
        *old,
        0,
        "postfix ++ old iterator"
    );

    require_equal(
        *it,
        2,
        "postfix ++ current iterator"
    );

    ++it;

    require_equal(
        *it,
        3,
        "prefix ++"
    );
}


// ============================================================
// 13. Default constructed iterator
// ============================================================

void test_default_iterator()
{
    ConditionalRange::Iterator a;
    ConditionalRange::Iterator b;

    require(
        a == b,
        "default constructed iterators must compare equal"
    );

    require(
        a == ConditionalRange::Iterator(),
        "default iterator equality"
    );
}


// ============================================================
// 14. End iterator equality
// ============================================================

void test_end_equality()
{
    ConditionalRange range1(
        0,
        -1,
        {}
    );

    ConditionalRange range2(
        10,
        0,
        {}
    );

    auto end1 = range1.end();
    auto end2 = range2.end();

    require(
        end1 == end2,
        "all end iterators must compare equal"
    );

    require(
        !(end1 != end2),
        "all end iterators must not compare unequal"
    );
}


// ============================================================
// 15. Empty result without empty input range
// ============================================================

void test_empty_result()
{
    std::vector<bool> allowed = {
        false,
        true
    };

    /*
     * Range [0, 0].
     *
     * 0 % 2 == 0, which is forbidden.
     */
    ConditionalRange range(
        0,
        0,
        {
            {2, allowed}
        }
    );

    require(
        !range.empty(),
        "range itself is not empty"
    );

    require(
        range.begin() == range.end(),
        "no valid value must produce begin == end"
    );
}


// ============================================================
// main
// ============================================================

} // namespace test2


int main()
{
    std::cout
        << "ConditionalRange corner-case test suite\n"
        << "=========================================\n";

    test2::test_int64_min_boundary();
    std::cout << "[PASS] INT64_MIN boundary\n";

    test2::test_int64_max_boundary();
    std::cout << "[PASS] INT64_MAX boundary\n";

    test2::test_increment_at_int64_max();
    std::cout << "[PASS] increment at INT64_MAX\n";

    test2::test_int64_max_wheel_overflow();
    std::cout << "[PASS] INT64_MAX wheel overflow\n";

    test2::test_int64_max_valid_wheel_value();
    std::cout << "[PASS] INT64_MAX valid wheel value\n";

    test2::test_int64_min_wheel();
    std::cout << "[PASS] INT64_MIN wheel\n";

    test2::test_modulus_one();
    std::cout << "[PASS] modulus == 1\n";

    test2::test_invalid_modulus();
    std::cout << "[PASS] invalid modulus\n";

    test2::test_invalid_allowed_size();
    std::cout << "[PASS] invalid allowed size\n";

    test2::test_remaining_conditions();
    test2::test_all_true_trivial_conditions();
    std::cout << "[PASS] remaining conditions\n";

    test2::test_remaining_condition_near_int64_max();
    std::cout << "[PASS] remaining condition near INT64_MAX\n";

    test2::test_nonempty_range_but_no_solution();
    std::cout << "[PASS] contradictory conditions\n";

    test2::test_iterator_increment_semantics();
    std::cout << "[PASS] iterator increment semantics\n";

    test2::test_default_iterator();
    std::cout << "[PASS] default iterator\n";

    test2::test_end_equality();
    std::cout << "[PASS] end iterator equality\n";

    test2::test_empty_result();
    std::cout << "[PASS] empty result\n";

    std::cout
        << "\nAll corner-case tests passed.\n";

    return 0;
}
