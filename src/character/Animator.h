#pragma once

namespace character {

// Critically-damped exponential follower. Converges toward `target` without
// overshoot. Time constant tau controls responsiveness (small = fast).
class Spring {
public:
  explicit Spring(float tauSeconds) noexcept;   // tau > 0 required
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
