# src/character/Controller.cpp — Tick / State Machine Implementation

## File role

Implements `Controller`: the 30 Hz simulation that turns raw cursor positions
into the `DrawState` consumed by the renderer.

## Includes

```cpp
#include "character/Controller.h"
#include "util/WinUtil.h"          // util::SanitizeDt
#include <algorithm>               // std::clamp, std::min
#include <cmath>                   // atan2f, cosf, sinf, hypotf
#include <cstdlib>                 // rand()
```

## Overview of members (mirrors the header)

```
cursor source ──► UpdatePupilTarget ──► targets ──► springs ──► draw_.pupilX/Y, headTilt
timers        ──► UpdateBlink / UpdateSaccade / UpdateBreath / UpdateInteraction
state         ──► UpdateTrackingState
              ──► dirty_ flag → App repaints
```

## Tick

```cpp
void Controller::Tick(long long nowMs) {
  if (lastTickMs_ == 0) { lastTickMs_ = nowMs; return; }   // first call: init
  const float dt = util::SanitizeDt(float(nowMs - lastTickMs_) / 1000.0f);
  lastTickMs_ = nowMs;

  // Consume the left-click flag: the only message that changes state directly.
  if (pendingLeftClick_) {
    pendingLeftClick_ = false;
    state_ = CharState::Interacting;
    interactionUntilMs_ = nowMs + cfg::kInteractionDurationMs;
    dirty_ = true;
  }

  int cx = 0, cy = 0;
  bool hasCursor = cursor_ && cursor_->Get(cx, cy);

  UpdateTrackingState(nowMs);
  if (hasCursor) UpdatePupilTarget(cx, cy);
  UpdateBlink(nowMs);
  UpdateSaccade(nowMs);
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
```

> `dt` is sanitized so a debugger break or sleep/wake (huge `nowMs` gap) cannot
> make springs jump — `SanitizeDt` clamps out-of-range deltas to a safe 16 ms.

## UpdateTrackingState

```
- if cursor available and moved > kCursorDeadZonePx from lastCursor:
    state_ = Tracking; idleSinceMs_ = nowMs;
- else if state_ == Tracking and (nowMs - idleSinceMs_) >= kIdleTimeoutMs:
    state_ = Idle;
- lastCursorX_/Y_ = cursor (if available)
```
When the cursor is unavailable (`Get` false), keep the previous state — do not
spam transitions.

## UpdatePupilTarget

```cpp
const float lx = float(cursorX - windowCx_);   // character-local, screen space
const float ly = float(cursorY - windowCy_);
const float theta = atan2f(ly, lx);
const float r = std::min(cfg::kMaxEyeOffset, hypotf(lx, ly));   // cap magnitude
pupilTargetX_ = cosf(theta) * r;
pupilTargetY_ = sinf(theta) * r;
headTiltTarget_ = std::clamp(theta * cfg::kHeadTiltAngleScale,
                             -cfg::kHeadMaxTilt, cfg::kHeadMaxTilt);
```

### Clamping rationale
- Pupil target magnitude capped at `kMaxEyeOffset` (5 px) → pupils never leave
  the eyeball.
- `hypot` capped so a cursor 500 px away gives the same pupil offset as 100 px
  (direction only) — eyes point, they don't stretch.
- Head tilt scales with angle (`kHeadTiltAngleScale = 0.15`) but clamps to
  ±0.30 rad so the head never looks deformed.

## UpdateBlink

```
- if blinkPhaseMs_ == 0:          // not currently blinking
    if nowMs >= nextBlinkMs_:     // time for a blink
        blinkPhaseMs_ = kBlinkDurationMs;      // 110 ms
        nextBlinkMs_ = nowMs + rand() in [kBlinkMinIntervalMs, kBlinkMaxIntervalMs]
- else:                           // mid-blink
    blinkPhaseMs_ -= dtMs;
    t = 1 - (blinkPhaseMs_ / kBlinkDurationMs)            // 0→1 over the window
    draw_.blink = (t < 0.5f) ? (1 - 2t) : (2t - 1)       // open→close→open
    if blinkPhaseMs_ <= 0: blinkPhaseMs_ = 0; draw_.blink = 1
```
`draw_.blink` = 1 fully open, 0 fully closed, triangle wave. Blinking stops
`draw_.blink` from hitting a flat line, giving natural closure.

## UpdateSaccade

Only when `state_ == Idle`:
```
- if saccadePhaseMs_ == 0 and nowMs >= nextSaccadeMs_:
    pick random glance angle in [−60°, +60°] around horizontal,
    magnitude random in [2, 5] px; saccadePhaseMs_ = 220;
    nextSaccadeMs_ = nowMs + random in [kSaccadeMinIntervalMs, kSaccadeMaxIntervalMs]
- if saccadePhaseMs_ > 0:
    advance; glance targets ramp in then out (spring handles smoothing);
    on expiry reset glanceTargetX_/Y_ to 0
```
Glance targets feed `glanceX_/glanceY_` springs; the rendered pupil position is
`pupil + glance` (added in `Character.cpp`).

## UpdateBreath

```cpp
breathPhase_ += dt * (2π / cfg::kBreathPeriodS);
draw_.breath = sinf(breathPhase_) * cfg::kBreathAmplitudePx;
```
Always runs (Idle, Tracking, Interacting) — the character is "alive".

## UpdateInteraction

```
if state_ != Interacting: draw_.hopY = 0; draw_.surprised = false; return;
t = (nowMs - (interactionUntilMs_ - kInteractionDurationMs)) / float(kInteractionDurationMs);
if t >= 1:                              // done
    state_ = CharState::Idle;           // deterministic exit; the next cursor
    draw_.hopY = 0; draw_.surprised = false;   // move re-enters Tracking
else:
    draw_.hopY  = kHopHeightPx * 4 * t * (1 - t);   // parabola, peak mid-way
    draw_.surprised = true;
```

> Exiting to `Idle` (rather than re-checking the cursor) keeps the state machine
> deterministic and trivially testable; `UpdateTrackingState` promotes back to
> `Tracking` on the next meaningful cursor move.

## DecideDirty

Compares `draw_` against the previous frame's values with thresholds:
| Value | Threshold |
|---|---|
| pupilX/Y or glanceX/Y | > 0.5 px |
| blink | > 0.02 |
| headTilt | > 0.005 rad |
| breath | > 0.05 px |
| hopY | > 0.1 px |
| surprised / state change | any change |

Sets `dirty_ = true` if exceeded; otherwise leaves the previous dirty state as
is (App calls `ClearRepaint()` after painting).

## Threading / determinism

- Single-threaded, UI thread only (see [architecture.md](../architecture.md)).
- `rand()`-based randomness is fine (not security-relevant). For reproducibility
  in tests, `SetCursorSource` + fixed clock values are used; randomness is
  isolated so tests can force next-blink windows.

## Memory / performance

- No allocations in `Tick`; fixed-size structs; 5 `Spring`s = 40 bytes.
- 30 Hz × ~5 `expf` = ~150 `expf`/s — negligible (< 0.1 % CPU).

## Edge cases

| Case | Behavior |
|---|---|
| First `Tick` (lastTickMs_==0) | initialize, do nothing else |
| Cursor `Get` returns false | skip pupil update; keep last state/targets |
| `dt` large (sleep/wake, debugger break) | `SanitizeDt` clamps; timers use absolute `nowMs`, so no jumps |
| Left-click during Interacting | restarts the interaction timer (fine) |
| Window not yet anchored (center=0,0) | eyes look toward top-left until `SetWindowCenter`; App always anchors before starting ticks |

> Right-click is **not** the controller's concern: the overlay delivers it to
> `App::OnRightClick`, which shows the modal menu directly
> ([App.cpp.md](../App.cpp.md)).

## Related

- Header: [Controller.h.md](Controller.h.md)
- Springs: [Animator.h.md](Animator.h.md) / [Animator.cpp.md](Animator.cpp.md)
- Config: [config.h.md](../config.h.md)
- Test: [test_controller.cpp.md](../test_controller.cpp.md)
