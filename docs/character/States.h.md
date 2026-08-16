# src/character/States.h — State Enum

## File role

Defines the character's behavioral state machine enum. Header-only, no
implementation file, no dependencies.

## Content

```cpp
#pragma once

namespace character {

enum class CharState {
  Idle,        // cursor still: blink, breathe, glance around
  Tracking,    // cursor moving: eyes + head follow the cursor
  Interacting, // reaction after a click: hop + surprised face (timer-bound)
};

// Debug / logging helper. Returns a stable short string.
inline const char* ToString(CharState s) noexcept {
  switch (s) {
    case CharState::Idle:        return "Idle";
    case CharState::Tracking:    return "Tracking";
    case CharState::Interacting: return "Interacting";
  }
  return "Unknown";
}

}  // namespace character
```

## Transition table (owned by Controller)

| From | To | Trigger | Where |
|---|---|---|---|
| Idle | Tracking | cursor displacement > `kCursorDeadZonePx` | [Controller.cpp.md](Controller.cpp.md) `UpdateTrackingState` |
| Tracking | Idle | cursor still for `kIdleTimeoutMs` (3 s) | same |
| Idle/Tracking | Interacting | left-click (`OnLeftClick`) | same |
| Interacting | Idle | `kInteractionDurationMs` (1.2 s) elapsed | same (`UpdateInteraction`) |

## Invariants

- Exactly one state at any time.
- Transitions happen **only** inside `Controller::Tick` (and the click callbacks
  which set a pending flag consumed at next tick). No other module may change
  the state.
- `Interacting` is always bounded by a timer; the controller never remains in it
  indefinitely.

## Rendering influence

- `Idle` → saccade glances allowed; blink/breath always run.
- `Tracking` → suppress saccades (eyes busy following).
- `Interacting` → `surprised=true` + hop offset (see
  [Controller.cpp.md](Controller.cpp.md) and
  [Character.cpp.md](Character.cpp.md)).

## Memory notes

- Enum: one byte. `ToString`: static const char* — no allocation.
