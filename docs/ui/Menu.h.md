# src/ui/Menu.h — Context Menu Contract

## File role

Declares the small Win32 popup-menu helper used for the character's
right-click menu. Wraps `CreatePopupMenu` + `TrackPopupMenu` so `App` only sees
a clean `Choice` enum.

## Public API

```cpp
#pragma once
#include <windows.h>

namespace ui {

enum class MenuChoice {
  None,          // dismissed / no selection
  Animate,       // manually trigger the hop reaction
  ToggleAutostart,
  Settings,      // placeholder dialog in v1
  Quit,
};

class Menu {
public:
  Menu() = default;

  // Show the popup anchored at (screenX, screenY). Blocks until the menu is
  // dismissed (TrackPopupMenu is modal). Returns the selected choice.
  MenuChoice Show(HWND owner, int screenX, int screenY);

  // Whether the autostart entry currently shows a checkmark. Set by App before
  // Show() (queried from Autostart::IsEnabled()).
  void SetAutostartChecked(bool checked) noexcept;

private:
  HMENU Build();
  bool autostartChecked_ = false;
};

}  // namespace ui
```

## Contract / invariants

- `Show` runs on the UI thread and **blocks** while the menu is open
  (`TrackPopupMenu` with `TPM_RETURNCMD`). This is acceptable: the tick timer
  still fires during the modal menu loop, so idle animation continues (see
  notes below).
- `SetAutostartChecked` is called by `App` immediately before `Show` so the
  checkbox reflects the current registry state.
- The menu is destroyed by `DestroyMenu` after each `Show` — no leak, no
  lingering GDI user objects.
- `MenuChoice::Settings` opens a placeholder dialog in v1 (a message box with a
  note), documented as a v2 target.

## Why `TrackPopupMenu` (modal) instead of non-modal

- Zero extra window classes; the existing message loop (with `WM_TIMER`)
  continues to run because `TrackPopupMenu` runs its own modal loop that
  dispatches messages — including our `WM_TIMER`, so the character keeps
  blinking/breathing under the menu.
- Simplest possible code: one call returns the chosen command id.

## Callers

- `App.cpp` — right-click callback → `menu_.Show(...)` → act on choice.

## Design decisions

- No icons, no sub-menus in v1 — keeps working-set impact negligible
  (a popup menu allocates ~1–2 KB).
- `HWND owner` is our own window handle; passing `TPM_RETURNCMD` returns the
  command id so we don't need a `WM_COMMAND` handler.
