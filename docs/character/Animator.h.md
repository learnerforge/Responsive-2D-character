# src/character/Animator.h — Spring / Easing Contract

## File role

Declares the smoothing primitive used for all continuous character motion
(eyes, head, saccades). Implements a frame-rate-independent, critically-damped
exponential approach: `value += (target - value) * (1 - exp(-dt / τ))`.

## Public API

```cpp
#pragma once

namespace character {

// Critically-damped exponential follower. Converges toward `target` without
// overshoot. Time constant τ controls responsiveness (small = fast).
class Spring {
public:
  explicit Spring(float tauSeconds) noexcept;   // τ > 0 required
  Spring() noexcept = default;

  // Advance one step. dt in seconds (float). Returns current value.
  float Tick(float target, float dtSeconds) noexcept;

  void  Reset(float value) noexcept;            // snap to value (e.g. at spawn)
  float Value() const noexcept { return value_; }
  float Tau() const noexcept { return tau_; }
  void  SetTau(float tauSeconds) noexcept;

private:
  float tau_  = 0.06f;   // matches cfg::kEyeSpringTau by default
  float value_ = 0.0f;
};

}  // namespace character
```

## Contract / invariants

- `Tick` is monotonic in approach: given fixed target, repeated calls move
  `value_` strictly toward `target` and **never overshoot** it.
- Frame-rate independent: calling with small dt twice yields the same result as
  once with doubled dt (up to floating-point error). `dt ≤ 0` is a no-op.
- `tau_ > 0` must hold; `SetTau` clamps to a small positive epsilon.
- All methods `noexcept`; the class allocates nothing.

## Why this formulation

- `1 - exp(-dt/τ)` approaches 1 as dt grows — no time-step instability, no
  clamping, no per-step constants that need retuning when the tick rate changes.
- Critically-damped = no bounce; eyes glide to the cursor, they never overshoot
  and wiggle (spring-of-springs with κ would add oscillation for "curiosity"
  later).

## Callers

- `Controller.cpp` — five `Spring`s: pupil X, pupil Y, head tilt, glance X,
  glance Y. (See [Controller.h.md](Controller.h.md).)
- `tests/test_animator.cpp` — directly.

## Design decisions

- Header-only declaration; the definition lives in
  [Animator.cpp.md](Animator.cpp.md) (a tiny out-of-line function that is easy
  to unit-test against the real translation unit).
- Value semantics, no vtable, no heap — a `Spring` is a `float` pair in memory.
