# Docs Index

Complete per-file specification for the **Responsive 2D Character** — a C++17 /
Win32 / GDI+ Windows taskbar pet that watches the mouse cursor, running under a
**5 MB working-set budget**.

## Quick reference

| Doc | Contents |
|---|---|
| [overview.md](overview.md) | Product goals, stack decision, behavior spec, non-goals |
| [architecture.md](architecture.md) | Module map, data flow, state machine, message/tick flow |
| [memory-budget.md](memory-budget.md) | The <5 MB working-set budget, per-component, and how to keep it |
| [revisions.md](revisions.md) | Implemented deviations from these specs (read first) |
| [CMakeLists.txt.md](CMakeLists.txt.md) | Build configuration, flags, per-target settings |
| [scripts/build.ps1.md](scripts/build.ps1.md) | Build script contract and usage |
| [scripts/package.ps1.md](scripts/package.ps1.md) | Packaging, stripping, size verification |

## Source file docs (one per file)

### App bootstrap

| File | Doc | Role |
|---|---|---|
| `src/main.cpp` | [main.cpp.md](main.cpp.md) | `wWinMain` entry, DPI setup, boot |
| `src/config.h` | [config.h.md](config.h.md) | All design tokens / tuning constants |
| `src/App.h` | [App.h.md](App.h.md) | Application class contract |
| `src/App.cpp` | [App.cpp.md](App.cpp.md) | Window proc, tick loop, wiring |

### Desktop layer (Win32)

| File | Doc | Role |
|---|---|---|
| `src/desktop/CursorTracker.h` | [desktop/CursorTracker.h.md](desktop/CursorTracker.h.md) | Cursor query contract |
| `src/desktop/CursorTracker.cpp` | [desktop/CursorTracker.cpp.md](desktop/CursorTracker.cpp.md) | `GetCursorPos` wrapper |
| `src/desktop/TaskbarLocator.h` | [desktop/TaskbarLocator.h.md](desktop/TaskbarLocator.h.md) | Taskbar geometry contract |
| `src/desktop/TaskbarLocator.cpp` | [desktop/TaskbarLocator.cpp.md](desktop/TaskbarLocator.cpp.md) | `SHAppBarMessage`, work-area math |
| `src/desktop/OverlayWindow.h` | [desktop/OverlayWindow.h.md](desktop/OverlayWindow.h.md) | Layered window contract |
| `src/desktop/OverlayWindow.cpp` | [desktop/OverlayWindow.cpp.md](desktop/OverlayWindow.cpp.md) | `UpdateLayeredWindow`, per-pixel hit test |

### Character layer (logic + rendering)

| File | Doc | Role |
|---|---|---|
| `src/character/Animator.h` | [character/Animator.h.md](character/Animator.h.md) | Spring / easing contract |
| `src/character/Animator.cpp` | [character/Animator.cpp.md](character/Animator.cpp.md) | Damped-exponential spring |
| `src/character/States.h` | [character/States.h.md](character/States.h.md) | State enum + helpers |
| `src/character/Controller.h` | [character/Controller.h.md](character/Controller.h.md) | Tick / state machine contract |
| `src/character/Controller.cpp` | [character/Controller.cpp.md](character/Controller.cpp.md) | Cursor→targets, blink/saccade/breath |
| `src/character/Character.h` | [character/Character.h.md](character/Character.h.md) | Vector-art contract |
| `src/character/Character.cpp` | [character/Character.cpp.md](character/Character.cpp.md) | Head/eyes/mouth/body drawing |
| `src/character/Renderer.h` | [character/Renderer.h.md](character/Renderer.h.md) | GDI+ surface contract |
| `src/character/Renderer.cpp` | [character/Renderer.cpp.md](character/Renderer.cpp.md) | Bitmap, lock, premultiplied present |

### UI + util

| File | Doc | Role |
|---|---|---|
| `src/ui/Menu.h` | [ui/Menu.h.md](ui/Menu.h.md) | Popup menu contract |
| `src/ui/Menu.cpp` | [ui/Menu.cpp.md](ui/Menu.cpp.md) | `CreatePopupMenu` / `TrackPopupMenu` |
| `src/util/WinUtil.h` | [util/WinUtil.h.md](util/WinUtil.h.md) | RAII + DPI + memory helpers |
| `src/util/WinUtil.cpp` | [util/WinUtil.cpp.md](util/WinUtil.cpp.md) | `ScopeGdiplus`, `TrimWorkingSet`, DPI scale |
| `src/util/Autostart.h` | [util/Autostart.h.md](util/Autostart.h.md) | Registry autostart contract |
| `src/util/Autostart.cpp` | [util/Autostart.cpp.md](util/Autostart.cpp.md) | HKCU `Run` key read/write |

### Tests

| File | Doc | Role |
|---|---|---|
| `tests/test_animator.cpp` | [test_animator.cpp.md](test_animator.cpp.md) | Spring math |
| `tests/test_controller.cpp` | [test_controller.cpp.md](test_controller.cpp.md) | State machine + timing |
| `tests/test_math.cpp` | [test_math.cpp.md](test_math.cpp.md) | Pupil/head/hop math |

## Reading order

1. `overview.md` — what we build and why.
2. `memory-budget.md` — the hard constraint that shaped every decision.
3. `architecture.md` — how the pieces connect.
4. Per-file docs in the order above, top to bottom.
5. `CMakeLists.txt.md` + `scripts/build.ps1.md` — how to compile and verify.
