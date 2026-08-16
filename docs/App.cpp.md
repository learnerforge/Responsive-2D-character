# src/App.cpp — Application Implementation

## File role

The wiring heart: creates the pet window, runs the tick, routes input, re-anchors
on system changes, and drives the render→present pipeline.

## File-local constants and helpers

```cpp
namespace {
  constexpr UINT_PTR kTickTimerId = 1;    // 30 Hz simulation timer
  constexpr UINT_PTR kTrimTimerId = 2;    // one-shot working-set trim after settle

  long long steady_clock_ms() noexcept {  // monotonic clock in ms
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
        .count();
  }
}
```

## CreateWindow

```cpp
bool App::CreateWindow() {
  desktop::WindowCallbacks cb;
  cb.ctx          = this;
  cb.onLeftClick  = [](void* c, int x, int y) { static_cast<App*>(c)->OnLeftClick(x, y); };
  cb.onRightClick = [](void* c, int x, int y) { static_cast<App*>(c)->OnRightClick(x, y); };
  cb.onMessage    = [](void* c, HWND h, UINT m, WPARAM w, LPARAM l) {
                      return static_cast<App*>(c)->OnMessage(h, m, w, l);
                    };

  if (!window_.Create(hinst_, cb)) return false;
  hwnd_ = window_.Handle();

  controller_.SetCursorSource(&cursorSource_);   // adapter over cursor_

  // 30 Hz tick timer.
  ::SetTimer(hwnd_, kTickTimerId, 1000 / cfg::kTickHz, nullptr);
  return true;
}
```

> Message routing is centralized in `OverlayWindow::WndProc`
> ([OverlayWindow.cpp.md](desktop/OverlayWindow.cpp.md)): clicks → its callbacks,
> everything else → `cb_.onMessage` → `App::OnMessage` below. `App` never
> registers a window class or touches `GWLP_USERDATA`.

## OnMessage (system messages from the overlay)

```cpp
LRESULT App::OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_TIMER:
      if (wp == kTickTimerId) { OnTick(); return 0; }
      if (wp == kTrimTimerId) {
        ::KillTimer(hwnd, kTrimTimerId);       // one-shot
        util::TrimWorkingSet(false);
        return 0;
      }
      break;
    case WM_DISPLAYCHANGE:  OnDisplayChange();  return 0;  // monitor changed
    case WM_SETTINGCHANGE:  OnSettingChange();  return 0;  // taskbar moved/auto-hide
    case WM_DPICHANGED:     OnDpiChanged(LOWORD(wp)); return 0;  // new DPI in wParam
    case WM_CLOSE:          ::PostQuitMessage(0);       return 0;  // clean exit
  }
  return ::DefWindowProcW(hwnd, msg, wp, lp);
}
```

### Message table

| Message | Handler | Purpose |
|---|---|---|
| `WM_TIMER` (`kTickTimerId`) | `OnTick()` | 30 Hz simulation + repaint |
| `WM_TIMER` (`kTrimTimerId`) | trim once | working-set shed after settle |
| `WM_DISPLAYCHANGE` | `OnDisplayChange` | monitor change → re-anchor |
| `WM_SETTINGCHANGE` | `OnSettingChange` | taskbar move/auto-hide → re-anchor |
| `WM_DPICHANGED` | `OnDpiChanged` | DPI change → resize surface + re-anchor |
| `WM_CLOSE` | `PostQuitMessage(0)` | clean exit |
| (anything else) | `DefWindowProcW` | default handling (incl. per-pixel hit test) |

Note: `WM_NCHITTEST` is intentionally **not** handled — `DefWindowProcW` performs
the per-pixel alpha hit test for layered windows (Win8+), making transparent
pixels click-through. See [OverlayWindow.cpp.md](desktop/OverlayWindow.cpp.md).

## OnTick (30 Hz)

```cpp
void App::OnTick() {
  const long long now = steady_clock_ms();
  controller_.Tick(now);

  if (reanchorPending_) Reanchor();             // cheap, only on system events
  if (controller_.WantsRepaint()) PresentFrame();
}
```

### PresentFrame

```cpp
void App::PresentFrame() {
  const int scale = util::WindowDpiScale(hwnd_);
  const int w = cfg::kWindowWidth  * scale;
  const int h = cfg::kWindowHeight * scale;

  if (!renderer_.EnsureSurface(w, h)) return;   // surface ready (lazy, once)
  if (auto* g = renderer_.BeginFrame()) {
    character_.Draw(*g, controller_.Current(), w, h);
    if (const auto* bits = renderer_.LockForPresent()) {
      window_.Present(bits, w, h, renderer_.Stride());
      renderer_.Unlock();
    }
  }
  controller_.ClearRepaint();
}
```

## Reanchor

```cpp
void App::Reanchor() {
  const int scale = util::WindowDpiScale(hwnd_);
  const int w = cfg::kWindowWidth  * scale;
  const int h = cfg::kWindowHeight * scale;

  const auto info = taskbar_.Locate();
  const auto dock = taskbar_.DockBottom(info, w, h);
  window_.MoveTo(dock.x, dock.y, dock.w, dock.h);

  controller_.SetWindowCenter(dock.x + w / 2, dock.y + h / 2);
  reanchorPending_ = false;
  lastW_ = w; lastH_ = h;
}
```

- Called at startup (first `OnTick`), on `WM_DISPLAYCHANGE`,
  `WM_SETTINGCHANGE`, `WM_DPICHANGED`.
- On `WM_DPICHANGED`, the OS's suggested rect (in `lp`) is ignored; we recompute
  from the taskbar so the pet stays glued to the taskbar rather than the old
  screen spot. Consistency with the taskbar is preferred.

## Click handling

```cpp
void App::OnLeftClick(int, int) { controller_.OnLeftClick(); }

void App::OnRightClick(int clientX, int clientY) {
  int sx = clientX, sy = clientY;
  util::ClientToScreen(hwnd_, sx, sy);          // client → screen
  menu_.SetAutostartChecked(util::Autostart::IsEnabled());
  const auto choice = menu_.Show(hwnd_, sx, sy);
  HandleMenuChoice(choice);
  util::TrimWorkingSet(true);                    // shed menu pages
}
```

> Note: `OverlayWindow` delivers client coordinates; converting here keeps the
> overlay module coordinate-agnostic. The right-click path is fully synchronous
> (menu is modal) — no flags, no deferral.

## HandleMenuChoice

| Choice | Action |
|---|---|
| `Animate` | `controller_.OnLeftClick()` (reuse the hop reaction) |
| `ToggleAutostart` | `util::Autostart::SetEnabled(!util::Autostart::IsEnabled())` |
| `Settings` | `MessageBoxW` placeholder ("Settings dialog coming in v2") |
| `Quit` | `PostQuitMessage(0)` |
| `None` | nothing |

## Run

```cpp
int App::Run() {
  if (!CreateWindow()) return 2;
  Reanchor();                        // initial dock before first tick
  ::SetTimer(hwnd_, kTrimTimerId, 2000, nullptr);   // one-shot settle trim

  MSG msg{};
  while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);        // messages go to OverlayWindow::WndProc
  }
  return (int)msg.wParam;
}
```

## Memory / performance

- 30 Hz `WM_TIMER` → CPU near zero when idle because `PresentFrame` is skipped
  when not dirty.
- `GetMessage` (not `PeekMessage` + sleep) blocks when no messages — the OS
  sleeps the thread for us; this is the correct idle loop for a GUI app.
- Only allocations: renderer surface (once), transient menu/HMENU, transient
  `std::wstring` in autostart — all budgeted.

## Edge cases

| Case | Behavior |
|---|---|
| First tick before anchor | `reanchorPending_` set in `Run` → docked in first `OnTick` before first draw |
| Monitor unplugged | `WM_DISPLAYCHANGE` → re-dock (taskbar may move) |
| Taskbar auto-hidden toggled | `WM_SETTINGCHANGE` → re-dock to work-area bottom |
| DPI moved across monitors | `WM_DPICHANGED` → recreate surface at new scale + re-dock |
| Secure desktop (UAC) | `GetCursorPos` fails → controller holds state; window keeps idle-animating |
| Window destroyed | `WM_CLOSE`/Quit → `PostQuitMessage` → `Run` returns → clean teardown |

## Testing

- Automated: none at this level (UI). Manual per milestone:
  M0 window topmost/transparent, M1 docking, M3 following, M4 idle, M5
  interaction, M6 autostart + packaging.
- Working-set gate run by `scripts/package.ps1`.

## Related

- [App.h.md](App.h.md) — declaration/contract
- [architecture.md](architecture.md) — flow
- [OverlayWindow.cpp.md](desktop/OverlayWindow.cpp.md) — WndProc → onMessage routing
- [WinUtil.cpp.md](util/WinUtil.cpp.md) — trim/DPI helpers
