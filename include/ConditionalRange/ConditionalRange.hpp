
#pragma once
#pragma message("===== ConditionalRange.hpp VERSION 2026-08-23-FINAL =====")

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#include <iostream>

class ConditionalRange {
public:
    struct Condition {
        int64_t modulus;
        std::vector<bool> allowed;

        Condition(int64_t m, std::vector<bool> bits)
            : modulus(m),
              allowed(std::move(bits))
        {
            if (modulus <= 0) {
                throw std::invalid_argument(
                    "ConditionalRange: modulus must be positive"
                );
            }

            if (allowed.size() != static_cast<std::size_t>(modulus)) {
                throw std::invalid_argument(
                    "ConditionalRange: allowed.size() must equal modulus"
                );
            }
        }
    };

    static constexpr const char* VERSION = "2026-08-23-FINAL";

private:

    /*
     * The wheel is deliberately bounded.
     *
     * Conditions whose combined period would exceed this value are
     * retained in remaining_conditions_ and checked only for wheel
     * candidates.
     */
    static constexpr int64_t MAX_WHEEL_PERIOD = 10'000'000;

    int64_t min_;
    int64_t max_;

    /*
     * The wheel represents the set
     *
     *   { x | x mod wheel_period_ is in wheel_residues_ }.
     *
     * Residues are always normalized to [0, wheel_period_).
     */
    int64_t wheel_period_ = 1;
    std::vector<int64_t> wheel_residues_{0};

    /*
     * Conditions which were not incorporated into the wheel.
     *
     * Every value produced by the wheel is checked against these
     * conditions before being exposed by the iterator.
     */
    std::vector<Condition> remaining_conditions_;

    bool empty_ = false;

    // ============================================================
    // Mathematical utilities
    // ============================================================

    static int64_t gcd_i64(int64_t a, int64_t b)
    {
        while (b != 0) {
            const int64_t r = a % b;
            a = b;
            b = r;
        }

        return a;
    }

    /*
     * Computes lcm(a, b) without overflowing int64_t.
     *
     * a and b are positive.
     */
    static bool try_lcm(
        int64_t a,
        int64_t b,
        int64_t& result
    )
    {
        const int64_t g = gcd_i64(a, b);
        const int64_t a_div_g = a / g;

        if (a_div_g > std::numeric_limits<int64_t>::max() / b) {
            return false;
        }

        result = a_div_g * b;
        return true;
    }

    /*
     * Mathematical modulo:
     *
     *   0 <= result < m
     *
     * with m > 0.
     */
    static int64_t positive_mod(int64_t x, int64_t m)
    {
        const int64_t r = x % m;
        return r < 0 ? r + m : r;
    }

    // ============================================================
    // Condition utilities
    // ============================================================

    static bool all_true(const std::vector<bool>& bits)
    {
        for (bool bit : bits) {
            if (!bit) {
                return false;
            }
        }

        return true;
    }

    static bool all_false(const std::vector<bool>& bits)
    {
        for (bool bit : bits) {
            if (bit) {
                return false;
            }
        }

        return true;
    }

    static bool satisfies(
        int64_t x,
        const Condition& condition
    )
    {
        const int64_t residue =
            positive_mod(x, condition.modulus);

        return condition.allowed[
            static_cast<std::size_t>(residue)
        ];
    }

    bool satisfies_remaining(int64_t x) const
    {
        for (const auto& condition : remaining_conditions_) {
            if (!satisfies(x, condition)) {
                return false;
            }
        }

        return true;
    }

    // ============================================================
    // Wheel construction
    // ============================================================

    void build(std::vector<Condition> conditions)
    {
        if (min_ > max_) {
            empty_ = true;
            return;
        }

        /*
         * Remove conditions which are either redundant or make the
         * entire range impossible.
         */
        std::vector<Condition> useful;
        useful.reserve(conditions.size());

        for (auto& condition : conditions) {
            if (all_false(condition.allowed)) {
                empty_ = true;
                return;
            }

            if (all_true(condition.allowed)) {
                continue;
            }

            useful.push_back(std::move(condition));
        }

        /*
         * Building the wheel from small moduli first tends to keep
         * intermediate residue sets small.
         *
         * The order does not affect the mathematical result.
         */
        std::sort(
            useful.begin(),
            useful.end(),
            [](const Condition& lhs, const Condition& rhs) {
                return lhs.modulus < rhs.modulus;
            }
        );

        for (auto& condition : useful) {
            int64_t new_period = 0;

            if (!try_lcm(
                    wheel_period_,
                    condition.modulus,
                    new_period
                ) ||
                new_period > MAX_WHEEL_PERIOD)
            {
                /*
                 * Since wheel_period_ never decreases, a condition
                 * which cannot be incorporated now cannot become
                 * incorporable later either.
                 *
                 * All such conditions are therefore checked later
                 * against wheel candidates.
                 */
                remaining_conditions_.push_back(
                    std::move(condition)
                );
                continue;
            }

            const int64_t gcd =
                gcd_i64(
                    wheel_period_,
                    condition.modulus
                );

            const int64_t copies =
                condition.modulus / gcd;

            std::vector<int64_t> next_residues;

            /*
             * Each old residue r expands to
             *
             *   r + k * old_period
             *
             * for k = 0 .. copies-1.
             *
             * These are exactly the residues modulo new_period
             * belonging to the old residue class.
             */
            const std::size_t old_size =
                wheel_residues_.size();

            const std::size_t copies_size =
                static_cast<std::size_t>(copies);

            if (copies_size != 0 &&
                old_size <=
                    std::numeric_limits<std::size_t>::max()
                    / copies_size)
            {
                next_residues.reserve(
                    old_size * copies_size
                );
            }

            for (const int64_t residue : wheel_residues_) {
                for (int64_t k = 0; k < copies; ++k) {
                    const int64_t candidate =
                        residue + k * wheel_period_;

                    /*
                     * candidate is in [0, new_period), so this
                     * modulo is already non-negative.
                     */
                    const int64_t condition_residue =
                        candidate % condition.modulus;

                    if (condition.allowed[
                        static_cast<std::size_t>(
                            condition_residue
                        )
                    ]) {
                        next_residues.push_back(candidate);
                    }
                }
            }

            wheel_period_ = new_period;
            wheel_residues_ = std::move(next_residues);

            if (wheel_residues_.empty()) {
                empty_ = true;
                return;
            }
        }

        /*
         * The initial wheel {0} / period 1 already represents all
         * integers. This also covers the no-condition case.
         */
        std::sort(
            wheel_residues_.begin(),
            wheel_residues_.end()
        );
    }

public:
    // ============================================================
    // Iterator
    // ============================================================

    class iterator {
        const ConditionalRange* range_ = nullptr;

        /*
         * current_ is a LOWER BOUND, not necessarily a value which
         * satisfies the range.
         *
         * seek_next_valid() finds the smallest valid value >=
         * current_.
         *
         * This makes the iterator state substantially simpler than
         * maintaining a separate "current residue series".
         */
        int64_t current_ = 0;

        bool end_ = true;

        void seek_next_valid()
        {
            if (range_ == nullptr ||
                range_->empty_)
            {
                end_ = true;
                return;
            }

            /*
            * Find the smallest wheel candidate >= current_.
            *
            * current_ is a lower bound, not necessarily a valid value.
            */
            bool found = false;
            int64_t best = 0;

            const int64_t period =
                range_->wheel_period_;

            const int64_t current_residue =
                positive_mod(current_, period);

            for (const int64_t residue :
                range_->wheel_residues_)
            {
                /*
                * delta is in [0, period).
                *
                * residue and current_residue are both in
                * [0, period), so their subtraction cannot overflow.
                */
                const int64_t delta =
                    positive_mod(
                        residue - current_residue,
                        period
                    );

                /*
                * We need:
                *
                *     candidate = current_ + delta
                *
                * to remain representable as int64_t.
                *
                * When current_ is negative, adding a non-negative delta
                * cannot overflow upward beyond INT64_MAX.
                *
                * Therefore the overflow check is only necessary when
                * current_ is non-negative.
                */
                if (current_ >= 0 &&
                    delta >
                        std::numeric_limits<int64_t>::max() - current_)
                {
                    continue;
                }

                const int64_t candidate =
                    current_ + delta;

                /*
                * The candidate must lie inside the requested range.
                */
                if (candidate > range_->max_) {
                    continue;
                }

                /*
                * The wheel only represents the conditions incorporated
                * into the wheel. Check every remaining condition here.
                */
                if (!range_->satisfies_remaining(candidate)) {
                    continue;
                }

                /*
                * Several wheel residues may produce candidates.
                * Choose the smallest one so that the iterator remains
                * monotonically increasing.
                */
                if (!found || candidate < best) {
                    best = candidate;
                    found = true;
                }
            }

            if (!found) {
                end_ = true;
                return;
            }

            current_ = best;
            end_ = false;
        }

    public:
        using value_type = int64_t;
        using difference_type = std::ptrdiff_t;
        using iterator_category =
            std::forward_iterator_tag;
        using iterator_concept =
            std::forward_iterator_tag;
        using reference = int64_t;
        using pointer = void;

        iterator() = default;

        explicit iterator(
            const ConditionalRange* range,
            bool is_end
        )
            : range_(range),
            current_(0),
            end_(is_end)
        {
            std::cout
                << "[iterator ctor]"
                << " range=" << range_
                << " is_end=" << is_end
                << " end_=" << end_
                << '\n';

            if (!is_end && range_ != nullptr) {
                current_ = range_->min_;

                std::cout
                    << "[iterator ctor before seek]"
                    << " current=" << current_
                    << " end_=" << end_
                    << '\n';

                seek_next_valid();

                std::cout
                    << "[iterator ctor after seek]"
                    << " current=" << current_
                    << " end_=" << end_
                    << '\n';
            }
        }

        value_type operator*() const
        {
            return current_;
        }

        iterator& operator++()
        {
            if (end_) {
                return *this;
            }

            /*
             * The current value has already been returned.
             *
             * We need the next integer strictly greater than it.
             *
             * Avoid current_ + 1 when current_ == INT64_MAX.
             */
            if (current_ ==
                std::numeric_limits<int64_t>::max())
            {
                end_ = true;
                return *this;
            }

            ++current_;

            /*
             * seek_next_valid() now interprets current_ as a lower
             * bound, so no residue-index bookkeeping is necessary.
             */
            seek_next_valid();

            return *this;
        }

        iterator operator++(int)
        {
            iterator old = *this;
            ++(*this);
            return old;
        }

        friend bool operator==(
            const iterator& lhs,
            const iterator& rhs
        )
        {
            /*
             * All end iterators compare equal, as required for the
             * usual range-for / forward-iterator use.
             */
            if (lhs.end_ && rhs.end_) {
                return true;
            }

            return lhs.range_ == rhs.range_
                && lhs.end_ == rhs.end_
                && lhs.current_ == rhs.current_;
        }

        friend bool operator!=(
            const iterator& lhs,
            const iterator& rhs
        )
        {
            return !(lhs == rhs);
        }
    };

    // ============================================================
    // Construction
    // ============================================================

    ConditionalRange(
        int64_t min,
        int64_t max,
        std::vector<Condition> conditions
    )
        : min_(min),
          max_(max)
    {
        build(std::move(conditions));
    }

    // ============================================================
    // Iteration
    // ============================================================

    iterator begin() const
    {
        if (empty_) {
            return end();
        }

        return iterator(this, false);
    }

    iterator end() const
    {
        return iterator(this, true);
    }

    // ============================================================
    // Information
    // ============================================================

    int64_t min() const
    {
        return min_;
    }

    int64_t max() const
    {
        return max_;
    }

    int64_t wheel_period() const
    {
        return wheel_period_;
    }

    std::size_t wheel_size() const
    {
        return wheel_residues_.size();
    }

    bool empty() const
    {
        return empty_;
    }
};
