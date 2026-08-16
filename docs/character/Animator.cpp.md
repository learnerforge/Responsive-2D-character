# src/character/Animator.cpp — Spring Implementation

## File role

Implements `Spring` with the damped-exponential update rule.

## Implementation

```cpp
#include "character/Animator.h"
#include <cmath>

namespace character {

Spring::Spring(float tauSeconds) noexcept { SetTau(tauSeconds); }

void Spring::SetTau(float tauSeconds) noexcept {
  tau_ = (tauSeconds > 1e-4f) ? tauSeconds : 1e-4f;
}

float Spring::Tick(float target, float dtSeconds) noexcept {
  if (dtSeconds <= 0.0f) return value_;
  const float k = 1.0f - std::exp(-dtSeconds / tau_);
  value_ += (target - value_) * k;
  return value_;
}

void Spring::Reset(float value) noexcept { value_ = value; }

}  // namespace character
```

## Properties to rely on (also asserted in tests)

1. **No overshoot:** for fixed target T, if `value_ < T` then `value_` never
   becomes `> T` (and vice versa). This follows from `k ∈ (0,1)` for dt>0.
2. **Time-step independence:** `Tick` at `dt/2` twice ≈ `Tick` at `dt` once.
3. **Convergence:** after time `t`, remaining distance is multiplied by
   `exp(-t/τ)`; the reach to within 1 % takes ≈ `4.6·τ` seconds.
4. **Damping constant:** `tau_ = 0.06 s` for eyes (fast), `0.12 s` for head
   (slower, more deliberate) — see [config.h.md](../config.h.md).

## Tuning guidance

| τ | Feel |
|---|---|
| 0.03 s | snappy, almost immediate |
| 0.06 s | soft but responsive (eyes — default) |
| 0.12 s | slow, heavy, deliberate (head — default) |
| 0.25 s | sluggish (use for "tired" moods later) |

## Performance / memory

- Two float operations + one `expf` per tick per spring.
- Controller runs ~4 springs at 30 Hz → ~120 `expf`/s. Negligible.
- No allocation; the object is 8 bytes.

## Testing

Covered by `tests/test_animator.cpp`:
- convergence to within epsilon after enough ticks,
- monotonic approach / no overshoot,
- frame-rate independence (dt/2 twice vs dt once),
- `dt ≤ 0` no-op,
- `Reset` snaps value.

## Related

- Header: [Animator.h.md](Animator.h.md)
- Consumer: [Controller.h.md](Controller.h.md)
- Test: [test_animator.cpp.md](../test_animator.cpp.md)
