#include <ConditionalRange/ConditionalRange.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using I64 = std::int64_t;
using Clock = std::chrono::steady_clock;
volatile std::uint64_t sink = 0;

#if defined(_MSC_VER)
#define CR_BENCH_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CR_BENCH_NOINLINE __attribute__((noinline))
#else
#define CR_BENCH_NOINLINE
#endif

struct Case {
    const char* name;
    I64 min, max;
    std::vector<Condition> conditions;
    I64 cap = ConditionalRange::DEFAULT_MAX_WHEEL_PERIOD;
};

Condition prefix(int m, int count) {
    std::vector<bool> bits(m, false);
    for (int r = 0; r < count; ++r) bits[r] = true;
    return Condition(m,std::move(bits));
}

CR_BENCH_NOINLINE std::uint64_t build(const Case& test) {
    
    ConditionalRange range(test.min,test.max,test.conditions,test.cap);

    return static_cast<std::uint64_t>(range.wheel_period()) ^
           (static_cast<std::uint64_t>(range.wheel_residues().size()) << 17) ^
           (static_cast<std::uint64_t>(range.remaining_conditions().size()) << 43);
}

CR_BENCH_NOINLINE std::uint64_t digest(const ConditionalRange& range) {
    std::uint64_t sum = 0x9e3779b97f4a7c15ULL, count = 0;

    for (I64 x : range) {
        sum += static_cast<std::uint64_t>(x) + 0xd1b54a32d192ed03ULL;
        ++count;
    }

    return sum ^ (count * 0x94d049bb133111ebULL);
}

CR_BENCH_NOINLINE std::uint64_t full(const Case& test) {

    ConditionalRange range(test.min,test.max,test.conditions,test.cap);
    
    return digest(range);
}

template<class F> double batch(F& fn, std::size_t iterations) {
    std::uint64_t local = 0;
    const auto begin = Clock::now();

    for (std::size_t i = 0; i < iterations; ++i) {
        auto value = fn();
#if defined(__GNUC__) || defined(__clang__)
        asm volatile("" : "+r"(value) : : "memory");
#else
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
        local ^= value + i;
    }

    const auto end = Clock::now();
    sink = sink ^ local;
    return std::chrono::duration<double,std::nano>(end-begin).count();
}

template<class F> double measure(F fn) {
    (void)fn();
    std::size_t iterations = 1;
    
    while (batch(fn,iterations) < 5'000'000 && iterations < (1U << 25)) {
        iterations *= 2;
    }
    
    std::array<double,7> samples{};
    
    for (auto& sample : samples) {
        sample = batch(fn,iterations) / iterations;
    }
    
    std::sort(samples.begin(),samples.end());
    return samples[3] / 1000; // Median, microseconds/op.
}

} // namespace

int main(int argc, char** argv) {
    
    const std::vector<Case> cases{
        {
            "no conditions / 10M",
            -5'000'000, 5'000'000,
            {}
        },
        {
            "dense 90% / 10M",
            -5'000'000, 5'000'000,
            {prefix(10,9)}
        },
        {
            "ordering / 20M",
            0, 20'000'000,
            {prefix(4000,3999),prefix(4001,1)}
        },
        {
            "dense/sparse construction",
            0, 1'000,
            {prefix(1000,999),prefix(1001,1)}
        },
        {
            "sparse CRT / 1M",
            0, 1'000'000,
            {prefix(996,21),prefix(997,21)}
        },
        {
            "sparse / default cap",
            0, 1'000'000'000,
            {prefix(4001,1),prefix(4003,1)}
        },
        {
            "sparse / explicit large cap",
            0, 1'000'000'000,
            {prefix(4001,1),prefix(4003,1)},
            std::numeric_limits<I64>::max()
        },
        {
            "mixed small moduli",
            -50'000, 50'000,
            {prefix(2,1),prefix(3,2),prefix(5,4),prefix(7,6),prefix(11,5),prefix(13,5)}
        },
        {
            "small moduli vs density",
            -500, 500,
            {prefix(5,1),prefix(7,1),prefix(11,1),prefix(23,1)},
            385
        }
    };

    const bool checks_only = argc > 1 && std::string_view(argv[1]) == "--checksums";

    if (!checks_only) {
        std::cout << "Median microseconds/op; 7 calibrated samples. Full includes condition copying.\n"
                  << std::left << std::setw(30) << "Case" << std::right
                  << std::setw(13) << "Build us" << std::setw(13) << "Iterate us"
                  << std::setw(13) << "Full us" << '\n';
    }
    
    for (const auto& test : cases) {

        ConditionalRange range(test.min,test.max,test.conditions,test.cap);
        
        if (checks_only) {
            std::cout << test.name << '\t' << digest(range) << '\n';
            continue;
        }
        
        const auto construction = measure([&] { return build(test); });
        const auto iteration = measure([&] { return digest(range); });
        const auto total = measure([&] { return full(test); });
        
        std::cout << std::left << std::setw(30) << test.name << std::right
                  << std::fixed << std::setprecision(3)
                  << std::setw(13) << construction << std::setw(13) << iteration
                  << std::setw(13) << total << '\n';
    }
}
