# Architecture

## Module map

```
src/main.cpp
   │  wWinMain: DPI aware → App
   ▼
src/App.{h,cpp}                 ◄── owns everything, message loop
   ├── util/WinUtil.{h,cpp}          RAII, DPI scale, working-set trim
   ├── util/Autostart.{h,cpp}        HKCU Run registry key
   ├── desktop/CursorTracker.{h,cpp} GetCursorPos
   ├── desktop/TaskbarLocator.{h,cpp} SHAppBarMessage, work area
   ├── desktop/OverlayWindow.{h,cpp} layered window + UpdateLayeredWindow
   ├── character/Animator.{h,cpp}    damped springs
   ├── character/States.h            state enum
   ├── character/Controller.{h,cpp}  tick, state machine, animation values
   ├── character/Character.{h,cpp}   vector-art drawing
   ├── character/Renderer.{h,cpp}    GDI+ surface, lock, present
   └── ui/Menu.{h,cpp}               context menu
```

Dependency rule: `App` is the only aggregator. Each layer depends only downward
(desktop/character/util), never upward, and nothing depends on `App`.

## Data flow per tick (33 ms)

```
CursorTracker (user32.GetCursorPos)
        │ (x, y) screen
        ▼
CursorSourceAdapter (App.h)  →  character::ICursorSource::Get
        │ (x, y)
        ▼
Controller.Tick(nowMs)
   ├─ dead-zone check → decide Tracking/Idle
   ├─ cursor→local → pupil target & head tilt target
   ├─ springs (Animator) advance → current look
   ├─ blink / saccade / breath / interaction timers
   └─ produce DrawState + dirty flag
        │
        ▼
(if dirty) Renderer.BeginFrame(w,h)
        │  GDI+ Graphics on 128×160 32bpp PARGB bitmap
        ▼
Character.Draw(graphics, DrawState)   // head, eyes, mouth, body, hop
        │
        ▼
Renderer.LockBits → premultiplied ARGB scan0
        │
        ▼
OverlayWindow.Present(scan0, w, h, stride)
        │  UpdateLayeredWindow(hwnd, ...)  →  per-pixel alpha on screen
        └─ Windows compositor
```

## Threading model

**Single thread.** The UI message loop (`GetMessage`/`DispatchMessage`) runs on
the main thread; `WM_TIMER` (30 Hz) drives `App::OnTick`. There are no worker
threads, no locks, no cross-thread marshaling. This keeps the working set small
(one stack, one heap) and eliminates a whole class of bugs.

## Message / event flow

All messages arrive at `OverlayWindow::WndProc` (the only registered proc). It
consumes clicks itself and forwards everything else to `App::OnMessage` via the
`onMessage` callback:

- `WM_LBUTTONDOWN` → `OverlayWindow::WndProc` → `onLeftClick` → `App::OnLeftClick`
  → `controller_.OnLeftClick()` (hop reaction at next tick).
- `WM_RBUTTONDOWN` → `onRightClick` → `App::OnRightClick` → `Menu::Show` (modal)
  → dispatch chosen action.
- `WM_TIMER` (`kTickTimerId`, 33 ms) → `App::OnMessage` → `OnTick()`:
  1. `controller_.Tick(now)`
  2. if `controller_.WantsRepaint()` → render + present
  3. if re-anchor pending (from `WM_DISPLAYCHANGE`/`WM_SETTINGCHANGE`) → dock
- `WM_TIMER` (`kTrimTimerId`, one-shot 2 s) → trim working set.
- `WM_DISPLAYCHANGE` / `WM_SETTINGCHANGE` → mark `reanchorPending = true`.
- `WM_DPICHANGED` → re-read DPI, resize + re-dock.
- `WM_CLOSE` / menu *Quit* → `PostQuitMessage(0)`.
- Hit testing is **not** overridden: `DefWindowProcW` performs per-pixel alpha
  hit testing for layered windows on Win8+ (alpha=0 → `HTTRANSPARENT`,
  click-through). Forcing `WM_NCHITTEST` to `HTCLIENT` would break this.

## State machine (character)

```
            ┌────────────┐
            │    IDLE    │◄──────────────┐
            │ blink timer │              │ cursor still > 3 s
            │ saccades    │              │
            └─────┬──────┘              │
                  │ cursor moved         │
                  ▼                      │
            ┌────────────┐               │
            │  TRACKING  │───────────────┘
            │ eyes→cursor │
            └─────┬──────┘
                  │ left click
                  ▼
            ┌────────────┐   timer (1.2 s) expires
            │ INTERACTING│─────────────────────┐
            │ hop+surprise│                     │
            └────────────┘                     │
                  └─────────────────────────────┘  → re-evaluate Tracking/Idle
```

Implementation: enum in [States.h](character/States.h.md); transitions and
timers in [Controller.cpp](character/Controller.cpp.md).

## Coordinate systems

| Space | Origin | Used by |
|---|---|---|
| Screen | top-left of primary monitor, +x right, +y down | cursor, dock position |
| Client | top-left of our window | clicks, hit test |
| Character local | character center | pupil/head math |

Conversion in `Controller`: `lx = cx - windowCenterX`, `ly = cy - windowCenterY`;
`θ = atan2(ly, lx)`; pupil target `(cos θ·r, sin θ·r)` with `r ≤ kMaxEyeOffset`.

## Key invariants

1. Only `App` touches the message loop and owns the HWND lifecycle.
2. `Controller` never draws; `Character`/`Renderer` never poll the cursor.
3. `Renderer` is the only module that talks to GDI+.
4. Everything is single-threaded; no mutable globals except the `cfg::`
   constants in `config.h`.
5. Memory budget must be respected per [memory-budget.md](memory-budget.md).

## Build / test wiring

See [CMakeLists.txt.md](CMakeLists.txt.md), [scripts/build.ps1.md](scripts/build.ps1.md),
and the three `tests/*.cpp` docs. Console test EXEs run under CTest; the main
EXE is a GUI (`/SUBSYSTEM:WINDOWS`) app.
