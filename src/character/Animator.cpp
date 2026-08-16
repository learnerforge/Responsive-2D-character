#include "character/Animator.h"
#include <algorithm>
#include <cmath>

namespace character {

Spring::Spring(float tauSeconds) noexcept { SetTau(tauSeconds); }

void Spring::SetTau(float tauSeconds) noexcept {
  if (tauSeconds > 0.0f) tau_ = tauSeconds;
}

void Spring::Reset(float value) noexcept { value_ = value; }

float Spring::Tick(float target, float dtSeconds) noexcept {
  if (dtSeconds <= 0.0f) return value_;          // non-positive dt: no-op
  // Critically damped: 1 - exp(-dt/tau). Guards: tau clamped by SetTau.
  const float k = 1.0f - std::exp(-dtSeconds / tau_);
  value_ += (target - value_) * k;
  return value_;
}

}  // namespace character
