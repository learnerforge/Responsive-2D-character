# src/desktop/CursorTracker.cpp — Cursor Query Implementation

## File role

Implements `CursorTracker::GetPosition` using the Win32 `user32.GetCursorPos`
API.

## Implementation

```cpp
#include "desktop/CursorTracker.h"

namespace desktop {

bool CursorTracker::GetPosition(int& x, int& y) const noexcept {
  POINT pt{};
  if (!::GetCursorPos(&pt)) return false;
  x = pt.x;
  y = pt.y;
  return true;
}

}  // namespace desktop
```

## Why `GetCursorPos`

- Returns **global** (virtual screen) coordinates regardless of which window
  has focus or where the cursor is — exactly what a taskbar pet needs.
- Synchronous, non-blocking, no callbacks, no hooks. Hooks
  (`SetWindowsHookEx(WH_MOUSE_LL)`) would give event-driven updates but add a
  system-wide hook (heavier, slower to install) — unnecessary when we only need
  30 Hz samples. Polling is the right trade for a 5 MB budget.
- Requires no special privileges.

## Contract details

- On success: `x`, `y` are valid virtual-screen coords; returns `true`.
- On failure (`GetCursorPos` returns 0): returns `false` and leaves `x`, `y`
  untouched. Typical causes: process is on a different desktop (secure desktop,
  UAC), or the input desktop is switching.
- `noexcept`: never throws. GDI failures surface as `false`, never exceptions.

## Performance / memory

- Single API call, no allocations, no syscall caching — ~microseconds.
- Called at 30 Hz (33 ms period). At this rate its cost is negligible
  (≈0.03 % CPU).
- Adds no working set beyond the object's (empty) footprint.

## Edge cases

| Scenario | Behavior |
|---|---|
| Cursor on secondary monitor | returns its virtual-screen coords (negative x possible if monitor is left of primary) — Controller clamps targets, see [Controller.cpp.md](../character/Controller.cpp.md) |
| Cursor during secure desktop (UAC) | `false` → sample skipped |
| Extreme coordinates (> monitor) during transition | returned as-is; Controller clamps |

## Testing

Not unit-tested directly (real Win32 call). Covered indirectly by
`test_controller.cpp` which injects a fake `ICursorSource`. Manual smoke: move
cursor across both monitors; character must follow.

## Related

- Header: [CursorTracker.h.md](CursorTracker.h.md)
- Consumer: [Controller.h.md](../character/Controller.h.md),
  [App.cpp.md](../App.cpp.md)
