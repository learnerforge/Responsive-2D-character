# src/character/Controller.h — Tick / State Machine Contract

## File role

Declares the heart of the app: samples the cursor, decides the state, advances
the animation values (pupils, head tilt, blink, breath, hop), and reports a
dirty flag so `App` knows whether to repaint. **Pure logic — no drawing, no
Win32.**

## Public API

```cpp
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
  void UpdateBlink(long long nowMs);
  void UpdateSaccade(long long nowMs);
  void UpdateBreath(float dt);
  void UpdateInteraction(long long nowMs);
  void UpdatePupilTarget(int cursorX, int cursorY);

  ICursorSource* cursor_ = nullptr;      // injected; owns nothing
  CharState state_ = CharState::Idle;
  DrawState draw_{};
  bool dirty_ = false;
  bool pendingLeftClick_ = false;

  long long lastTickMs_ = 0;
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
};

}  // namespace character
```

## Contract / invariants

- `Tick` must be called at a roughly steady rate (~30 Hz) from the UI thread
  only. It reads the cursor, advances springs and timers, and sets `dirty_`
  when the produced frame would differ measurably from the last.
- `OnLeftClick` is cheap, noexcept, and **non-blocking**; it only sets a flag.
  The state transition happens at the next `Tick`. The right-click menu is
  handled entirely by `App`/`Menu` — the controller is not involved.
- `Current()` returns a reference to the frame state; `App` copies it into the
  renderer after `ClearRepaint()`.
- `SetWindowCenter` is called by `App` whenever the window is re-anchored; the
  controller converts screen cursor coords into character-local space using it.
- No allocation in `Tick`; no dynamic memory at all in this class.

## Data flow (per tick)

```
Tick(nowMs)
 ├─ dt = nowMs - lastTickMs_
 ├─ UpdateTrackingState: dead-zone + idle timeout → Idle/Tracking
 ├─ UpdatePupilTarget: (cursor - windowCenter) → atan2 → pupil/head targets
 ├─ UpdateBlink: schedule + run blink window → draw_.blink
 ├─ UpdateSaccade: when Idle, random glance → draw_.glanceX/Y
 ├─ UpdateBreath: advance phase → draw_.breath
 ├─ UpdateInteraction: hop parabola → draw_.hopY + draw_.surprised
 ├─ springs Tick(target, dt)  → draw_.pupilX/Y, headTilt, glanceX/Y
 ├─ decide dirty_ (see below)
 └─ consume pendingLeftClick_ → start interaction
```

## Dirty-flag policy

`dirty_` is set when any of the following changed beyond threshold since last
tick: pupil/glance moved > 0.5 px, blink changed > 0.02, headTilt > 0.005 rad,
breath > 0.05 px, hopY changed, state changed, or a new blink started/ended.
This is the CPU/memory saver behind the <1 % idle target
([memory-budget.md](../memory-budget.md)).

## Testing

`tests/test_controller.cpp` injects a fake `ICursorSource` and asserts:
- dead-zone: tiny moves don't switch to Tracking,
- Idle→Tracking→Idle timing,
- click → Interacting → returns after 1.2 s,
- blink scheduling falls in [min,max] interval,
- `WantsRepaint` true on meaningful changes, false on no-change ticks.
