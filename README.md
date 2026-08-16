# Responsive 2D Character

A C++17 / Win32 / GDI+ Windows taskbar pet that watches the mouse cursor and
reacts: it follows your mouse with its eyes and head, blinks, breathes, and hops
when you click it. Runs under a hard **5 MB working-set budget** and draws almost
nothing when idle.

![milestones](./docs/overview.md#roadmap)

## Highlights

- **Always-on-top, click-through where transparent** layered window docked above
  the taskbar.
- **Eye-tracking** that eases toward the cursor with critically-damped springs
  (eyes fast, head slow).
- **Idle life**: blinking, saccade glances, and breathing that cost ~0 CPU when
  nothing changed.
- **Interaction**: click it and it hops with a surprised face.
- **Right-click menu**: Animate, Auto-start with Windows (HKCU Run key),
  Settings placeholder, Quit.
- **DPI-aware** (per-monitor v2) so it docks correctly on mixed-DPI setups.
- **Memory- and size-conscious**: /O1, LTCG, a single 82 KB render surface, and
  a scripted working-set gate.

## Build

Prerequisites: Visual Studio 2022 (MSVC) or MinGW with CMake ≥ 3.20.

```powershell
./scripts/build.ps1            # configure + build Release + run tests
./scripts/package.ps1          # copy to dist/, verify size + working set
```

The build produces `build/r2dc/Release/r2dc.exe`; tests run under CTest
(`ctest --test-dir build -C Release`).

## Usage

Run `r2dc.exe`. The pet docks at the bottom-left of the screen above the
taskbar. Click it to make it hop; right-click for the menu. Exit via the menu's
**Quit**.

## Documentation

Every source file has a one-to-one spec in [`docs/`](docs/README.md) — behavior,
invariants, and rationale. Suggested reading order:

1. [`docs/overview.md`](docs/overview.md) — product goals and behavior spec.
2. [`docs/memory-budget.md`](docs/memory-budget.md) — the constraint that shaped
   every decision.
3. [`docs/architecture.md`](docs/architecture.md) — module map and data flow.
4. [`docs/README.md`](docs/README.md) — full per-file index.

## Tests

- `test_animator` — spring math (convergence, damping, frame-rate independence).
- `test_controller` — the full state machine: dead zone, Tracking↔Idle timing,
  pupil direction/clamp, click→hop→return, blink scheduling, cursor loss, dirty
  flag.
- `test_math` — pupil clamp, head-tilt clamp, hop parabola, dt sanitization.

## Project layout

```
src/
  main.cpp                  wWinMain, DPI awareness, GDI+ startup
  App.h/.cpp                message loop, tick, re-anchoring, wiring
  config.h                  every tunable value in one header
  desktop/                  CursorTracker, TaskbarLocator, OverlayWindow
  character/                Controller (state machine), Character (vector art),
                            Renderer (GDI+ surface), Animator (springs)
  ui/                       Menu (right-click popup)
  util/                     WinUtil (GDI+/DPI/trim), Autostart (registry)
tests/                      CTest unit tests (no framework)
scripts/                    build.ps1, package.ps1
docs/                       per-file specs
```

## License / status

Personal learning project. v1 scope per [`docs/overview.md`](docs/overview.md);
see the roadmap there for v2 ideas.
