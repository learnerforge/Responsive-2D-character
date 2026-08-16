# tests/test_controller.cpp — Controller State Machine Tests

## File role

Unit tests for `character::Controller` using an injected fake cursor source. The
most important test file — it verifies the whole behavior spec without a GUI.

## Fake cursor source

```cpp
#include "character/Controller.h"   // also pulls in config.h for cfg tokens
#include <cstdio>
#include <cmath>

struct FakeCursor : character::ICursorSource {
  int x = 0, y = 0;
  bool available = true;
  long long now = 0;                      // test clock (ms)
  bool Get(int& cx, int& cy) override {
    if (!available) return false;
    cx = x; cy = y; return true;
  }
};

static FakeCursor cur;
static character::Controller ctl;
```

The test drives the controller with a manual clock: `ctl.Tick(t)` with `t`
advanced by 33 ms steps, and moves `cur.x/y` to simulate cursor behavior.

## Test cases

### 1. Dead zone: tiny cursor moves stay Idle
```
ctl.SetCursorSource(&cur);
ctl.SetWindowCenter(1000, 1000);
ctl.Tick(0);
// move cursor within 4 px dead zone
cur.x = 1003; cur.y = 1001;
ctl.Tick(33);
CHECK(ctl.State() == character::CharState::Idle);
```

### 2. Idle → Tracking when the cursor moves beyond the dead zone
```
cur.x = 1200; cur.y = 1050;      // 200+ px away
ctl.Tick(66);
CHECK(ctl.State() == character::CharState::Tracking);
```

### 3. Tracking → Idle after 3 s of stillness
```
// hold cursor still, tick 3.2 s
for (int i = 0; i < 100; ++i) ctl.Tick(now += 33);
CHECK(ctl.State() == character::CharState::Idle);
```

### 4. Pupils point toward the cursor (direction sanity)
```
// cursor directly right → pupilX positive, pupilY ~0
cur.x = 1200; cur.y = 1000; ctl.Tick(now += 33);
auto st = ctl.Current();
CHECK(st.pupilX > 0.0f && std::fabs(st.pupilY) < 0.5f);
// cursor directly below → pupilY positive
cur.x = 1000; cur.y = 1300; ctl.Tick(now += 33);
st = ctl.Current();
CHECK(st.pupilY > 0.0f && std::fabs(st.pupilX) < 0.5f);
```

### 5. Pupil magnitude clamped to kMaxEyeOffset
```
cur.x = 5000; cur.y = 1000;      // very far
for (int i = 0; i < 30; ++i) ctl.Tick(now += 33);   // let spring settle
auto st = ctl.Current();
CHECK(hypotf(st.pupilX, st.pupilY) <= cfg::kMaxEyeOffset + 0.01f);
```

### 6. Click → Interacting → back within 1.2 s
```
ctl.OnLeftClick();  ctl.Tick(now += 33);
CHECK(ctl.State() == character::CharState::Interacting);
CHECK(ctl.Current().surprised == true);
// after 1.3 s (still cursor) → Idle
for (int i = 0; i < 40; ++i) ctl.Tick(now += 33);
CHECK(ctl.State() == character::CharState::Idle);
CHECK(ctl.Current().surprised == false);
CHECK(ctl.Current().hopY == 0.0f);
```

### 7. Blink scheduling within [min, max]
```
// force a fresh controller; drive 20 s of idle ticks, collect blink timestamps
// (blink triggers when draw_.blink < 1.0). Assert first blink happens in
// [kBlinkMinIntervalMs, kBlinkMaxIntervalMs] and each subsequent gap is within
// the same bounds. Requires Controller to expose blink value via Current().
bool sawBlink = false; long long firstBlink = -1;
for (long long t = 0; t < 60'000; t += 33) {
  ctl.Tick(t);
  if (ctl.Current().blink < 0.999f && !sawBlink) { sawBlink = true; firstBlink = t; }
}
CHECK(sawBlink);
CHECK(firstBlink >= cfg::kBlinkMinIntervalMs && firstBlink <= cfg::kBlinkMaxIntervalMs);
```

### 8. Cursor unavailable → no crash, state preserved
```
cur.available = false;
ctl.Tick(now += 33);                 // must not throw / change state
cur.available = true;
CHECK(ctl.State() == character::CharState::Idle);
```

### 9. Dirty flag: no change → no repaint; change → repaint
```
ctl.ClearRepaint();
ctl.Tick(now += 33);                 // nothing changed since last tick
CHECK(!ctl.WantsRepaint());
cur.x += 50; ctl.Tick(now += 33);    // cursor moved → dirty
CHECK(ctl.WantsRepaint());
```

## main / integration

Same pattern as [test_animator.cpp.md](test_animator.cpp.md): `CHECK` macro,
`g_failures`, return 1 on failure. Links `src/character/Controller.cpp`,
`src/character/Animator.cpp`. Registered as `test_controller` in
[CMakeLists.txt.md](CMakeLists.txt.md).

> Note: test 7 requires the controller to expose blink through `Current()` and
> blink scheduling to be deterministic enough; the `[min,max]` assertion is
> statistical (uniform random) — run with a fixed seed for reproducibility, or
> make the RNG injectable. Documented as a known test-design consideration.

## Related

- [Controller.h.md](character/Controller.h.md),
  [Controller.cpp.md](character/Controller.cpp.md)
- [States.h.md](character/States.h.md)
- [config.h.md](config.h.md)
