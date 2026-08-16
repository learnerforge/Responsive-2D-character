# src/App.h — Application Class Contract

## File role

Declares the top-level application object: the message loop, the window proc
delegation, the tick, re-anchoring, and the ownership of every subsystem. This
is the *only* aggregator in the codebase (see
[architecture.md](architecture.md)).

## Public API

```cpp
#pragma once
#include "desktop/OverlayWindow.h"
#include "desktop/CursorTracker.h"
#include "desktop/TaskbarLocator.h"
#include "character/Controller.h"
#include "character/Character.h"
#include "character/Renderer.h"
#include "ui/Menu.h"

// Bridge: presents desktop::CursorTracker to character::ICursorSource. Keeps the
// layering clean (desktop never depends on character).
struct CursorSourceAdapter : character::ICursorSource {
  desktop::CursorTracker& tracker;
  explicit CursorSourceAdapter(desktop::CursorTracker& t) noexcept : tracker(t) {}
  bool Get(int& x, int& y) override { return tracker.GetPosition(x, y); }
};

class App {
public:
  explicit App(HINSTANCE hInstance);
  ~App();

  // Runs the message loop until WM_QUIT. Returns the exit code.
  int Run();

private:
  bool CreateWindow();            // owns OverlayWindow::Create + SetTimer
  // Called by OverlayWindow::WndProc for every message it does not consume.
  LRESULT OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  void OnTick();                  // WM_TIMER: controller → render → present
  void OnLeftClick(int x, int y);
  void OnRightClick(int x, int y);
  void OnDisplayChange();
  void OnSettingChange();
  void OnDpiChanged(int newDpi);
  void HandleMenuChoice(ui::MenuChoice choice);
  void Reanchor();                // taskbar locator → MoveTo + SetWindowCenter
  void PresentFrame();            // renderer → overlay window
  void ScheduleTrim();            // one-shot trim after settle / menu

  // Subsystems (composition, declared in dependency order).
  desktop::CursorTracker   cursor_;
  CursorSourceAdapter      cursorSource_{cursor_};
  desktop::TaskbarLocator  taskbar_;
  desktop::OverlayWindow   window_;
  character::Controller    controller_;
  character::Character     character_;
  character::Renderer      renderer_;
  ui::Menu                 menu_;

  HINSTANCE hinst_ = nullptr;
  HWND      hwnd_   = nullptr;
  bool      reanchorPending_ = false;
  int       lastW_ = 0, lastH_ = 0;
};
```

## Contract / invariants

- One `App` per process. Created by `main.cpp` after GDI+ startup.
- `Run` blocks until `WM_QUIT`; returns a small exit code (0 = clean).
- `OverlayWindow::WndProc` is the only registered message handler; `App` receives
  every unhandled message through its `OnMessage` (wired via the `onMessage`
  callback). Clicks arrive as `OnLeftClick`/`OnRightClick`.
- `App` owns every subsystem and wires their callbacks:
  - `window_` callbacks → `OnLeftClick` / `OnRightClick` / `OnMessage`
  - `controller_` cursor source → `cursorSource_` (adapter over `cursor_`)
- No global mutable state.

## Ownership diagram

```
App
 ├─ cursor_        (stateless wrapper)
 ├─ cursorSource_  (adapter: cursor_ → ICursorSource)
 ├─ taskbar_       (stateless wrapper)
 ├─ window_        (owns HWND)
 ├─ controller_    (owns springs/timers + DrawState)
 ├─ character_     (stateless view)
 ├─ renderer_      (owns GDI+ bitmap/graphics)
 └─ menu_          (stateless popup)
```

## Memory notes

- `App` itself holds no buffers; total object overhead is a few hundred bytes.
- All heavy state is owned by two subsystems: `renderer_` (the 82 KB GDI+
  surface) and `window_` (the 82 KB presentation DIB) — together ~164 KB, the
  only large buffers in the process ([memory-budget.md](memory-budget.md)).

## Related

- [App.cpp.md](App.cpp.md) — the full implementation contract.
- [main.cpp.md](main.cpp.md) — construction.
- [architecture.md](architecture.md) — module map.
