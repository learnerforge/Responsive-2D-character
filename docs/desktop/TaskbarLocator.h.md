# src/desktop/TaskbarLocator.h — Taskbar Geometry Contract

## File role

Declares the types that locate the Windows taskbar and compute where the pet
window should dock. Encapsulates all `SHAppBarMessage` / monitor work-area
logic so `App` and `OverlayWindow` never touch it directly.

## Public API

```cpp
#pragma once
#include <windows.h>

namespace desktop {

enum class TaskbarEdge { Left, Top, Right, Bottom, Unknown };

struct TaskbarInfo {
  TaskbarEdge edge = TaskbarEdge::Unknown;  // which screen edge the taskbar is on
  RECT        rect{};                       // taskbar bounding rect (screen coords)
  bool        autoHidden = false;           // true if taskbar auto-hides
  bool        visible    = true;            // false if taskbar cannot be read at all
};

// Where the character (size w×h logical px, DPI-scaled) should be placed.
struct DockRect {
  int x = 0, y = 0, w = 0, h = 0;           // screen coords, physical px
};

class TaskbarLocator {
public:
  TaskbarLocator() = default;

  // Query the taskbar via SHAppBarMessage. Never throws; on any failure returns
  // TaskbarInfo with visible=false (caller falls back to the work area).
  TaskbarInfo Locate() const;

  // The work area (screen minus taskbar) for the monitor containing the taskbar.
  // Fallback used when Locate() fails or when the taskbar is auto-hidden.
  RECT WorkArea() const;

  // Compute the anchored window rect for a bottom-edge taskbar.
  // characterW/H are DPI-scaled physical pixel dimensions.
  DockRect DockBottom(const TaskbarInfo& info, int characterW, int characterH) const;
};

}  // namespace desktop
```

## Contract / invariants

- All outputs are in **virtual screen coordinates**, physical pixels.
- `Locate()` is const and noexcept-safe (catches internal errors); it must never
  throw and never allocate.
- `DockBottom` returns a deterministic rect given the same inputs.
- v1 supports a **bottom-edge taskbar** (the overwhelmingly common layout).
  `edge == Top/Left/Right` is detected but falls back to the same
  bottom-docking math, documented as a v1.1 extension.

## Geometry policy (why this shape)

- The character floats **above** the taskbar top edge by `kAnchorGap` (2 px),
  resting its feet on the taskbar surface — never overlapping the taskbar's
  Start button / pinned icons / system tray.
- Horizontal position: fixed `kAnchorX = 64 px` from the left of the taskbar
  monitor, clear of the Start button (~48 px) — avoids the tray whose width is
  unknowable without extra queries.
- When the taskbar is **auto-hidden**, `Locate().autoHidden == true` and the
  caller docks to the bottom of the work area instead (feet at screen bottom).

## Callers

- `App.cpp` — after startup, on `WM_DISPLAYCHANGE`/`WM_SETTINGCHANGE`, and on
  `WM_DPICHANGED`, to re-anchor the window.
- No other module needs taskbar geometry.

## Design decisions

- Header-only declarations; implementation in
  [TaskbarLocator.cpp.md](TaskbarLocator.cpp.md).
- No HWND stored — stateless queries keep the class trivially testable.
- Structure fields instead of getters: value-type data, no virtuals.
