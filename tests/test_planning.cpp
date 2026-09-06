#include <ConditionalRange/ConditionalRange.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <ranges>

namespace {
using I = std::int64_t;
constexpr I low = std::numeric_limits<I>::min();
constexpr I high = std::numeric_limits<I>::max();

void require(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

Condition prefix(int m, int count) {
    std::vector<bool> bits(m, false);
    for (int r = 0; r < count; ++r) bits[r] = true;
    return Condition(m, std::move(bits));
}

Condition mask(int m, unsigned bits) {
    std::vector<bool> allowed(m);
    for (int r = 0; r < m; ++r) allowed[r] = (bits >> r) & 1U;
    return Condition(m, std::move(allowed));
}

std::vector<I> oracle(I a, I b, const std::vector<Condition>& conditions) {
    std::vector<I> result;
    if (a > b) return result;
    for (I x = a;; ++x) {
        bool accepted = true;
        for (const auto& c : conditions) {
            I r = x % c.modulus;
            if (r < 0) r += c.modulus;
            if (!c.allowed[static_cast<std::size_t>(r)]) { accepted = false; break; }
        }
        if (accepted) result.push_back(x);
        if (x == b) break; // Never increment INT64_MAX.
    }
    return result;
}

std::vector<I> collect(const ConditionalRange& range) {
    std::vector<I> values;
    for (I x : range) {
        require(values.size() < 10000, "iteration must terminate");
        values.push_back(x);
    }
    return values;
}

void check(I a, I b, const std::vector<Condition>& cs, I cap) {
    ConditionalRange range(a, b, cs, cap);
    require(collect(range) == oracle(a, b, cs), "ordered output must equal oracle");
    auto first = range.begin();
    if (first != range.end()) {
        auto independent = first;
        const auto value = *first;
        auto previous = first++;
        require(*independent == value && *previous == value, "forward iterator multipass");
    }
}

void exhaustive() {
    for (int m = 1; m <= 5; ++m) for (int n = 1; n <= 5; ++n)
    for (unsigned u = 0; u < (1U << m); ++u)
    for (unsigned v = 0; v < (1U << n); ++v)
    for (I cap : {I{0}, I{3}, I{30}}) {
        const std::vector<Condition> cs{mask(m,u), mask(n,v)};
        check(-35,35,cs,cap);
        check(3,3,cs,cap);
        check(5,-5,cs,cap);
        check(low,low+35,cs,cap);
        check(high-35,high,cs,cap);
    }
    for (int m = 1; m <= 16; ++m) for (int n = 1; n <= 16; ++n)
    for (int r = 0; r < m; ++r) for (int s = 0; s < n; ++s) {
        const I p = std::lcm(m,n);
        check(-p,p,{mask(m,1U<<r),mask(n,1U<<s)},1000);
    }
}

void randomized() {
    std::mt19937_64 rng(0x713a0984);
    for (int i = 0; i < 10000; ++i) {
        std::vector<Condition> cs;
        const unsigned count = rng() % 8;
        for (unsigned j = 0; j < count; ++j) {
            const int m = 1 + rng() % 45;
            const unsigned probability = rng() % 101;
            std::vector<bool> bits(m);
            for (int r = 0; r < m; ++r) bits[r] = rng() % 100 < probability;
            cs.emplace_back(m,std::move(bits));
        }
        I a = static_cast<I>(rng() % 801) - 400;
        I b = a + static_cast<I>(rng() % 301);
        if (i % 3 == 0) { a = low; b = low + 100; }
        if (i % 3 == 1) { a = high - 100; b = high; }
        check(a,b,cs,static_cast<I>(rng() % 1001) - 1);
    }
}

void ownership() {
    const std::vector<Condition> cs{mask(3,5),mask(5,3)};
    const auto expected = oracle(-20,20,cs);
    for (int operation = 0; operation < 4; ++operation) {
        auto source = std::make_unique<ConditionalRange>(-20,20,cs);
        std::unique_ptr<ConditionalRange> dest;
        if (operation == 0) dest = std::make_unique<ConditionalRange>(*source);
        else if (operation == 1) dest = std::make_unique<ConditionalRange>(std::move(*source));
        else {
            dest = std::make_unique<ConditionalRange>(0,0,std::vector<Condition>{});
            if (operation == 2) *dest = *source;
            else *dest = std::move(*source);
        }
        source.reset();
        require(collect(*dest) == expected, "copy/move must own its conditions and wheel");
    }
}

void planning_regressions() {
    const std::vector<Condition> cs{prefix(4000,3999),prefix(4001,1)};
    ConditionalRange range(-100,100,cs);
    require(range.wheel_residues().size() == 1, "selective wheel under period cap");
    require(!range.remaining_conditions().empty(), "period cap remains effective");
    require(collect(range) == oracle(-100,100,cs), "selective wheel output");

    // 1/385 beats 1/23 under this cap.
    const std::vector<Condition> small{prefix(5,1),prefix(7,1),prefix(11,1),prefix(23,1)};
    ConditionalRange combined(-400,400,small,385);
    require(combined.wheel_period() == 385, "retain better small-modulus plan");
    require(collect(combined) == oracle(-400,400,small), "small-modulus plan output");

    std::vector<Condition> large;
    for (int m : {101,103,107,109,113,127,131,137,139,149}) large.push_back(prefix(m,1));
    check(-200,200,large,high);
    check(low,low+300,large,high);
    check(high-300,high,large,high);

    check(low,low,{mask(3,2)},30); // k*P underflows, but k*P+r is INT64_MIN.
    check(low,low+3,{mask(3,6)},30);
    check(low,low,{},30);
    check(high,high,{},30);
    check(1,0,{},30);
}
} // namespace

static_assert(std::forward_iterator<ConditionalRange::Iterator>);
static_assert(std::ranges::forward_range<ConditionalRange>);

int main() {
    exhaustive();
    randomized();
    ownership();
    planning_regressions();
    std::cout << "Planning, boundary, oracle and ownership regressions passed\n";
}
