#include "character/Controller.h"
#include "util/WinUtil.h"          // util::SanitizeDt
#include <algorithm>               // std::clamp, std::min, std::max
#include <cmath>                   // atan2f, cosf, sinf, hypotf
#include <cstdlib>                 // rand()

namespace character {

Controller::Controller() = default;

void Controller::SetCursorSource(ICursorSource* src) noexcept {
  cursor_ = src;
}

void Controller::SetWindowCenter(int cx, int cy) noexcept {
  windowCx_ = cx;
  windowCy_ = cy;
}

void Controller::OnLeftClick() noexcept {
  pendingLeftClick_ = true;
}

// Random integer in [lo, hi] (both inclusive). Local so tests can influence
// scheduling purely via the clock rather than global seeding.
static int RandInt(int lo, int hi) noexcept {
  return lo + rand() % (hi - lo + 1);          // NOLINT(cert-msc50-cpp)
}

void Controller::Tick(long long nowMs) {
  if (!initialized_) {
    // First call: establish the dead-zone baseline (cursor position at
    // startup) and schedule the first blink/saccade AFTER a full random
    // interval, so the pet does not react instantly at t=0.
    initialized_ = true;
    lastTickMs_ = nowMs;
    int cx = 0, cy = 0;
    if (cursor_ && cursor_->Get(cx, cy)) { lastCursorX_ = cx; lastCursorY_ = cy; }
    nextBlinkMs_   = nowMs + RandInt(cfg::kBlinkMinIntervalMs, cfg::kBlinkMaxIntervalMs);
    nextSaccadeMs_ = nowMs + RandInt(cfg::kSaccadeMinIntervalMs, cfg::kSaccadeMaxIntervalMs);
    return;
  }
  const float dt = util::SanitizeDt(float(nowMs - lastTickMs_) / 1000.0f);
  const int dtMs = static_cast<int>(nowMs - lastTickMs_);
  lastTickMs_ = nowMs;

  // Consume the left-click flag: the only message that changes state directly.
  if (pendingLeftClick_) {
    pendingLeftClick_ = false;
    state_ = CharState::Interacting;
    interactionUntilMs_ = nowMs + cfg::kInteractionDurationMs;
    dirty_ = true;
  }

  int cx = 0, cy = 0;
  const bool hasCursor = cursor_ && cursor_->Get(cx, cy);

  UpdateTrackingState(nowMs);
  if (hasCursor) UpdatePupilTarget(cx, cy);
  UpdateBlink(nowMs, dtMs);
  UpdateSaccade(nowMs, dtMs);
  UpdateBreath(dt);
  UpdateInteraction(nowMs);

  // Advance springs toward the current targets.
  draw_.pupilX   = pupilX_.Tick(pupilTargetX_, dt);
  draw_.pupilY   = pupilY_.Tick(pupilTargetY_, dt);
  draw_.headTilt = headTilt_.Tick(headTiltTarget_, dt);
  draw_.glanceX  = glanceX_.Tick(glanceTargetX_, dt);
  draw_.glanceY  = glanceY_.Tick(glanceTargetY_, dt);

  DecideDirty();
}

void Controller::UpdateTrackingState(long long nowMs) {
  int cx = 0, cy = 0;
  const bool ok = cursor_ && cursor_->Get(cx, cy);
  if (ok) {
    const int dx = cx - lastCursorX_;
    const int dy = cy - lastCursorY_;
    if (dx * dx + dy * dy >=
        static_cast<int>(cfg::kCursorDeadZonePx * cfg::kCursorDeadZonePx)) {
      state_ = CharState::Tracking;
      idleSinceMs_ = nowMs;
    }
    lastCursorX_ = cx;
    lastCursorY_ = cy;
  }

  if (state_ == CharState::Tracking &&
      (nowMs - idleSinceMs_) >= cfg::kIdleTimeoutMs)
    state_ = CharState::Idle;
}

void Controller::UpdatePupilTarget(int cursorX, int cursorY) {
  const float lx = float(cursorX - windowCx_);   // character-local, screen space
  const float ly = float(cursorY - windowCy_);
  const float theta = std::atan2f(ly, lx);
  const float r = std::min(cfg::kMaxEyeOffset, std::hypotf(lx, ly));   // cap magnitude
  pupilTargetX_ = std::cosf(theta) * r;
  pupilTargetY_ = std::sinf(theta) * r;
  headTiltTarget_ = std::clamp(theta * cfg::kHeadTiltAngleScale,
                               -cfg::kHeadMaxTilt, cfg::kHeadMaxTilt);
}

void Controller::UpdateBlink(long long nowMs, int dtMs) {
  if (blinkPhaseMs_ == 0) {          // not currently blinking
    if (nowMs >= nextBlinkMs_) {     // time for a blink
      blinkPhaseMs_ = cfg::kBlinkDurationMs;
      nextBlinkMs_ = nowMs + RandInt(cfg::kBlinkMinIntervalMs,
                                     cfg::kBlinkMaxIntervalMs);
    }
    return;
  }

  // Mid-blink: triangle 1 → 0 → 1 over kBlinkDurationMs.
  blinkPhaseMs_ -= dtMs;
  const float t = 1.0f - float(blinkPhaseMs_) / float(cfg::kBlinkDurationMs);
  draw_.blink = (t < 0.5f) ? (1.0f - 2.0f * t) : (2.0f * t - 1.0f);
  if (blinkPhaseMs_ <= 0) {
    blinkPhaseMs_ = 0;
    draw_.blink = 1.0f;
  }
}

void Controller::UpdateSaccade(long long nowMs, int dtMs) {
  if (state_ != CharState::Idle) {
    glanceTargetX_ = 0;
    glanceTargetY_ = 0;
    return;
  }

  if (saccadePhaseMs_ == 0 && nowMs >= nextSaccadeMs_) {
    const float angle = (RandInt(0, 1000) / 1000.0f - 0.5f) * 2.0f *
                        (60.0f * 3.14159265f / 180.0f);
    const float mag = cfg::kSaccadeMinPx +
        (cfg::kSaccadeMaxPx - cfg::kSaccadeMinPx) *
        (RandInt(0, 1000) / 1000.0f);
    glanceTargetX_ = std::cosf(angle) * mag;
    glanceTargetY_ = std::sinf(angle) * mag;
    saccadePhaseMs_ = cfg::kSaccadeDurationMs;
    nextSaccadeMs_ = nowMs + RandInt(cfg::kSaccadeMinIntervalMs,
                                     cfg::kSaccadeMaxIntervalMs);
  }

  if (saccadePhaseMs_ > 0) {
    saccadePhaseMs_ -= dtMs;
    if (saccadePhaseMs_ <= 0) {
      saccadePhaseMs_ = 0;
      glanceTargetX_ = 0;
      glanceTargetY_ = 0;
    }
  }
}

void Controller::UpdateBreath(float dt) {
  breathPhase_ += dt * (6.2831853f / cfg::kBreathPeriodS);
  draw_.breath = std::sinf(breathPhase_) * cfg::kBreathAmplitudePx;
}

void Controller::UpdateInteraction(long long nowMs) {
  if (state_ != CharState::Interacting) {
    draw_.hopY = 0;
    draw_.surprised = false;
    return;
  }

  const float t = float(nowMs - (interactionUntilMs_ - cfg::kInteractionDurationMs)) /
                  float(cfg::kInteractionDurationMs);
  if (t >= 1.0f) {                 // done
    state_ = CharState::Idle;      // next meaningful cursor move re-enters Tracking
    draw_.hopY = 0;
    draw_.surprised = false;
  } else {
    draw_.hopY = cfg::kHopHeightPx * 4.0f * t * (1.0f - t);   // parabola
    draw_.surprised = true;
  }
}

void Controller::DecideDirty() {
  const auto& p = prevDraw_;
  const auto& d = draw_;
  const float pupil = std::max(std::abs(d.pupilX - p.pupilX),
                               std::abs(d.pupilY - p.pupilY));
  const float glance = std::max(std::abs(d.glanceX - p.glanceX),
                                std::abs(d.glanceY - p.glanceY));
  if (pupil > cfg::kDirtyPupilPx || glance > cfg::kDirtyPupilPx ||
      std::abs(d.blink - p.blink) > cfg::kDirtyBlink ||
      std::abs(d.headTilt - p.headTilt) > cfg::kDirtyHeadRad ||
      std::abs(d.breath - p.breath) > cfg::kDirtyBreathPx ||
      std::abs(d.hopY - p.hopY) > cfg::kDirtyHopPx ||
      d.surprised != p.surprised ||
      state_ != lastDirtyState_)
    dirty_ = true;

  lastDirtyState_ = state_;
  prevDraw_ = d;
}

}  // namespace character
