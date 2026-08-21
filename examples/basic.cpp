#include <ConditionalRange/ConditionalRange.hpp>

#include <iostream>
#include <vector>

int main()
{
    std::vector<bool> b3 = {
        true,
        false,
        true
    };

    std::vector<bool> b5 = {
        true,
        false,
        true,
        false,
        true
    };

    ConditionalRange range(
        -100,
        100,
        {
            {3, b3},
            {5, b5}
        }
    );

    for (int64_t x : range) {
        std::cout << x << '\n';
    }
}