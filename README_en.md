# ConditionalRange

`ConditionalRange` is a C++17 range that lazily iterates over integers
satisfying one or more periodic modular conditions.

The main purpose of this project is not merely to provide a convenient
range-for interface, but to exploit the periodic structure of the
conditions so that values which can never satisfy the conditions are
skipped without being individually tested.

---

## Overview

A `ConditionalRange` represents an integer interval

```text
[min, max]
````

together with a collection of modular conditions.

A condition is defined by:

* a positive modulus `m`
* a boolean table `allowed` of length `m`

For an integer `x`, the condition is satisfied when

```text
allowed[x mod m] == true
```

where `x mod m` is interpreted as the unique residue in

```text
[0, m)
```

All conditions must be satisfied.

For example:

```cpp
std::vector<bool> allowed = {
    true,
    false,
    true
};

ConditionalRange range(
    -100,
    100,
    {
        {3, allowed}
    }
);
```

This condition allows residues

```text
x mod 3 = 0
x mod 3 = 2
```

and rejects

```text
x mod 3 = 1
```

Therefore the range produces:

```text
..., -99, -98, -96, -95, -93, -92, ...
```

---

## Design Philosophy

The central idea of `ConditionalRange` is:

> **Do not iterate over every integer and then ask whether it is valid.
> Instead, use the periodicity of the conditions to construct the set of possible residues first, and iterate only over values belonging to those residue classes.**

A naive implementation would be:

```cpp
for (int64_t x = min; x <= max; ++x) {
    if (satisfies_all_conditions(x)) {
        yield(x);
    }
}
```

This is correct, but can be extremely wasteful.

If a condition says that only two residues out of every hundred are allowed,
then 98% of the integers can be rejected without evaluating the remaining
conditions.

`ConditionalRange` therefore separates the problem into two stages:

```text
Conditions
    │
    ▼
Periodic structure
    │
    ▼
Wheel construction
    │
    ▼
Candidate residue classes
    │
    ▼
Remaining conditions
    │
    ▼
Lazy iterator
```

---

## 1. Conditions are periodic

Every condition has a period.

For a modulus `m`, the result of the condition depends only on

```text
x mod m
```

Therefore the condition repeats every `m` integers.

For example:

```text
modulus = 5
allowed = {false, true, true, false, true}
```

represents:

```text
x mod 5 ∈ {1, 2, 4}
```

so the pattern repeats indefinitely:

```text
...  -1   0   1   2   3   4   5   6   7   8   9 ...
      F   F   T   T   F   T   F   T   T   F   T
```

The important point is that the range does not need to store an infinite
set of valid integers.

It only needs to store the valid residue classes within one period.

---

## 2. Combining conditions with a wheel

Suppose we have two conditions with moduli

```text
P
m
```

The combined pattern repeats with period

```text
lcm(P, m)
```

Therefore we can construct a new periodic filter.

The implementation maintains:

```text
wheel_period
wheel_residues
```

where:

* `wheel_period` is the period of the currently constructed combined
  condition
* `wheel_residues` contains every residue in `[0, wheel_period)` that
  satisfies all conditions incorporated into the wheel

Conceptually:

```text
0 <= r < wheel_period
```

and

```text
r ∈ wheel_residues
```

means that every integer of the form

```text
x = r + k * wheel_period
```

is a candidate for the range.

---

## 3. Incremental wheel construction

The wheel starts with the trivial periodic representation:

```text
wheel_period  = 1
wheel_residues = {0}
```

This represents every integer because every integer is congruent to `0`
modulo `1`.

When a condition with modulus `m` is incorporated, let

```text
g = gcd(wheel_period, m)
q = m / g
```

Then

```text
lcm(wheel_period, m)
    = wheel_period * q
```

For every currently allowed residue `r`, the implementation generates

```text
r + k * wheel_period
```

for

```text
k = 0, 1, ..., q - 1
```

and checks the new condition.

The surviving candidates become the new wheel.

Thus the wheel grows incrementally:

```text
{0}
  │
  ├── condition m = 2
  ▼
{...}
  │
  ├── condition m = 3
  ▼
{...}
  │
  ├── condition m = 5
  ▼
{...}
```

The algorithm does not repeatedly scan all integers in the new period.
It expands only the residue classes that survived the previous conditions.

---

## 4. Why the wheel is bounded

The least common multiple of many moduli can become enormous.

For example:

```text
2 × 3 × 5 × 7 × 11 × ...
```

grows extremely quickly.

Constructing a wheel with billions or trillions of residues would be
counterproductive.

Therefore the implementation has a maximum wheel period:

```cpp
MAX_WHEEL_PERIOD = 10'000'000
```

If adding a condition would make the wheel period exceed this limit,
that condition is not incorporated into the wheel.

Instead, it is placed into:

```cpp
remaining_conditions_
```

This is an important part of the design.

The wheel is an optimization, not a requirement for correctness.

---

## 5. Remaining conditions

A condition that is not incorporated into the wheel is still a normal
condition.

When the iterator finds a candidate generated by the wheel, it checks
the remaining conditions:

```text
wheel candidate
      │
      ▼
remaining condition 1
      │
      ▼
remaining condition 2
      │
      ▼
...
      │
      ▼
valid value
```

Therefore:

```text
wheel filtering
```

and

```text
ordinary condition checking
```

are deliberately separated.

This allows the wheel to remain reasonably small while still supporting
arbitrarily many conditions.

---

## 6. The wheel is an optimization layer

The semantics of `ConditionalRange` do not depend on whether a condition
was placed into the wheel.

Conceptually, these two implementations must produce exactly the same
sequence:

```cpp
for (int64_t x = min; x <= max; ++x) {
    if (satisfies_all_conditions(x)) {
        ...
    }
}
```

and:

```text
wheel candidate generation
        +
remaining-condition checking
```

The second implementation only avoids work that the first implementation
would perform unnecessarily.

This distinction is important:

> **The wheel must never change the mathematical meaning of the conditions.**

---

## 7. Lazy iteration

`ConditionalRange` is a range, not a container of all results.

The range does not generate every valid integer in advance.

Instead, values are generated by the iterator as they are requested.

For example:

```cpp
for (int64_t x : range) {
    use(x);
}
```

does not require the range to first construct a `std::vector<int64_t>`
containing all matching values.

The iterator maintains enough state to locate the next candidate.

The important iterator state includes:

```text
range
residue_index
current
end state
```

---

## 8. Candidate generation

Suppose the wheel is:

```text
period = P
residues = {r1, r2, r3}
```

Then the possible values are:

```text
r1 + kP
r2 + kP
r3 + kP
...
```

The iterator does not increment by `1`.

Instead, it moves through the residue classes directly.

For example:

```text
P = 10
residues = {1, 4, 7}
```

generates:

```text
..., -19, -16, -13,
    -9,  -6,  -3,
     1,   4,   7,
    11,  14,  17,
    ...
```

If a candidate fails a remaining condition, the iterator advances within
the same residue class before moving to another residue class.

---

## 9. Negative integers

Negative values are fully supported.

A mathematical modulo operation is used so that:

```text
positive_mod(x, m)
```

always returns a value in:

```text
[0, m)
```

For example:

```text
positive_mod(-1, 5) = 4
positive_mod(-2, 5) = 3
positive_mod(-5, 5) = 0
```

This is necessary because C++'s `%` operator for negative operands follows
the language's signed remainder rules rather than directly providing the
mathematical modulo operation needed by the condition tables.

All condition checks therefore use the normalized residue.

---

## 10. Overflow-aware arithmetic

The range uses `int64_t` for its bounds and generated values.

Because the valid range includes the entire signed 64-bit domain, arithmetic
must not casually perform expressions such as:

```cpp
min - residue
```

or:

```cpp
current + wheel_period
```

without considering overflow.

The implementation therefore uses overflow-aware calculations when:

* calculating the least common multiple
* calculating the next value of a residue class
* calculating distances to the next residue
* advancing the iterator

For example, LCM construction first divides by the GCD:

```text
lcm(a, b) = (a / gcd(a, b)) * b
```

and checks whether the multiplication is representable in `int64_t`.

Similarly, candidate advancement checks whether an addition would overflow
before performing it.

Overflow behavior is therefore part of the correctness requirements, not
merely an optimization detail.

---

## 11. Empty ranges

The range can become empty for several reasons.

### Invalid interval

If:

```text
min > max
```

the range contains no values.

### Impossible condition

If a condition has no allowed residues:

```text
allowed = {false, false, ..., false}
```

then no integer can satisfy it.

The range is immediately known to be empty.

### Wheel elimination

If all currently possible wheel residues are eliminated by a condition,
the range is also empty.

In this case there is no reason to inspect later conditions.

---

## 12. Trivial conditions

Some conditions contain no useful information.

### All true

If:

```text
allowed = {true, true, ..., true}
```

then every integer satisfies the condition.

The condition can therefore be discarded.

### All false

If:

```text
allowed = {false, false, ..., false}
```

then the entire range is empty.

These cases are handled before constructing the wheel.

---

## 13. Composite moduli are supported

The modulus does not have to be prime.

The following are all valid:

```text
2
3
4
5
8
9
16
25
27
49
...
```

Prime powers such as:

```text
p^k
```

are treated exactly like any other modulus.

The wheel construction is based on:

```text
gcd
lcm
```

rather than on primality, so no special prime-only logic is required.

---

## 14. Duplicate moduli

Multiple conditions may use the same modulus.

For example:

```cpp
{
    {5, condition_a},
    {5, condition_b}
}
```

is valid.

Mathematically, this represents the intersection of the two residue
sets.

The implementation must therefore not assume that all moduli are distinct.

---

## 15. Order of conditions

Wheel construction compares modulus and selectivity order under the same cap.
Sparse joins use generalized CRT; other joins use expansion. The public API
and default cap are unchanged, but the exposed internal plan may differ.

The ordering does not change the mathematical result because all conditions
represent an intersection:

```text
valid(x)
=
condition_1(x)
AND
condition_2(x)
AND
...
```

The implementation may therefore change the processing strategy for
performance as long as the resulting set of integers remains identical.

See [benchmarks/README.md](benchmarks/README.md) for separate construction and
iteration measurements.

---

## 16. Correctness model

The intended mathematical model is:

```text
R = { x ∈ Z | min <= x <= max }

V = { x ∈ R |
      every condition is satisfied by x }
```

`ConditionalRange` must produce exactly the elements of `V`, in increasing
order.

The internal wheel is only a representation of a subset of the periodic
constraints.

Therefore correctness can always be tested against a simple reference
implementation:

```cpp
std::vector<int64_t> expected;

for (int64_t x = min; x <= max; ++x) {
    bool ok = true;

    for (const auto& condition : conditions) {
        const int64_t r = positive_mod(x, condition.modulus);

        if (!condition.allowed[r]) {
            ok = false;
            break;
        }
    }

    if (ok) {
        expected.push_back(x);
    }
}
```

The optimized range must produce exactly the same sequence.

This reference implementation is intentionally simple and is used for
testing rather than as the production implementation.

---

## 17. Iterator requirements

The iterator is intended to behave as a forward iterator.

It supports:

```cpp
auto it = range.begin();
auto last = range.end();

while (it != last) {
    auto x = *it;
    ++it;
}
```

and therefore normal range-for syntax:

```cpp
for (int64_t x : range) {
    ...
}
```

The iterator is lazy.

Incrementing it performs only the amount of work necessary to locate the
next valid integer.

---

## 18. Complexity philosophy

The naive algorithm examines every integer in the range.

If the interval contains `N` integers, this costs approximately:

```text
O(N × number_of_conditions)
```

The purpose of the wheel is to reduce the number of integers that reach
the remaining-condition checks.

If a wheel has period `P` and contains `W` valid residues, then only those
`W` residue classes need to be traversed.

The effectiveness of the optimization therefore depends on the density of
the surviving residue classes.

A wheel is beneficial when:

```text
W / P
```

is significantly smaller than `1`.

However, constructing an excessively large wheel is also expensive.

This is why the implementation uses a bounded wheel rather than attempting
to construct the complete LCM of all moduli.

---

## 19. Design priorities

The project prioritizes the following order:

1. **Mathematical correctness**
2. **Correct behavior for the entire `int64_t` domain**
3. **Lazy iteration**
4. **Avoidance of unnecessary candidate evaluation**
5. **Reasonable wheel construction cost**
6. **Simple and maintainable implementation**

The wheel should never be optimized at the expense of correctness.

In particular:

> A smaller but correct wheel is preferable to a larger wheel whose
> construction or arithmetic is unsafe.

---

## 20. Current implementation architecture

The main class is:

```cpp
ConditionalRange
```

Its important internal concepts are:

```text
min_
max_

wheel_period_
wheel_residues_

remaining_conditions_

empty_
```

### `wheel_period_`

The period of the currently constructed wheel.

### `wheel_residues_`

The residues within `wheel_period_` that satisfy all conditions incorporated
into the wheel.

### `remaining_conditions_`

Conditions which were not incorporated into the wheel, usually because
expanding the wheel would exceed the configured maximum period.

### `empty_`

Whether the range is known to contain no values.

---

## 21. Public condition representation

A condition is represented by:

```cpp
struct Condition {
    int64_t modulus;
    std::vector<bool> allowed;
};
```

The following invariant is required:

```text
modulus > 0
allowed.size() == modulus
```

Otherwise construction throws `std::invalid_argument`.

---

## 22. Example

```cpp
#include <ConditionalRange/ConditionalRange.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    std::vector<bool> divisible_by_3_pattern = {
        true,
        false,
        true
    };

    ConditionalRange range(
        -20,
        20,
        {
            {3, divisible_by_3_pattern}
        }
    );

    for (std::int64_t x : range) {
        std::cout << x << '\n';
    }
}
```

The range produces only values whose residue modulo `3` is `0` or `2`.

---

## 23. Multiple conditions

Multiple conditions are combined with logical AND.

For example:

```cpp
std::vector<bool> mod3 = {
    true,
    false,
    true
};

std::vector<bool> mod5 = {
    false,
    true,
    true,
    false,
    true
};

ConditionalRange range(
    -100,
    100,
    {
        {3, mod3},
        {5, mod5}
    }
);
```

A value is returned only if it satisfies both tables.

---

## 24. Testing philosophy

Testing is based on comparison with a naive reference implementation.

The test suite includes:

* empty ranges
* ranges without conditions
* a single modulus
* all-true conditions
* all-false conditions
* negative ranges
* ranges crossing zero
* prime powers
* composite moduli
* duplicate moduli
* boundary values
* randomized conditions
* randomized ranges

Random tests use deterministic seeds so that a failure can be reproduced.

The fundamental test invariant is:

```text
optimized result == naive result
```

The optimization strategy is free to change internally as long as this
invariant remains true.

---

## 25. Non-goals

`ConditionalRange` is not intended to:

* materialize all matching values in advance
* require moduli to be prime
* require moduli to be pairwise coprime
* restrict values to non-negative integers
* use a gigantic complete LCM wheel
* replace a general-purpose arbitrary-precision integer range
* change the mathematical meaning of the supplied conditions for the sake
  of optimization

---

## 26. Future optimization directions

Possible future improvements include:

* better ordering of wheel conditions
* estimating wheel density before incorporating a condition
* choosing between wheel expansion and deferred checking based on cost
* more efficient storage of sparse residue sets
* optimized candidate advancement
* stronger overflow-safe arithmetic primitives
* additional iterator optimizations
* benchmarking different wheel construction strategies

These are implementation strategies only.

The core semantic model remains:

```text
integer interval
    +
periodic modular predicates
    ↓
exact intersection
    ↓
lazy increasing iteration
```

---

## Summary

`ConditionalRange` is fundamentally a **periodic constraint solver exposed
as a C++ range**.

The important distinction from a normal filtered range is that it does not
intend to evaluate every integer individually.

Instead, it recognizes that modular conditions define periodic sets:

```text
condition
    ↓
allowed residues
    ↓
periodic set
```

and combines those sets into a bounded wheel:

```text
multiple periodic conditions
          ↓
       wheel
          ↓
 candidate residue classes
          ↓
 remaining conditions
          ↓
 lazy iterator
```

The wheel is therefore an optimization of the search space, while the
conditions remain the source of truth for correctness.

The intended result is a range that is:

* mathematically exact
* lazy
* `int64_t` aware
* negative-number aware
* independent of prime/composite modulus restrictions
* capable of handling duplicate conditions
* able to exploit strong periodic sparsity
* safe when a complete wheel would become too large

The central principle of the project is:

> **Exploit periodic structure before evaluating individual integers.**
