#ifndef CONDITIONAL_RANGE_HPP
#define CONDITIONAL_RANGE_HPP

#include <cstdint>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <limits>
#include <iterator>
#include <iostream>
#include <string_view>
#include <utility>

namespace cr {

struct Condition {
    std::int64_t modulus;
    std::vector<bool> allowed;

    Condition(std::int64_t m, std::vector<bool> a)
        : modulus(m), allowed(std::move(a)) {
        if (modulus <= 0) {
            throw std::invalid_argument("Modulus must be positive.");
        }
        if (static_cast<std::size_t>(modulus) != allowed.size()) {
            throw std::invalid_argument("Allowed array size must equal modulus.");
        }
    }
};

// Safe mathematical positive modulo: returns r in [0, m)
inline constexpr std::int64_t positive_mod(std::int64_t x, std::int64_t m) noexcept {
    std::int64_t r = x % m;
    if (r < 0) {
        r += m;
    }
    return r;
}

// Floor division: floor(a / b) for b > 0
inline constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t q = a / b;
    std::int64_t r = a % b;
    if (r != 0 && ((a < 0) ^ (b < 0))) {
        q -= 1;
    }
    return q;
}

// Safe addition checking for int64_t overflow
inline bool safe_add(std::int64_t a, std::int64_t b, std::int64_t& result) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(a, b, &result);
#else
    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
        (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b)) {
        return false;
    }
    result = a + b;
    return true;
#endif
}

// Safe multiplication checking for int64_t overflow
inline bool safe_mul(std::int64_t a, std::int64_t b, std::int64_t& result) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(a, b, &result);
#else
    if (a == 0 || b == 0) {
        result = 0;
        return true;
    }
    if (a > 0) {
        if (b > 0 && a > std::numeric_limits<std::int64_t>::max() / b) return false;
        if (b < 0 && b < std::numeric_limits<std::int64_t>::min() / a) return false;
    } else {
        if (b > 0 && a < std::numeric_limits<std::int64_t>::min() / b) return false;
        if (b < 0 && a < std::numeric_limits<std::int64_t>::max() / b) return false;
    }
    result = a * b;
    return true;
#endif
}

static std::int64_t inverse_mod(std::int64_t a, std::int64_t m) noexcept {
    // Requires coprime positive a,m.
    std::int64_t r = m, next_r = a, t = 0, next_t = 1;
    while (next_r != 0) {
        const auto q = r / next_r;
        const auto remainder = r - q * next_r;
        const auto coefficient = t - q * next_t;
        r = next_r; next_r = remainder;
        t = next_t; next_t = coefficient;
    }
    return positive_mod(t, m);
}

static std::uint64_t multiply_mod(std::uint64_t a, std::uint64_t b,
                                    std::uint64_t m) noexcept {
#if defined(__SIZEOF_INT128__)
    return static_cast<std::uint64_t>(__extension__ static_cast<unsigned __int128>(a) * b % m);
#else
    // a,b < m <= INT64_MAX keeps unsigned sums representable.
    std::uint64_t result = 0;
    while (b != 0) {
        if (b & 1) {
            result += a;
            if (result >= m) result -= m;
        }
        b >>= 1;
        a += a;
        if (a >= m) a -= m;
    }
    return result;
#endif
}

class ConditionalRange {
public:
    static constexpr std::int64_t DEFAULT_MAX_WHEEL_PERIOD = 10'000'000;
    static constexpr std::string_view VERSION_STRING = "1.0.0";

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::int64_t;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const std::int64_t*;
        using reference         = std::int64_t;

        Iterator() : parent_(nullptr), is_end_(true) {}

        Iterator(const ConditionalRange* parent, bool is_end)
            : parent_(parent)
            , is_end_(is_end) {
            if (!is_end_) {
                if (parent_->empty_) {
                    is_end_ = true;
                } else {
                    init_position();
                }
            }
        }

        reference operator*() const {
            return current_value_;
        }

#if defined(_MSC_VER)
        __forceinline
#elif defined(__GNUC__) || defined(__clang__)
        __attribute__((always_inline))
#endif
        Iterator& operator++() {
            if (!is_end_) {
                advance_indices();
                find_next_valid();
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            if (is_end_ && other.is_end_) return true;
            if (is_end_ != other.is_end_) return false;

            return parent_ == other.parent_ &&
                   current_k_ == other.current_k_ &&
                   residue_idx_ == other.residue_idx_;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        const ConditionalRange* parent_ = nullptr;
        std::int64_t current_k_ = 0;
        std::size_t residue_idx_ = 0;
        std::int64_t current_value_ = 0;
        bool is_end_ = true;

        void init_position() {
            std::int64_t P = parent_->wheel_period_;
            const auto& residues = parent_->wheel_residues_;

            std::int64_t k_start = floor_div(parent_->min_, P);
            std::int64_t target_r = positive_mod(parent_->min_, P);

            // std::cerr << "[DEBUG] init_position: min_=" << parent_->min_ 
            //           << ", P=" << P 
            //           << ", k_start=" << k_start 
            //           << ", target_r=" << target_r << "\n";

            auto it = std::ranges::lower_bound(residues, target_r);
            if (it != residues.end()) {
                current_k_ = k_start;
                residue_idx_ = static_cast<std::size_t>(it - residues.begin());
            } else {
                current_k_ = k_start + 1;
                residue_idx_ = 0;
            }

            // std::cerr << "[DEBUG] initial iterator state: current_k_=" << current_k_
            //           << ", residue_idx_=" << residue_idx_ << "\n";

            find_next_valid();
        }

        // 内部状態を1歩進める基本処理
        void advance_indices() {
            const std::size_t num_residues = parent_->wheel_residues_.size();
            residue_idx_++;
            if (residue_idx_ >= num_residues) {
                residue_idx_ = 0;
                if (current_k_ == std::numeric_limits<std::int64_t>::max()) {
                    is_end_ = true;
                    return;
                }
                current_k_++;
            }
        }

        void advance_indices(std::size_t num_residues) {
            residue_idx_++;
            if (residue_idx_ >= num_residues) {
                residue_idx_ = 0;
                // current_k_ が INT64_MAX に達している場合はオーバーフローするので繰り上げ不可
                if (current_k_ == std::numeric_limits<std::int64_t>::max()) {
                    is_end_ = true;
                    return;
                }
                current_k_++;
            }
        }

        // Avoid an out-of-line call per increment.
#if defined(_MSC_VER)
        __forceinline
#elif defined(__GNUC__) || defined(__clang__)
        __attribute__((always_inline))
#endif
        void find_next_valid() {
            std::int64_t P = parent_->wheel_period_;
            const auto& residues = parent_->wheel_residues_;
            std::size_t num_residues = residues.size();

            if (!is_end_ && P == 1 && parent_->remaining_conditions_.empty()) {
                current_value_ = current_k_;
                is_end_ = current_value_ > parent_->max_;
                return;
            }

            while (!is_end_) {
                std::int64_t k_P, candidate;
                if (!safe_mul(current_k_, P, k_P)) [[unlikely]] {
                    if (current_k_ > 0) {
                        is_end_ = true;
                        return;
                    }
                    // Here -2^64 < k*P+r < 0. C++20 modulo conversion recovers
                    // valid negative values; nonnegative results are below INT64_MIN.
                    candidate = static_cast<std::int64_t>(
                        static_cast<std::uint64_t>(current_k_) * static_cast<std::uint64_t>(P) +
                        static_cast<std::uint64_t>(residues[residue_idx_]));
                    if (candidate >= 0) {
                        advance_indices(num_residues);
                        continue;
                    }
                } else if (!safe_add(k_P, residues[residue_idx_], candidate)) [[unlikely]] {
                    if (k_P >= 0) {
                        // 正方向の溢れは INT64_MAX 超えを意味するので終端
                        is_end_ = true;
                        return;
                    } else {
                        // ありえない
                        std::cerr << "FATAL: [process] value is invalid (" << k_P << "," << residues[residue_idx_] << ")" << std::endl;
                        std::terminate(); 
                    }
                }

                // k の符号に関わらず、candidate が max_ を超えたら
                // これ以上 residue や k を増やしても candidate が小さくなることはないため即時終了
                if (candidate > parent_->max_) {
                    is_end_ = true;
                    return;
                }

                if (candidate >= parent_->min_) {
                    if (parent_->satisfies_remaining(candidate)) {
                        current_value_ = candidate;
                        return;
                    }
                }

                advance_indices(num_residues);
            }
        }

    }; // Iterator

    ConditionalRange(
        std::int64_t min_val,
        std::int64_t max_val,
        std::vector<Condition> conditions,
        std::int64_t max_wheel_period = DEFAULT_MAX_WHEEL_PERIOD
    ) : min_(min_val), max_(max_val), max_wheel_period_(max_wheel_period) {

        if (min_ > max_) {
            empty_ = true;
            return;
        }

        // Trivial conditions check & Filtering
        std::vector<PlanningCondition> effective_conditions;
        for (auto& cond : conditions) {
            const auto allowed_count = static_cast<std::size_t>(
                std::count(cond.allowed.begin(), cond.allowed.end(), true));

            // 全て false ならその時点で全域で解なし (空集合)
            if (allowed_count == 0) {
                empty_ = true;
                return;
            }

            // 全て true (Trivial) でない、有効な条件のみを抽出
            // (Trivial な条件は LCM に寄与せず、無視して次へ進む)
            if (allowed_count != cond.allowed.size()) {
                effective_conditions.push_back({std::move(cond), allowed_count});
            }
        }

        const auto order = choose_order(effective_conditions);

        // Initialize Wheel: period = 1, residues = {0}
        wheel_period_ = 1;
        wheel_residues_.assign(1, 0); // または clear() して push_back(0) / assign

        for (auto index : order) {
            auto& item = effective_conditions[index];
            auto& cond = item.condition;
            std::int64_t m = cond.modulus;
            std::int64_t g = std::gcd(wheel_period_, m);
            std::int64_t q = m / g;

            std::int64_t next_period;
            bool overflow = !safe_mul(wheel_period_, q, next_period);

            // overflow または上限超えの場合のみ remaining に退避
            if (overflow || next_period > max_wheel_period_) {
                remaining_conditions_.push_back(std::move(cond));
                continue;
            }

            std::vector<std::int64_t> new_residues;
            if (wheel_period_ == 1) {
                new_residues.reserve(item.allowed_count);
                for (std::int64_t r = 0; r < m; ++r) {
                    if (cond.allowed[static_cast<std::size_t>(r)]) {
                        new_residues.push_back(r);
                    }
                }
            } else if (q > 1 && item.allowed_count < static_cast<std::size_t>(q) / 4) {
                new_residues = combine_sparse(cond, g, q, item.allowed_count);
            } else {
                new_residues.reserve(wheel_residues_.size() * static_cast<std::size_t>(q));
                for (std::int64_t k = 0; k < q; ++k) {
                    const std::int64_t shift = k * wheel_period_;
                    for (std::int64_t r : wheel_residues_) {
                        const std::int64_t new_r = r + shift;
                        if (cond.allowed[static_cast<std::size_t>(positive_mod(new_r, m))]) {
                            new_residues.push_back(new_r);
                        }
                    }
                }
            }

            if (new_residues.empty()) {
                empty_ = true;
                return;
            }

            wheel_period_ = next_period;
            wheel_residues_ = std::move(new_residues);
        }
    }

    Iterator begin() const {
        return Iterator(this, empty_);
    }

    Iterator end() const {
        return Iterator(this, true);
    }

    bool empty() const noexcept {
        return empty_;
    }

    std::int64_t wheel_period() const noexcept {
        return wheel_period_;
    }

    const std::vector<std::int64_t>& wheel_residues() const noexcept {
        return wheel_residues_;
    }

    const std::vector<Condition>& remaining_conditions() const noexcept {
        return remaining_conditions_;
    }

private:
    struct PlanningCondition {
        Condition condition;
        std::size_t allowed_count;
    };

    std::vector<std::size_t> choose_order(const std::vector<PlanningCondition>& items) const {
        std::vector<std::size_t> order(items.size());
        std::iota(order.begin(), order.end(), std::size_t{0});

        if (order.size() < 2) return order;

        std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
            return items[a].condition.modulus < items[b].condition.modulus;
        });
        
        auto selective = order;
        
        std::stable_sort(selective.begin(), selective.end(), [&](auto a, auto b) {
            return static_cast<long double>(items[a].allowed_count) / items[a].condition.modulus <
                   static_cast<long double>(items[b].allowed_count) / items[b].condition.modulus;
        });
        
        if (selective == order) return order;
        
        // Independence is a cost estimate, not a correctness assumption.
        const auto estimate = [&](const auto& indices) {
            std::int64_t period = 1;
            long double density = 1, work = 0;
            for (auto index : indices) {
                const auto& item = items[index];
                const auto m = item.condition.modulus;
                const auto q = m / std::gcd(period, m);
                std::int64_t next;
                if (!safe_mul(period, q, next) || next > max_wheel_period_) continue;
                const auto expansions = item.allowed_count < static_cast<std::size_t>(q) / 4
                    ? item.allowed_count : static_cast<std::size_t>(q);
                work += period * density * expansions;
                density *= static_cast<long double>(item.allowed_count) / m;
                period = next;
            }
            return std::pair{density, work};
        };
        return estimate(selective) < estimate(order) ? selective : order;
    }

    std::vector<std::int64_t> combine_sparse(const Condition& cond, std::int64_t g,
                                            std::int64_t q, std::size_t count) const {
        std::vector<std::int64_t> allowed;
        allowed.reserve(count);
        for (std::int64_t r = 0; r < cond.modulus; ++r) {
            if (cond.allowed[static_cast<std::size_t>(r)]) allowed.push_back(r);
        }
        const auto inverse = inverse_mod((wheel_period_ / g) % q, q);
        std::vector<std::int64_t> result;
        result.reserve(wheel_residues_.size() * count);
        for (auto r : wheel_residues_) {
            const auto old_mod = r % cond.modulus;
            for (auto a : allowed) {
                const auto difference = a - old_mod;
                if (difference % g != 0) continue;
                const auto rhs = positive_mod(difference / g, q);
                const auto t = static_cast<std::int64_t>(multiply_mod(rhs, inverse, q));
                // 0 <= r + P*t < checked LCM.
                result.push_back(r + wheel_period_ * t);
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    std::int64_t min_;
    std::int64_t max_;
    std::int64_t max_wheel_period_;

    std::int64_t wheel_period_ = 1;
    std::vector<std::int64_t> wheel_residues_;
    std::vector<Condition> remaining_conditions_;
    bool empty_ = false;

    bool satisfies_remaining(std::int64_t x) const noexcept {
        for (const auto& cond : remaining_conditions_) {
            std::int64_t r = positive_mod(x, cond.modulus);
            if (!cond.allowed[static_cast<size_t>(r)]) {
                return false;
            }
        }
        return true;
    }
}; // ConditionalRange

} // namespace cr

using ConditionalRange = cr::ConditionalRange;
using Condition = cr::Condition;

#endif // CONDITIONAL_RANGE_HPP
