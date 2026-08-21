#include <ConditionalRange/ConditionalRange.hpp>

#include <cassert>
#include <vector>

int main()
{
    std::vector<bool> b3 = {
        true,
        false,
        true
    };

    ConditionalRange range(
        0,
        100,
        {
            {3, b3}
        }
    );

    for (int64_t x : range) {
        assert(x % 3 == 0 || x % 3 == 2);
    }
}