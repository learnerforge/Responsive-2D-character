# tests/test_math.cpp — Geometry / Clamp Math Tests

## File role

Unit tests for the small pure-math pieces the character relies on: pupil
clamping, head-tilt clamping, the hop parabola, and the `util::Clamp` /
`util::SanitizeDt` helpers. No GUI, no GDI+, no Win32 required.

## What it exercises

The math is implemented inline in the modules under test, so this file tests the
**contracts** documented in [Controller.cpp.md](character/Controller.cpp.md)
and [WinUtil.h.md](util/WinUtil.h.md) by replicating the same formulas in a
test-local helper OR by including the real headers and checking their inline
functions. Preferred: include the real headers so we test actual code:

```cpp
#include "util/WinUtil.h"      // util::Clamp, util::SanitizeDt
#include "character/Animator.h"
#include <cmath>
#include <cstdio>
```

## Test cases

### 1. util::Clamp
```
CHECK(util::Clamp(0.5f, 0.0f, 1.0f) == 0.5f);
CHECK(util::Clamp(-1.0f, 0.0f, 1.0f) == 0.0f);   // below → lo
CHECK(util::Clamp(2.0f, 0.0f, 1.0f) == 1.0f);    // above → hi
CHECK(util::Clamp(5, 0, 10) == 5);               // int overload
```

### 2. util::SanitizeDt
```
CHECK(util::SanitizeDt(1.0f / 30.0f) == 1.0f / 30.0f);   // normal → unchanged
CHECK(util::SanitizeDt(0.0f) == 0.016f);                 // ≤0 → default
CHECK(util::SanitizeDt(5.0f) == 0.016f);                 // huge → default
```

### 3. Pupil target clamp (angle + magnitude)
Reproduces `UpdatePupilTarget` math from
[Controller.cpp.md](character/Controller.cpp.md):
```
// helper replicating the formula (documented contract)
auto pupilTarget = [](float lx, float ly) {
  float th = atan2f(ly, lx);
  float r  = std::min(cfg::kMaxEyeOffset, hypotf(lx, ly));
  return std::make_pair(cosf(th) * r, sinf(th) * r);
};

auto [px, py] = pupilTarget(200.0f, 0.0f);   // far right
CHECK(px == cfg::kMaxEyeOffset && py == 0.0f);           // capped at max

auto [qx, qy] = pupilTarget(3.0f, 0.0f);                 // near
CHECK(qx == 3.0f && qy == 0.0f);                          // not capped

auto [sx, sy] = pupilTarget(10.0f, 10.0f);                // diagonal
float mag = hypotf(sx, sy);
CHECK(mag <= cfg::kMaxEyeOffset + 1e-4f);                 // magnitude bounded
CHECK(sx > 0.0f && sy > 0.0f);                            // correct quadrant
```

### 4. Head-tilt clamp
```
float tilt = util::Clamp(atan2f(100.0f, 1.0f) * cfg::kHeadTiltAngleScale,
                         -cfg::kHeadMaxTilt, cfg::kHeadMaxTilt);
CHECK(tilt == cfg::kHeadMaxTilt);            // extreme angle → clamped
float small = util::Clamp(0.1f * cfg::kHeadTiltAngleScale,
                          -cfg::kHeadMaxTilt, cfg::kHeadMaxTilt);
CHECK(small < cfg::kHeadMaxTilt);            // small angle passes through
```

### 5. Hop parabola returns to 0
```
// v = kHopHeightPx * 4 * t * (1 - t), t in [0,1]
float v(float t) { return cfg::kHopHeightPx * 4 * t * (1 - t); }
CHECK(v(0.0f) == 0.0f);
CHECK(v(0.5f) == cfg::kHopHeightPx);         // peak = height
CHECK(v(1.0f) == 0.0f);                      // lands back on the ground
CHECK(v(0.25f) > 0.0f && v(0.75f) > 0.0f);   // symmetric positive
```

### 6. Spring-time scaling (see test_animator)
- Included here is a cross-check that `SanitizeDt` keeps a `Spring` stable for
  the first tick after a wake-up (large dt sanitized to default).

## main / integration

Standard pattern: `CHECK` macro + `g_failures`, `main` returns 1 on any failure.
Registered as `test_math` in [CMakeLists.txt.md](CMakeLists.txt.md). Links
`src/character/Animator.cpp` only — `util::Clamp` / `util::SanitizeDt` are
header-inline so no `WinUtil.cpp` is needed.

## Value of this file

- Catches regressions in the angle/clamp/parabola math **without** a display —
  the formula-level bugs that GDI+ visuals would hide behind "looks off".
- Verifies the exact tolerances the controller relies on (pupils never escape
  the eyeball, hop always lands at 0).

## Related

- [Controller.cpp.md](character/Controller.cpp.md) — formulas under test
- [WinUtil.h.md](util/WinUtil.h.md) — `Clamp`, `SanitizeDt`
- [config.h.md](config.h.md) — the token values used
