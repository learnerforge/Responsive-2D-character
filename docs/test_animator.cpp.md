# tests/test_animator.cpp — Spring Math Tests

## File role

Unit tests for `character::Spring` (see
[Animator.h.md](character/Animator.h.md)). Console executable, no test
framework — prints failures and returns non-zero so CTest fails loudly.

## Test harness

```cpp
#include "character/Animator.h"
#include <cmath>
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++g_failures;                                     \
       std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } }  \
  while (0)

static float Close(float a, float b, float eps) { return std::fabs(a - b) < eps; }
```

> `CHECK` macro keeps the tests dependency-free and readable. CTest only needs
> the exit code.

## Test cases

### 1. Convergence
```
Spring s(0.06f);
for (int i = 0; i < 1000; ++i) s.Tick(10.0f, 1.0f / 60.0f);
CHECK(Close(s.Value(), 10.0f, 0.01f));
```

### 2. No overshoot (critical damping)
```
Spring s(0.06f);
s.Reset(0.0f);
float prev = s.Value();
for (int i = 0; i < 300; ++i) {
  float v = s.Tick(5.0f, 1.0f / 60.0f);
  CHECK(v >= prev);        // strictly monotonic approach upward
  CHECK(v <= 5.0f);        // never exceeds target
  prev = v;
}
```
Also mirrored: reset to 5, target 0 → strictly decreasing, never below 0.

### 3. Frame-rate independence
```
Spring a(0.06f), b(0.06f);
for (int i = 0; i < 60; ++i) { a.Tick(7.0f, 1.0f/60.0f); }
for (int i = 0; i < 30; ++i) { b.Tick(7.0f, 1.0f/30.0f); }
CHECK(Close(a.Value(), b.Value(), 1e-3f));   // same total time, same result
```

### 4. dt ≤ 0 is a no-op
```
Spring s(0.06f); s.Reset(3.0f);
float v = s.Tick(9.0f, 0.0f);
CHECK(Close(v, 3.0f, 0.0f));
v = s.Tick(9.0f, -1.0f);
CHECK(Close(v, 3.0f, 0.0f));
```

### 5. Reset snaps
```
Spring s(0.06f); s.Tick(5.0f, 1.0f/30.0f);
s.Reset(2.0f);
CHECK(Close(s.Value(), 2.0f, 0.0f));
```

### 6. SetTau clamps to positive epsilon
```
Spring s(0.06f);
s.SetTau(0.0f);
CHECK(s.Tau() > 0.0f);
```

## main

```cpp
int main() {
  test_convergence();
  test_no_overshoot();
  test_frame_independence();
  test_dt_noop();
  test_reset();
  test_tau_clamp();

  std::printf("test_animator: %s (%d failure(s))\n",
              g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
```

## Rationale for tolerances

- Convergence: 1000 ticks at 60 Hz = ~16.7 s; `τ=0.06` reaches 99.9 % in
  ~0.4 s → 0.01 tolerance is generous.
- Frame-independence: float `expf` error across 60 vs 30 calls — 1e-3 is safe.

## Integration

Registered in [CMakeLists.txt.md](CMakeLists.txt.md) as `test_animator`
(console, links `src/character/Animator.cpp`). Run via `ctest --test-dir build
-C Release`.

## Related

- [Animator.h.md](character/Animator.h.md),
  [Animator.cpp.md](character/Animator.cpp.md)
