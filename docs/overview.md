# Overview

## What we are building

A small, always-on-top, transparent, clickable 2D character that **sits on top
of the Windows taskbar** and continuously **watches the mouse cursor**. Its
eyes and head rotate smoothly toward wherever the cursor is; when the cursor is
still it idles (blinks, breathes, glances around); clicking it triggers a
reaction and a context menu.

**Hard requirement:** the app must run under **5 MB working-set memory**
(Task Manager column) during normal operation on Windows 10/11.

## Stack (final decision)

| Aspect | Choice | Why |
|---|---|---|
| Language | **C++17** | Zero-runtime native, deterministic memory |
| GUI | **Win32 API** (no MFC/Qt) | Smallest possible footprint, direct control |
| Drawing | **GDI+** | Anti-aliased vector art (head/eyes) at low cost |
| Win32 services | `user32`, `gdi32`, `gdiplus`, `shell32`, `advapi32` | All system-loaded; no extra DLLs |
| Build | **CMake + MSVC** (`cl.exe`, Release, `/MT`) | Static CRT, `/O1`, LTCG |
| Tests | Plain console asserts + CTest | No framework overhead |
| Package | Single stripped EXE via `scripts/build.ps1` + `scripts/package.ps1` | ~0.2–0.4 MB on disk |

> **Why not Python/PySide6?** A Python interpreter alone needs ~10–20 MB of RAM
> and PySide6 adds 100–200 MB. The 5 MB budget is impossible in Python. C++17 +
> Win32 + GDI+ meets it while staying fast to iterate.

## Behavior specification

### Always-on / placement
- Window is `WS_POPUP`, layered (`WS_EX_LAYERED`), always on top
  (`WS_EX_TOPMOST`), no Alt-Tab entry (`WS_EX_TOOLWINDOW`), never steals focus
  (`WS_EX_NOACTIVATE`).
- The window is a **tight 128×160 logical-pixel box** around the character
  (scaled by DPI).
- It docks to the taskbar inner edge. For a bottom taskbar: the character's feet
  sit `kAnchorGap = 2 px` above the taskbar top edge; the body is near the left
  side (`kAnchorX = 64 px`) to avoid the system tray.
- Transparent pixels (alpha = 0) let the mouse fall through to windows below
  (per-pixel hit testing, Win8+ layered-window behavior). Opaque character
  pixels are clickable.

### Cursor tracking
- Global cursor sampled at **30 Hz** (`GetCursorPos`).
- A **dead zone** of `kCursorDeadZonePx = 4 px` suppresses re-targeting when the
  cursor barely moves.
- The character turns toward the cursor using a **critically-damped spring**
  (eyes fast `τ=0.06 s`, head slower `τ=0.12 s`) — smooth, never snappy.

### Idle
- **Blink:** random every 2.4–5.2 s, 110 ms eyelid closure.
- **Breathe:** sinusoidal body scale, period 3.4 s, amplitude 1.6 px.
- **Saccade:** occasional quick glance to a random direction while idle.

### Interaction
- **Left click:** hop reaction (10 px parabola) + surprised face, 1.2 s.
- **Right click:** context menu — *Animate*, *Toggle autostart*, *Settings…*
  (placeholder), *Quit*.

### Resource policy
- Idle CPU < 1 %: repaint **only when a visible value changed** (dirty flag).
- Idle working set < 5 MB: see [memory-budget.md](memory-budget.md).

## Non-goals (v1)
- No physics / drag / gravity.
- No in-taskbar reservation via `ABM_NEW` (that is a v2 stretch goal).
- No multi-monitor-aware docking beyond primary-monitor taskbar (v1.1).
- No speech / AI / chat.
- No pixel-art sprite sheets (vector art only in v1).

## High-level behavior map

```
cursor moves  ──► Tracking   (eyes + head follow)
cursor still  ──► Idle       (blink, breathe, glance)
click         ──► Interacting(hop + surprise, 1.2 s) ──► back to Idle/Tracking
```

## Reading next
- [memory-budget.md](memory-budget.md) — the constraint that drives all design.
- [architecture.md](architecture.md) — module map and flow.
