# ConditionalRange.hpp ユーザーガイド

`ConditionalRange.hpp` は、複数の合同式（周期条件）を満たす整数列を、中国の余剰定理（CRT）に基づく **Wheel（輪）最適化** と C++20 スタイルの範囲（Range / Iterator）モデルを用いて高速に生成・走査するためのヘッダーライブラリです。

---

## 目次

- [ConditionalRange.hpp ユーザーガイド](#conditionalrangehpp-ユーザーガイド)
  - [目次](#目次)
  - [1. 概要・特徴](#1-概要特徴)
  - [2. 動作原理](#2-動作原理)
  - [3. クイックスタート](#3-クイックスタート)
  - [4. API 詳細](#4-api-詳細)
    - [`cr::Condition`](#crcondition)
      - [メンバー変数](#メンバー変数)
      - [Conditionコンストラクタ](#conditionコンストラクタ)
    - [`cr::ConditionalRange`](#crconditionalrange)
      - [定数](#定数)
      - [ConditionalRangeコンストラクタ](#conditionalrangeコンストラクタ)
      - [メンバ関数](#メンバ関数)
    - [`cr::ConditionalRange::Iterator`](#crconditionalrangeiterator)
  - [5. パフォーマンス・設計上の注意点](#5-パフォーマンス設計上の注意点)
  - [6. 使用例](#6-使用例)
    - [複合条件（複数の剰余条件の適用）](#複合条件複数の剰余条件の適用)

---

## 1. 概要・特徴

- **全探索の排除**: 与えられた剰余条件からあらかじめ周期構造（Wheel）を合成し、条件を満たさない数を最初からスキップして走査します。
- **負の数・オーバーフローへの完全対応**: C++ 標準の `%` 演算子（0 方向への丸め）ではなく数論的な数学的剰余（$\mathbb{Z}/m\mathbb{Z}$）を内部で計算し、`int64_t` の限界付近でも算術オーバーフローを安全に検知します。
- **メモリ安全策 (`max_wheel_period`)**: 複合周期が巨大化してメモリを圧迫するのを防ぐため、一定以上の周期を持つ条件は後判定（`remaining_conditions`）へ自動的に退避させます。
- **ヘッダーオンリー**: C++20 以上に対応した標準ライブラリ（`<vector>`, `<numeric>`, `<ranges>` 等）のみに依存しており、追加の外部ライブラリは不要です。

---

## 2. 動作原理

整数 $x \in [\text{min}, \text{max}]$ のうち、複数条件 $x \equiv a \pmod m$ を満たすものを探す際、本ライブラリは以下のアルゴリズムを実行します：

1. **条件の整理**: 常に全可（Trivial）な条件を除外させ、全不可な条件が存在した場合は即座に空集合（`empty() == true`）と判定します。
2. **Wheel の構築**: 法 $m$ が小さい順に条件を統合し、周期 $P = \mathrm{lcm}(m_1, m_2, \dots)$ と、その周期内で条件を満たす剰余集合 $R = \{ r_1, r_2, \dots \}$ を構築します。
3. **限界値制御**: 周期 $P$ が `max_wheel_period`（デフォルト: $10,000,000$）を超えた場合、以降の条件は Wheel 構築に使わず、イテレート時の個別評価（`satisfies_remaining`）へ回します。
4. **高速走査**: イテレータは $k \cdot P + r$ ($r \in R$) の形式で値を生成し、探索空間を大幅に削減して走査します。

---

## 3. クイックスタート

```cpp
#include <iostream>
#include "ConditionalRange.hpp"

int main() {
    // 1. 条件の作成: 3 で割った余りが 0 または 2 (1 は不可)
    // index 0: true, index 1: false, index 2: true
    cr::Condition cond1(3, {true, false, true});

    // 2. [-10, 10] の範囲で ConditionalRange を構築
    cr::ConditionalRange range(-10, 10, {cond1});

    // 3. 範囲ベース for ループで走査
    for (std::int64_t val : range) {
        std::cout << val << " ";
    }
    std::cout << "\n";
    // 出力: -10 -9 -7 -6 -4 -3 -1 0 2 3 5 6 8 9

    return 0;
}

```

---

## 4. API 詳細

名前空間 `cr` 内に主要なクラスが定義されています（利便性のためグローバル空間へ `using` エイリアスも提供されています）。

### `cr::Condition`

剰余条件を表す構造体です。

#### メンバー変数

- `std::int64_t modulus`: 法 $m$ ($m > 0$)。
- `std::vector<bool> allowed`: サイズ $m$ のフラグ配列。`allowed[r] == true` のとき、余り $r$ を許可。
- `std::int64_t true_count`: `allowed` 内の `true` の要素数。

#### Conditionコンストラクタ

```cpp
// 1. allowed のみを受け取る (modulus = allowed.size() として推測)
explicit Condition(std::vector<bool> a);

// 2. modulus と allowed を指定
Condition(std::int64_t m, std::vector<bool> a);

// 3. 事前計算済みの true_count を直接指定 (O(1) 高速構築・呼び出し側責任)
Condition(std::int64_t m, std::vector<bool> a, std::int64_t tc);

```

---

### `cr::ConditionalRange`

探索範囲と条件集合を管理するメインクラスです。

#### 定数

- `DEFAULT_MAX_WHEEL_PERIOD`: `10'000'000` (Wheel 周期のデフォルト上限値)
- `VERSION_STRING`: `"1.0.0"`

#### ConditionalRangeコンストラクタ

```cpp
ConditionalRange(
    std::int64_t min_val,
    std::int64_t max_val,
    std::vector<Condition> conditions,
    std::int64_t max_wheel_period = DEFAULT_MAX_WHEEL_PERIOD
);

```

- `min_val`, `max_val`: 探索範囲の閉区間 $[\text{min\_val}, \text{max\_val}]$。
- `conditions`: 適用する `Condition` のリスト。
- `max_wheel_period`: Wheel 構築を許容する最大周期。

#### メンバ関数

| メソッド | 戻り値 | 説明 |
| --- | --- | --- |
| `begin()` | `Iterator` | 最初の有効な要素を指すイテレータを取得 |
| `end()` | `Iterator` | 終端を示すイテレータを取得 |
| `empty()` | `bool` | 条件を満たす要素が絶対に存在しない場合 `true` |
| `wheel_period()` | `std::int64_t` | 構築された Wheel の合成周期 $P$ |
| `wheel_residues()` | `const std::vector<std::int64_t>&` | Wheel 内で保持されている有効な剰余リスト $R$ |
| `remaining_conditions()` | `const std::vector<Condition>&` | 周期超過で Wheel に組み込めなかった後判定条件 |

---

### `cr::ConditionalRange::Iterator`

`std::forward_iterator_tag` に準拠した前進イテレータです。

- `operator*()`: 現在指している `std::int64_t` 型の値を返します。
- `operator++()` / `operator++(int)`: 条件を満たす次の値まで進めます。
- `operator==` / `operator!=`: 比較演算子。

---

## 5. パフォーマンス・設計上の注意点

1. **法（modulus）の指定順序**
内部で自動的に `modulus` の昇順（`std::stable_sort`）にソートされて処理されるため、引数渡しの順番を意識する必要はありません。
2. **`max_wheel_period` の調整**
   - 互いに素な大きな法が多数指定されると、周期 $P = \prod m_i$ が爆発します。
   - メモリ使用量を抑えたい場合は `max_wheel_period` を小さく（例: $10^5$ 等に）設定し、探索を安定させてください。

3. **`Condition` の高速作成**
大量の条件を動的生成する場合、`Condition(m, allowed, tc)` コンストラクタを使用すると、`std::ranges::count` の走査オーバーヘッドを削減できます。

---

## 6. 使用例

### 複合条件（複数の剰余条件の適用）

「3 で割って 1 余り、かつ 5 で割って 2 または 3 余る整数」を $[0, 100]$ で走査する例：

```cpp
#include <iostream>
#include "ConditionalRange.hpp"

int main() {
    // x ≡ 1 (mod 3)
    cr::Condition cond3(3, {false, true, false});

    // x ≡ 2, 3 (mod 5)
    cr::Condition cond5(5, {false, false, true, true, false});

    cr::ConditionalRange range(0, 100, {cond3, cond5});

    std::cout << "Wheel Period: " << range.wheel_period() << "\n"; // LCM(3, 5) = 15
    std::cout << "Valid Residues Count: " << range.wheel_residues().size() << "\n"; // 2 個

    for (std::int64_t val : range) {
        std::cout << val << " ";
    }
    std::cout << "\n";
    // 出力: 7 13 22 28 37 43 52 58 67 73 82 88 97

    return 0;
}

```
