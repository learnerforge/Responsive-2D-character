# Spec Revisions — Implemented Deviations

Every place where the implemented code intentionally differs from the per-file
specs. Read this alongside the specs; where it conflicts with a `.md` doc, the
code (and this file) is authoritative.

## Build-affecting fixes (docs were never compiled)

| # | Doc | Spec said | Why it broke | Fix in code |
|---|---|---|---|---|
| R1 | `Character.h.md`, `Renderer.h.md` | forward-declared GDI+ in `namespace gdiplus` (lowercase) | GDI+ actually lives in `namespace Gdiplus`; header/definition mismatch → compile error | forward-declarations and member types use `Gdiplus::` |
| R2 | `Character.cpp.md` | `g.FillEllipse(&SolidBrush(...), ...)` — address-of-rvalue | non-standard MSVC-ism; GCC (MinGW-w64) rejects it | named locals (`SolidBrush skin(...); g.FillEllipse(&skin, ...)`) |
| R3 | `App.h.md` / `App.cpp.md` | private method `App::CreateWindow()` | collides with the `CreateWindow` macro from `windows.h` (`UNICODE` → `CreateWindowW`) — breaks on MSVC too | renamed to `App::CreateOverlay()` |
| R4 | `OverlayWindow.cpp.md` | uses `GET_X_LPARAM`/`GET_Y_LPARAM` | those macros live in `<windowsx.h>`, not `<windows.h>` | added `#include <windowsx.h>` |
| R5 | `CMakeLists.txt.md` | one `target_include_directories(test_animator test_controller test_math ...)` call | CMake 4 rejects multi-target form in this context | split into three calls |

## MinGW / non-MSVC support

| # | Doc | Spec said | Fix in code |
|---|---|---|---|
| R6 | `CMakeLists.txt.md` | MSVC-only flags | added a MinGW fallback branch: `target_link_options(r2dc PRIVATE -municode -mwindows -static)` |
| R7 | `CMakeLists.txt.md` | `-DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00` | also add `-DNOMINMAX -DWIN32_LEAN_AND_MEAN` (keeps `std::min/max/clamp` working) |
| R8 | (implicit) | `#include <gdiplus.h>` directly | mingw-w64 GDI+ headers require `PROPID` from `<wtypes.h>` first; added the include in `WinUtil.cpp`, `Renderer.cpp`, `Character.cpp` |
| R9 | `CMakeLists.txt.md` size gate | `r2dc.exe` ≤ ~500 KB (MSVC `/O1` + LTCG) | MinGW static CRT produces ~830 KB; the on-disk gate is MSVC-only. The real budget (working set) holds: measured **1.03 MB** vs the 5 MB limit. |

## Controller / state-machine corrections

| # | Doc | Spec said | Fix in code |
|---|---|---|---|
| R10 | `Controller.h.md` private section | omitted `DecideDirty()` and the previous-frame bookkeeping | added `DecideDirty()`, `prevDraw_`, `lastDirtyState_` (all required by `Controller.cpp.md` §DecideDirty) |
| R11 | `Controller.cpp.md` first-`Tick` | "initialize, do nothing else" (sentinel `lastTickMs_ == 0`) | first `Tick` now also seeds `lastCursorX_/Y_` (the dead-zone baseline) and schedules the first blink/saccade after a full random interval. Uses a real `initialized_` bool — `lastTickMs_ == 0` is ambiguous because test clocks legitimately start at 0 |
| R12 | `Controller.cpp.md` | blinks/saccades could fire instantly at `t=0` (`nextBlinkMs_` starts 0) | first interval is scheduled at init, so the first blink lands in `[2400, 5200]` ms (asserted by `test_controller` #7) |
| R13 | `Controller.h.md`/`.cpp.md` | `UpdateBlink(nowMs)`, `UpdateSaccade(nowMs)` decrement phase by an implicit dt | signatures take an explicit `dtMs`; clearer and free of per-tick clock arithmetic |
| R14 | `Controller.cpp.md` §UpdateSaccade | glance targets could persist after leaving Idle | when `state_ != Idle`, `glanceTarget_*` are reset to 0 (glance relaxes in Tracking) |
| R15 | `Animator.cpp.md` | spring snippet had no `dt ≤ 0` guard | `Spring::Tick` now returns unchanged on `dtSeconds <= 0` (required by `test_animator` #4) |
| R16 | `config.h.md`, `Controller.cpp.md` | `kDirtyBreathPx = 0.05f` | peak breath delta is `amplitude × 2π/T × dt ≈ 0.098 px` per 33 ms tick — a 0.05 threshold would mark nearly every tick dirty and destroy the idle-CPU goal. Implementation uses `kDirtyBreathPx = 0.2f` (must stay above ~0.1) |

## Test-file corrections

| # | Doc | Spec said | Fix in code |
|---|---|---|---|
| R17 | `test_animator.cpp.md` | `CHECK(Close(v, 3.0f, 0.0f))` with strict `<` | `|a-b| < 0` is never true; changed exact-equality checks to `eps = 1e-6f` |
| R18 | `test_math.cpp.md` §4 | `atan2f(100, 1) * 0.15 = 0.46` (assumed) | actual value is `1.56 × 0.15 = 0.234 < 0.30` → never clamps; changed input to `atan2f(1, -1)` (`2.36 × 0.15 = 0.353`) |
| R19 | `test_controller.cpp.md` | one global static `ctl` shared across all tests | sequential state/spring bleed breaks single-tick assertions; each test now builds a fresh `Rig` (controller + fake cursor at window center) |
| R20 | `test_controller.cpp.md` §4 | single-tick window for `|pupilX| < 0.5` after a direction change | with `τ=0.06` the residual after one tick is ~1.2 px; the test now lets the spring settle ~6 ticks (~200 ms) |

## Minor behavior notes

| # | Doc | Spec said | Fix in code |
|---|---|---|---|
| R21 | `Autostart.cpp.md` | empty-path guard "App should guard" | the guard lives inside `Autostart::SetEnabled` (returns false on empty path) |
| R22 | `OverlayWindow.cpp.md` | `Create` never shows the window | `Create` also calls `ShowWindow(SW_SHOWNOACTIVATE)` defensively; `UpdateLayeredWindow`/`MoveTo` would show it anyway |
| R23 | `WinUtil.cpp.md` | calls `::GetDpiForWindow` directly | resolved via `GetProcAddress` so a toolchain without the DPI declaration still compiles; falls back to the primary-monitor LOGPIXELSX |
| R24 | `config.h.md` | `cfg::kAutostartValueName` | declared but unused — `util::Autostart` keeps its own `kValueName` (per `Autostart.h.md`) |

## Verification

- `./scripts/build.ps1` equivalent (Ninja + MinGW-w64 UCRT64): configure, build,
  `ctest` — all 3 suites pass.
- App smoke: launches, creates the layered window + GDI+ surface, runs the
  message loop. Working set 1.03 MB, private 1.68 MB, 113 handles (after 4 s).
