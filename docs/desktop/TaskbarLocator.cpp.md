# src/desktop/TaskbarLocator.cpp — Taskbar Geometry Implementation

## File role

Implements `TaskbarLocator` using the shell appbar API and monitor work areas.

## Implementation outline

```cpp
#include "desktop/TaskbarLocator.h"
#include <shellapi.h>
#include <windowsx.h>      // GET_X_LPARAM etc. (if used)

namespace desktop {

TaskbarInfo TaskbarLocator::Locate() const {
  TaskbarInfo out{};
  APPBARDATA abd{};
  abd.cbSize = sizeof(abd);

  // Current edge + rect.
  if (::SHAppBarMessage(ABM_GETTASKBARPOS, &abd) == 0) {
    out.visible = false;
    return out;                      // fall back to work area
  }
  switch (abd.uEdge) {
    case ABE_LEFT:   out.edge = TaskbarEdge::Left;   break;
    case ABE_TOP:    out.edge = TaskbarEdge::Top;    break;
    case ABE_RIGHT:  out.edge = TaskbarEdge::Right;  break;
    case ABE_BOTTOM: out.edge = TaskbarEdge::Bottom; break;
    default:         out.edge = TaskbarEdge::Unknown;
  }
  out.rect = abd.rc;

  // Auto-hide state.
  APPBARDATA state{};
  state.cbSize = sizeof(state);
  if (::SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE)
    out.autoHidden = true;

  return out;
}

RECT TaskbarLocator::WorkArea() const {
  RECT wa{};
  ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);   // primary monitor
  return wa;
}

DockRect TaskbarLocator::DockBottom(const TaskbarInfo& info,
                                    int charW, int charH) const {
  DockRect d;
  d.w = charW; d.h = charH;
  RECT wa = WorkArea();

  if (info.visible && info.edge == TaskbarEdge::Bottom && !info.autoHidden) {
    // Feet on the taskbar top edge.
    d.x = wa.left + kAnchorX_;              // 64 px from work-area left
    d.y = info.rect.top - charH - kAnchorGap_;   // 2 px above the taskbar
  } else {
    // Auto-hidden / unknown: sit at the bottom of the work area.
    d.x = wa.left + kAnchorX_;
    d.y = wa.bottom - charH - kAnchorGap_;
  }
  return d;
}

}  // namespace desktop
```

## API notes

- `SHAppBarMessage(ABM_GETTASKBARPOS)` fills `abd.rc` (taskbar rect) and
  `abd.uEdge` (edge). Requires `shell32` (linked in
  [CMakeLists.txt.md](../CMakeLists.txt.md)).
- `ABM_GETSTATE` returns `ABS_AUTOHIDE` when the taskbar auto-hides; in that
  state `abd.rc` is typically **offscreen** (negative or beyond screen), so the
  auto-hidden branch uses the work area instead.
- `SPI_GETWORKAREA` returns the primary-monitor work area. v1 targets the
  primary monitor (taskbar usually lives there). Multi-monitor extension is a
  known follow-up (see [overview.md](../overview.md) non-goals).

## Constants

The dock constants live in `config.h` (`cfg::kAnchorX`, `cfg::kAnchorGap`);
the `kAnchorX_`/`kAnchorGap_` members above are shorthand for `cfg::…`.

## Failure / edge handling

| Scenario | Behavior |
|---|---|
| `ABM_GETTASKBARPOS` fails | `visible=false`; `DockBottom` uses work area bottom |
| Taskbar auto-hidden | `autoHidden=true`; dock to work-area bottom |
| Taskbar on left/right/top | detected, but v1 docks as if bottom (documented limitation) |
| Multiple monitors | uses primary work area (v1) |
| DPI change / display change | caller re-invokes `Locate`+`DockBottom` (see [App.cpp.md](../App.cpp.md)) |

## Performance / memory

- A few `SHAppBarMessage`/`SystemParametersInfo` calls only on startup or on
  system events — never per tick. Negligible cost.
- No allocation, no fields → no working-set impact.

## Testing

- `test_controller` does not need the locator (controller takes cursor only).
- Manual smoke (see [overview.md](../overview.md)): move taskbar to each edge,
  toggle auto-hide, change resolution — character must re-dock correctly.

## Related

- Header: [TaskbarLocator.h.md](TaskbarLocator.h.md)
- Consumer: [App.cpp.md](../App.cpp.md)
- Config: [config.h.md](../config.h.md)
