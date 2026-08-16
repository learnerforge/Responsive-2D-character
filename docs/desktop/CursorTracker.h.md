# src/desktop/CursorTracker.h — Cursor Query Contract

## File role

Declares the only type that reads the global mouse cursor position. A thin,
statically-testable wrapper around Win32 `GetCursorPos`.

## Public API

```cpp
#pragma once
#include <windows.h>   // POINT

namespace desktop {

class CursorTracker {
public:
  CursorTracker() = default;

  // Returns true and writes current global cursor coordinates into (x, y).
  // Returns false if the position could not be read (rare: session change,
  // input desktop switch, UAC secure desktop, remote desktop quirks).
  bool GetPosition(int& x, int& y) const noexcept;
};

}  // namespace desktop
```

## Contract / invariants

- **Output space:** *virtual screen coordinates* (primary-monitor top-left is
  (0,0), `+y` grows downward). Matches the space used by `TaskbarLocator`
  and `OverlayWindow`.
- `GetPosition` is `const`, noexcept, cheap, and **never blocks**.
- It may be called up to 30×/s (see Controller). Keep the implementation a bare
  `GetCursorPos` call — no caching, no allocations (see
  [memory-budget.md](../memory-budget.md)).
- Single-threaded use only (see [architecture.md](../architecture.md)).

## Callers

- `App.cpp` — once during setup, wires it into the controller as the
  `ICursorSource` via the `CursorSourceAdapter` in [App.h.md](../App.h.md).
  Every tick the controller calls `GetPosition()` through that adapter.
- Tests inject a fake source through `ICursorSource` (see
  [Controller.h.md](../character/Controller.h.md)); `CursorTracker` itself is
  not test-instrumented.

## Failure semantics

| Case | Result |
|---|---|
| Normal desktop, cursor on any monitor | true, valid coords |
| During UAC prompt / secure desktop | false (caller keeps last known target) |
| Session locked / switch | false |

Caller policy on `false`: **ignore the sample** (keep previous look targets),
continue idle timers. Never assert or crash.

## Design decisions

- Returns coords by reference instead of a struct to keep the call allocation
  free.
- No direct dependency on the window or HWND — the cursor is a global resource.
- Header-only *declaration*; implementation is in
  [CursorTracker.cpp.md](CursorTracker.cpp.md).

## Memory notes

- Adds nothing to working set (no fields of consequence, no heap).
