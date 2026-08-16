#pragma once
#include "character/Animator.h"
#include "character/States.h"
#include "config.h"                 // cfg tokens used by defaults and Tick

namespace character {

// All values that Character.cpp needs to draw one frame.
struct DrawState {
  float pupilX = 0, pupilY = 0;   // pupil offset from eye center (px)
  float headTilt = 0;             // radians, tilt of the head
  float blink = 1.0f;             // 1 = open, 0 = fully closed
  float breath = 0.0f;            // body scale offset (px, sin phase)
  float hopY = 0.0f;              // vertical offset during interaction (px, up = +)
  float glanceX = 0, glanceY = 0; // saccade glance offset added to pupils (px)
  bool  surprised = false;        // interaction face (big pupils, "o" mouth)
};

// Cursor source abstraction so tests can inject fake positions.
struct ICursorSource {
  virtual ~ICursorSource() = default;
  virtual bool Get(int& x, int& y) = 0;
};

class Controller {
public:
  Controller();

  // Inject a cursor source (must be called before the first Tick; the class
  // stays Win32-free, so App wires the real CursorTracker through the adapter).
  void SetCursorSource(ICursorSource* src) noexcept;

  // Advance the simulation. nowMs = steady clock in milliseconds.
  void Tick(long long nowMs);

  // Interaction entry point (called from OverlayWindow's left-click callback).
  // Sets a flag consumed at the next Tick so the state machine stays the single
  // owner of state transitions. noexcept + non-blocking.
  void OnLeftClick() noexcept;

  // Queries for App.
  CharState State() const noexcept { return state_; }
  const DrawState& Current() const noexcept { return draw_; }
  bool WantsRepaint() const noexcept { return dirty_; }
  void ClearRepaint() noexcept { dirty_ = false; }

  // Window center in screen coords; set by App when re-anchoring.
  void SetWindowCenter(int cx, int cy) noexcept;

private:
  void UpdateTrackingState(long long nowMs);
  void UpdateBlink(long long nowMs, int dtMs);
  void UpdateSaccade(long long nowMs, int dtMs);
  void UpdateBreath(float dt);
  void UpdateInteraction(long long nowMs);
  void UpdatePupilTarget(int cursorX, int cursorY);
  void DecideDirty();

  ICursorSource* cursor_ = nullptr;      // injected; owns nothing
  CharState state_ = CharState::Idle;
  DrawState draw_{};
  bool dirty_ = false;
  bool pendingLeftClick_ = false;

  long long lastTickMs_ = 0;
  bool initialized_ = false;           // set on the first Tick (not the ms value,
                                       // which may legitimately be 0 in tests)
  int windowCx_ = 0, windowCy_ = 0;      // screen center of the character

  Spring pupilX_{0.06f}, pupilY_{0.06f}; // τ = cfg::kEyeSpringTau
  Spring headTilt_{0.12f};               // τ = cfg::kHeadSpringTau
  Spring glanceX_{0.20f}, glanceY_{0.20f};

  // Desired values computed each tick; the springs ease toward these.
  float pupilTargetX_ = 0, pupilTargetY_ = 0;
  float headTiltTarget_ = 0;
  float glanceTargetX_ = 0, glanceTargetY_ = 0;

  float breathPhase_ = 0.0f;
  long long nextBlinkMs_ = 0;
  int blinkPhaseMs_ = 0;                 // remaining blink window, 0 = none
  long long idleSinceMs_ = 0;
  long long nextSaccadeMs_ = 0;
  int saccadePhaseMs_ = 0;
  long long interactionUntilMs_ = 0;
  int lastCursorX_ = 0, lastCursorY_ = 0;
  bool haveCursor_ = false;
  DrawState prevDraw_{};             // last painted frame for the dirty check
  CharState lastDirtyState_ = CharState::Idle;
};

}  // namespace character
