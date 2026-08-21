# ConditionalRange

A C++17 range that iterates only over integers satisfying
multiple modular bit conditions.

## Example

```cpp
#include <ConditionalRange/ConditionalRange.hpp>

std::vector<bool> B3 = {
    true,
    false,
    true
};

ConditionalRange range(
    -100,
    100,
    {
        {3, B3}
    }
);

for (int64_t x : range) {
    // ...
}
````

Each condition consists of:

* a positive modulus `m`
* a `std::vector<bool>` of length `m`

An integer `x` satisfies the condition if:

```text
B[x mod m] == true
```

All conditions must be satisfied.

## Requirements

* C++17 or later

---

考え:

* int64_t の min/max
* std::vector\<bool\> を入力
* modulus は素数に限定しない
* p^k もそのまま扱える
* 同じ modulus の条件も処理可能
* 全true条件は自動除外
* 全false条件なら空range
* 小さい条件を組み合わせて最大約1000万周期までwheel化
* wheelに入れなかった条件は、wheel候補に対して後から判定
* range-for で使用可能
* operator++() による遅延生成
* 負数にも対応
* int64_t のオーバーフローを避ける
