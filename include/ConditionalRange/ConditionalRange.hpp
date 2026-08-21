#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

class ConditionalRange {
public:
    struct Condition {
        int64_t modulus;
        std::vector<bool> allowed;

        Condition(int64_t m, std::vector<bool> bits)
            : modulus(m), allowed(std::move(bits))
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

private:
    static constexpr int64_t MAX_WHEEL_PERIOD = 10'000'000;

    int64_t min_;
    int64_t max_;

    /*
     * wheel_period_:
     *
     *   0 <= residue < wheel_period_
     *
     * の範囲で、wheel_conditions_を全て満たすresidueだけを
     * wheel_residues_に保持する。
     */
    int64_t wheel_period_ = 1;

    std::vector<int64_t> wheel_residues_;

    /*
     * wheelに入らなかった条件。
     *
     * wheel_residues_から生成したxについてのみ検査する。
     */
    std::vector<Condition> remaining_conditions_;

    bool empty_ = false;

    // ------------------------------------------------------------
    // 数学ユーティリティ
    // ------------------------------------------------------------

    static int64_t gcd_i64(int64_t a, int64_t b)
    {
        while (b != 0) {
            const int64_t t = a % b;
            a = b;
            b = t;
        }
        return a;
    }

    /*
     * a * b が int64_t に収まるか確認しながらlcmを計算。
     *
     * overflowする場合は false を返す。
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
     * floor(a / b)
     *
     * C++の整数除算は負数について0方向に丸めるため、
     * 数学的floorを自前で計算する。
     */
    static int64_t floor_div(int64_t a, int64_t b)
    {
        // b > 0
        int64_t q = a / b;
        int64_t r = a % b;

        if (r < 0) {
            --q;
        }

        return q;
    }

    /*
     * ceil(a / b)
     *
     * b > 0 前提。
     */
    static int64_t ceil_div(int64_t a, int64_t b)
    {
        int64_t q = a / b;
        int64_t r = a % b;

        if (r > 0) {
            ++q;
        }

        return q;
    }

    /*
     * x mod m を常に [0,m) にする。
     */
    static int64_t positive_mod(int64_t x, int64_t m)
    {
        const int64_t r = x % m;
        return r < 0 ? r + m : r;
    }

    // ------------------------------------------------------------
    // Condition utilities
    // ------------------------------------------------------------

    static bool all_true(const std::vector<bool>& bits)
    {
        for (bool b : bits) {
            if (!b) {
                return false;
            }
        }

        return true;
    }

    static bool all_false(const std::vector<bool>& bits)
    {
        for (bool b : bits) {
            if (b) {
                return false;
            }
        }

        return true;
    }

    /*
     * xがconditionを満たすか。
     */
    static bool satisfies(
        int64_t x,
        const Condition& condition
    )
    {
        const int64_t r = positive_mod(x, condition.modulus);

        return condition.allowed[
            static_cast<std::size_t>(r)
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

    // ------------------------------------------------------------
    // iteratorが次の値を探す
    // ------------------------------------------------------------

    int64_t first_x_for_residue(
        int64_t residue
    ) const
    {
        /*
         * x = residue + k * wheel_period_
         *
         * かつ x >= min_
         *
         * となる最小のx。
         */

        const int64_t diff = min_ - residue;

        const int64_t k = ceil_div(
            diff,
            wheel_period_
        );

        /*
         * ここでの積がint64_tを超えることは、
         * min/maxとwheel_periodの範囲から通常は起きないが、
         * 念のため安全に扱う。
         */

        if (k > 0) {
            const int64_t max_k =
                (std::numeric_limits<int64_t>::max() - residue)
                / wheel_period_;

            if (k > max_k) {
                return std::numeric_limits<int64_t>::max();
            }
        }

        if (k < 0) {
            const int64_t min_k =
                (std::numeric_limits<int64_t>::min() - residue)
                / wheel_period_;

            if (k < min_k) {
                return std::numeric_limits<int64_t>::min();
            }
        }

        return residue + k * wheel_period_;
    }

    // ------------------------------------------------------------
    // wheel構築
    // ------------------------------------------------------------

    void build(
        std::vector<Condition> conditions
    )
    {
        if (min_ > max_) {
            empty_ = true;
            return;
        }

        /*
         * まず明らかな冗長条件を除去する。
         *
         * all true:
         *   条件として意味がないので削除。
         *
         * all false:
         *   何をしても成立しないのでrange全体が空。
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
         * 小さいmodulusから処理する。
         *
         * 今回のような
         *
         *   2,3,5,7,11,13,...
         *
         * を想定すると自然に小さいwheelから育つ。
         */
        std::sort(
            useful.begin(),
            useful.end(),
            [](const Condition& a, const Condition& b) {
                return a.modulus < b.modulus;
            }
        );

        /*
         * wheelに入れる条件を決定する。
         *
         * 新しいmodulusを加えたLCMが
         * MAX_WHEEL_PERIOD以下ならwheelに入れる。
         *
         * 超えるならremainingへ。
         */
        std::vector<Condition> wheel_conditions;

        for (auto& condition : useful) {
            int64_t new_period;

            if (try_lcm(
                    wheel_period_,
                    condition.modulus,
                    new_period
                ) &&
                new_period <= MAX_WHEEL_PERIOD)
            {
                wheel_period_ = new_period;
                wheel_conditions.push_back(
                    std::move(condition)
                );
            }
            else {
                remaining_conditions_.push_back(
                    std::move(condition)
                );
            }
        }

        /*
         * wheel_period_の範囲を直接走査して、
         * wheel_conditionsを全て満たす剰余だけ保存する。
         *
         * 今回の想定では最大1000万程度なので、
         * range生成ごとに一度これを行う。
         */
        wheel_residues_.reserve(
            static_cast<std::size_t>(
                wheel_period_ / 4
            )
        );

        for (int64_t r = 0; r < wheel_period_; ++r) {
            bool ok = true;

            for (const auto& condition : wheel_conditions) {
                if (!condition.allowed[
                    static_cast<std::size_t>(
                        r % condition.modulus
                    )
                ]) {
                    ok = false;
                    break;
                }
            }

            if (ok) {
                wheel_residues_.push_back(r);
            }
        }

        /*
         * wheel条件を満たす剰余が一つもなければ空。
         */
        if (wheel_residues_.empty()) {
            empty_ = true;
        }
    }

public:
    // ============================================================
    // iterator
    // ============================================================

    class iterator {
        const ConditionalRange* range_ = nullptr;

        std::size_t residue_index_ = 0;
        int64_t current_ = 0;

        bool end_ = true;

        void seek_next_valid()
        {
            if (range_ == nullptr || range_->empty_) {
                end_ = true;
                return;
            }

            while (residue_index_ <
                   range_->wheel_residues_.size())
            {
                const int64_t residue =
                    range_->wheel_residues_[residue_index_];

                /*
                 * current_がこのresidue系列上で
                 * 最初にmin以上になる値。
                 */
                int64_t candidate;

                if (current_ <=
                    range_->first_x_for_residue(residue))
                {
                    candidate =
                        range_->first_x_for_residue(residue);
                }
                else {
                    /*
                     * 現在位置より後ろにある
                     * 同じresidue系列の値を求める。
                     */
                    const int64_t diff =
                        current_ - residue;

                    const int64_t k =
                        ceil_div(
                            diff,
                            range_->wheel_period_
                        );

                    /*
                     * candidate = residue + k * period
                     */
                    if (k > 0 &&
                        k >
                        (std::numeric_limits<int64_t>::max()
                         - residue)
                        / range_->wheel_period_)
                    {
                        ++residue_index_;
                        continue;
                    }

                    candidate =
                        residue +
                        k * range_->wheel_period_;
                }

                if (candidate > range_->max_) {
                    ++residue_index_;

                    if (residue_index_ <
                        range_->wheel_residues_.size())
                    {
                        current_ =
                            range_->first_x_for_residue(
                                range_->wheel_residues_[
                                    residue_index_
                                ]
                            );
                    }

                    continue;
                }

                if (range_->satisfies_remaining(candidate)) {
                    current_ = candidate;
                    end_ = false;
                    return;
                }

                /*
                 * 同じresidue系列の次の値へ。
                 */
                if (candidate >
                    std::numeric_limits<int64_t>::max()
                    - range_->wheel_period_)
                {
                    ++residue_index_;
                    continue;
                }

                current_ =
                    candidate +
                    range_->wheel_period_;
            }

            end_ = true;
        }

    public:
        using value_type = int64_t;
        using difference_type = std::ptrdiff_t;
        using iterator_category =
            std::forward_iterator_tag;
        using reference = int64_t;
        using pointer = void;

        iterator() = default;

        explicit iterator(
            const ConditionalRange* range,
            bool is_end
        )
            : range_(range),
              residue_index_(0),
              current_(0),
              end_(is_end)
        {
            if (!is_end && range_ != nullptr) {
                current_ = range_->min_;
                seek_next_valid();
            }
        }

        int64_t operator*() const
        {
            return current_;
        }

        iterator& operator++()
        {
            if (end_) {
                return *this;
            }

            /*
             * 同じresidue系列の次の値をまず試す。
             */
            if (current_ >
                std::numeric_limits<int64_t>::max()
                - range_->wheel_period_)
            {
                ++residue_index_;

                if (residue_index_ >=
                    range_->wheel_residues_.size())
                {
                    end_ = true;
                    return *this;
                }

                current_ =
                    range_->first_x_for_residue(
                        range_->wheel_residues_[
                            residue_index_
                        ]
                    );
            }
            else {
                current_ += range_->wheel_period_;
            }

            seek_next_valid();

            return *this;
        }

        iterator operator++(int)
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(
            const iterator& a,
            const iterator& b
        )
        {
            if (a.end_ && b.end_) {
                return true;
            }

            return a.range_ == b.range_
                && a.residue_index_ == b.residue_index_
                && a.current_ == b.current_
                && a.end_ == b.end_;
        }

        friend bool operator!=(
            const iterator& a,
            const iterator& b
        )
        {
            return !(a == b);
        }
    };

    // ============================================================
    // Constructor
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
    // begin/end
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
    // 情報取得
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