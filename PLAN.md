# Responsive 2D Character — Taskbar Pet

**Complete planning document** for a small animated 2D character that sits on the
Windows taskbar and continuously watches the mouse cursor (eyes + head follow),
with idle behavior, click interaction, and low CPU usage.

---

## 1. Product Goal

Build a small, always-on-top, transparent, clickable 2D character "pet" that:

1. Anchors itself to the Windows taskbar (auto-detects taskbar position/edge).
2. Tracks the global mouse cursor in real time.
3. Turns its **eyes** and **head** smoothly toward the cursor.
4. Performs **idle animations** (blinking, breathing, occasional saccade glances)
   when the cursor is still.
5. Reacts to **click / right-click** (reaction animation + context menu).
6. Stays transparent — only the character is visible, transparent areas let
   clicks pass through to apps underneath.
7. Runs at **low CPU** (redraws only when something visually changes).
8. Optionally **auto-starts** with Windows.

### Non-goals for v1
- No physics / gravity / window-dragging physics.
- No speech / chat / AI.
- No multi-monitor taskbar styling beyond basic edge detection (single monitor first, then multi-monitor).
- No 3D rendering.

---

## 2. Technology Decision

| Option | Pros | Cons | Verdict |
|---|---|---|---|
| **Python + PySide6** | Fast to iterate, easy vector drawing, ctypes for Win32, PyInstaller packaging | Slightly higher idle CPU than native; interpreter startup ~0.3s | **Recommended for v1** |
| C# + WPF | Native-ish, easy transparent windows, better perf | Slower iteration, heavier tooling | Fallback if Python perf disappoints |
| C++ + Direct2D | Best performance, game-like | Much slower to build, complex | Later, if pixel-art/sprites needed |

**Chosen: Python 3.11+ + PySide6 + ctypes (Win32 API).**

Rationale: the drawing is simple vector art; PySide6's `QPainter` renders with GPU
acceleration; ctypes gives direct access to `GetCursorPos`, `SHAppBarMessage`,
`SetWindowLong`, and `UpdateLayeredWindow`.

---

## 3. High-Level Architecture

```
┌─────────────────────────── Windows Desktop ───────────────────────────┐
│                                                                       │
│   Mouse Cursor (global)           Taskbar (auto-hidden / visible)     │
│         │                                    │                        │
│         ▼                                    ▼                        │
│   ┌─────────────┐                    ┌──────────────┐                 │
│   │ Cursor Tracker│  GetCursorPos @  │ Taskbar Locator│ SHAppBarMessage│
│   │ (Win32 via   │  ~30–60 Hz       │ (ABM_GETTASKBARPOS)            │
│   │  ctypes)     │                   └──────────────┘                 │
│   └──────┬───────┘                         │                          │
│          └──────────────┬──────────────────┘                          │
│                         ▼                                             │
│                ┌───────────────────┐                                  │
│                │ Character Controller│                                │
│                │  · look target math │                                │
│                │  · state machine    │                                │
│                │  · timers (blink,   │                                │
│                │    saccade, breath) │                                │
│                └─────────┬──────────┘                                 │
│                          ▼                                           │
│                ┌───────────────────┐                                  │
│                │ Animator (springs/ │  smooth interp, eases           │
│                │   easing curves)   │                                 │
│                └─────────┬──────────┘                                 │
│                          ▼                                           │
│                ┌───────────────────┐                                  │
│                │ Renderer (QPainter)│  draw to offscreen QImage w/    │
│                │   → per-pixel RGBA │  alpha; only redraw on change   │
│                └─────────┬──────────┘                                 │
│                          ▼                                           │
│                ┌───────────────────┐  WS_EX_LAYERED, TOOLWINDOW,      │
│                │ Overlay Window    │  TOPMOST, NOACTIVATE;            │
│                │ (PySide6 QWidget) │  UpdateLayeredWindow for per-pixel│
│                └───────────────────┘  hit-testing                     │
└────────────────────────────────────────────────────────────────────────┘
```

**Data flow:** Win32 (imperative C calls) → ctypes adapters (pure Python) →
controller (logic, no GUI) → renderer (draws QImage) → window (presents via
layered window). Keeping GUI concerns out of the controller makes it testable.

---

## 4. Core Modules (Detailed Design)

### 4.1 `desktop/cursor.py` — Cursor Tracker

- Wraps `ctypes.windll.user32.GetCursorPos`.
- Polled by a 30 Hz timer in the controller (every 33 ms). 30 Hz is enough: a
  human arm moves at ~7 Hz max; 30 Hz sampling with smoothing looks flawless.
- Optimization: if the cursor hasn't moved more than a dead-zone radius
  (e.g. 4 px) since last sample, skip the recompute of eye targets (but still
  allow idle animation timers to run).

```python
POINT = ctypes.wintypes.POINT
def get_cursor_pos() -> tuple[int, int] | None:
    p = POINT()
    if ctypes.windll.user32.GetCursorPos(ctypes.byref(p)):
        return p.x, p.y
    return None
```

### 4.2 `desktop/taskbar.py` — Taskbar Locator

- Calls `SHAppBarMessage(ABM_GETTASKBARPOS, ...)` via `ctypes` + `shell32`.
- Returns: `(edge: Edge, rect: RECT)` where edge is LEFT/RIGHT/TOP/BOTTOM and
  rect is the taskbar's screen rect.
- Also handles **auto-hide**: when taskbar is auto-hidden, query `ABM_GETSTATE`
  / use `MonitorFromPoint` + `GetMonitorInfo` work-area to decide placement.
- Multi-monitor: enumerate monitors via `EnumDisplayMonitors`, find the one the
  taskbar lives on; v1 targets the primary monitor's taskbar, extend later.

**Placement strategy (v1):**
The character window floats **just above the taskbar** and docks to its inner
edge, e.g. for a bottom taskbar the window bottom sits 2 px above the taskbar
top edge. This avoids fighting the system for space *inside* the taskbar and
never overlaps Start-button / system tray interactions.

> Later (v2): reserve a slice of empty taskbar space using
> `SHAppBarMessage(ABM_NEW, ABM_QUERYPOS)` to "own" a 96×96 zone inside the
> taskbar itself. This is more complex and riskier (system UI reflow) — deferred.

### 4.3 `desktop/window.py` — Overlay Window

Native flags (set with `SetWindowLongPtr(GWL_EXSTYLE)` on the Qt window handle):

| Flag | Purpose |
|---|---|
| `WS_EX_LAYERED` | Per-pixel alpha composition |
| `WS_EX_TOPMOST` | Always above normal windows |
| `WS_EX_TOOLWINDOW` | Hidden from Alt-Tab / taskbar buttons |
| `WS_EX_NOACTIVATE` | Never steals keyboard focus on click |
| `WS_EX_TRANSPARENT` | Lets mouse fall through transparent pixels |

Qt hints on the `QWidget`:
- `Qt.FramelessWindowHint`
- `Qt.WindowStaysOnTopHint`
- `Qt.Tool` (no taskbar entry)
- `Qt.WA_TranslucentBackground`
- `Qt.WA_ShowWithoutActivating`

**Per-pixel click-through** (key trick):
- Render the frame to an offscreen `QImage` (RGBA).
- Pass it to `UpdateLayeredWindow()`.
- Windows automatically hit-tests only *opaque* pixels — transparent pixels are
  click-through for free, and the character stays clickable.

```python
# pseudo — frame N
qimage = painter_render_frame()          # 4-channel RGBA, character only
hdc  = GetDC(0)
memdc = CreateCompatibleDC(hdc)
# select qimage.handle() as bitmap
UpdateLayeredWindow(hwnd, hdc, pt_screen, size, memdc,
                    (0,0), 0, ULW_ALPHA)
ReleaseDC(0, hdc)
```

Qt note: PySide6 supports this pattern; alternatively, keep a normal top-level
widget the exact size of the character (a tight bounding box) and skip per-pixel
hit testing in v1. The document recommends the tight-window approach first
(simplest, robust) with per-pixel `UpdateLayeredWindow` as a stretch goal.

### 4.4 `character/controller.py` — Character Controller

Central `tick(dt, cursor_pos)` called every 33 ms:

1. Convert cursor screen position to **local space** relative to character
   center: `lx, ly = cursor - window_center`.
2. Compute **look target**: `angle = atan2(ly, lx)`; eye target offset =
   `(cos(angle) * EYE_R, sin(angle) * EYE_R)` clamped to the eyeball radius.
3. Feed targets into the animator (springs) → current eye/head values.
4. Run the **state machine** timers (blink, saccade, breath phase).
5. If any visual value changed beyond epsilon → set `dirty` flag → repaint.

### 4.5 `character/animator.py` — Smoothing / Springs

Two standard options; use **critically-damped springs** for eyes/head:

```python
# frame-rate independent spring: value approaches target
v   += (target - v) * (1 - exp(-dt / TAU))      # TAU ≈ 0.06 s (eyes)
head+= (head_t  - head) * (1 - exp(-dt / TAU2)) # TAU2 ≈ 0.12 s (slower)
```

Properties: never overshoots (damped), smooth, responsive yet soft. Breathing is
a sinusoid `breath = sin(2π * t / BREATH_PERIOD)` added as scale on the body.

### 4.6 `character/character.py` + `parts.py` — The Character

**Vector art** built from `QPainterPath` (no image assets needed, scales with DPI):

- **Head**: rounded circle/ellipse, subtle tilt by `head_angle * max_tilt`.
- **Eyes**: two white ellipses; pupil circles offset by the eye target vector.
  Blink = vertical scale of the eyelid toward 0 over ~100 ms.
- **Mouth**: small line/arc; smiles on interaction, neutral otherwise.
- **Body**: rounded rect slightly below head; sways with breathing and shifts
  opposite to head tilt (counterbalance) for organic feel.

Design tokens (constants): `EYE_R`, `PUPIL_R`, `MAX_EYE_OFFSET`, `HEAD_MAX_TILT`,
`BLINK_PERIOD`, `BLINK_DURATION`, `SACCADE_MIN/MAX_INTERVAL`.

### 4.7 `character/states.py` — State Machine

```
          ┌──────────────────────┐
          │         IDLE         │◄──────┐  cursor still ≥ N s
          │  blink timer         │       │
          │  breath anim         │       │
          │  saccade glances     │       │
          └──────────┬───────────┘       │
                     │ cursor moved      │
                     ▼                   │
          ┌──────────────────────┐       │
          │       TRACKING       │       │
          │  eyes→cursor         │       │
          │  head→cursor         │       │
          └──────────┬───────────┘       │
                     │ click             │
                     ▼                   │
          ┌──────────────────────┐       │
          │   INTERACTION (1.2s) │       │
          │  surprise eyes, hop  │───────┘  timer expires
          └──────────────────────┘
```

- **IDLE** → **TRACKING**: cursor displacement exceeds dead-zone.
- **TRACKING** → **IDLE**: cursor still for > `IDLE_TIMEOUT` (≈ 3 s).
- **TRACKING/IDLE** → **INTERACTION**: left-click on character (hop + surprised
  face), right-click opens menu. After 1.2 s → back to TRACKING/IDLE.

### 4.8 `ui/menu.py` — Interaction Menu

`QMenu` triggered on right-click:
- Animate (spin / wave) — manual trigger
- Settings…
- Enable auto-start (toggle)
- Quit

`QSystemTrayIcon` is *not* required for v1 (character itself is the handle).

---

## 5. Rendering & CPU Budget

| Item | Approach | CPU cost |
|---|---|---|
| Poll cursor | 30 Hz timer, dead-zone skip | ~0.1 % |
| Animate | same 33 ms tick, pure math | ~0.1 % |
| Draw | only when `dirty`; QPainter vector, offscreen QImage | ~0.2 % |
| Present | `UpdateLayeredWindow` only on dirty | ~0.1 % |
| Idle timers | `QTimer` for blink/saccade | negligible |

Total target: **< 1 % CPU idle**, no measurable GPU cost when nothing changes.

**Repaint optimization:** compute all eye/head/blink values → if the rendered
frame would differ by < 1 px, skip the draw. Blink timer only triggers a frame
during its ~100 ms active window.

---

## 6. Coordinate Math (Reference)

- Screen coords origin: top-left, `+x` right, `+y` down.
- Character local frame: origin at character center; `(lx, ly)` in pixels.
- Look angle: `θ = atan2(ly, lx)`.
- Pupil offset: `px = cos(θ) * min(EYE_R - PUPIL_R, MAX_OFFSET)`, `py` likewise.
- Head tilt: `tilt = clamp(θ * 0.15, -MAX_TILT, MAX_TILT)` in radians.
- All smooth quantities use exponential approach (section 4.5).

---

## 7. Project Structure

```
Responsive-2D-character/
├── app/
│   ├── main.py                  # entry point; QApplication + wiring
│   ├── config.py                # design tokens, defaults, paths
│   ├── desktop/
│   │   ├── cursor.py            # GetCursorPos
│   │   ├── taskbar.py           # SHAppBarMessage, monitor enum
│   │   └── window.py            # overlay window + layered present
│   ├── character/
│   │   ├── controller.py        # tick loop, dead-zone, triggers
│   │   ├── animator.py          # springs / easing
│   │   ├── states.py            # state machine
│   │   ├── parts.py             # vector drawing of head/eyes/body
│   │   └── character.py         # high-level draw(renderer, t)
│   └── ui/
│       ├── menu.py              # context menu
│       └── settings.py          # settings dialog (optional)
├── tests/
│   ├── test_animator.py         # spring convergence, no overshoot
│   ├── test_controller.py       # state transitions, dead-zone
│   └── test_math.py             # pupil offset, angle math
├── scripts/
│   └── build.ps1                # PyInstaller one-file build
├── requirements.txt             # PySide6, (pywin32 optional)
├── README.md
└── PLAN.md                      # this document
```

---

## 8. Implementation Milestones

| Milestone | Deliverable | Acceptance |
|---|---|---|
| **M0** | Skeleton: `QApplication`, frameless translucent always-on-top widget | Blank window appears topmost, no taskbar entry |
| **M1** | Taskbar detection + anchoring | Window docks to bottom taskbar edge on monitor change; handles auto-hide |
| **M2** | Cursor tracking + static eyes render | Eyes point at cursor; dead-zone works |
| **M3** | Springs + head tilt | Smooth, non-snappy following; idle→tracking transitions |
| **M4** | Idle animation | Blink, breathing, occasional saccades |
| **M5** | Interaction | Click hop + surprised face; right-click menu (Settings/Quit) |
| **M6** | Auto-start (HKCU Run key) + packaging | One-file EXE; toggling works; `build.ps1` clean |
| **M7** | Perf polish + edge cases | < 1 % CPU idle; multi-monitor aware; screen-lock/DND-safe |

Estimated: each milestone is independently runnable/testable.

---

## 9. Windows Integration Details

### Auto-start
Write registry value:
```
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
  "Responsive2DCharacter" = "<path-to-exe>"
```
Remove the value to disable. Installed with `--autostart-on` / `--autostart-off`
flags or the Settings menu.

### Edge cases to handle
- **Taskbar auto-hide**: character docks to screen edge instead of taskbar edge.
- **Taskbar moved / multi-monitor**: re-query `ABM_GETTASKBARPOS` on
  `WM_SETTINGCHANGE` / `DisplayChange` events and re-anchor.
- **Screen lock / sleep**: pause timers on `WM_SESSION_CHANGE`; wake gracefully.
- **High DPI**: set `QApplication::setHighDpiScaleFactorRoundingPolicy`, use
  device pixel ratio when sizing.
- **Cursor leaves logical screen / spans monitors**: clamp look targets.
- **Another pet/window overlaps**: character is always-on-top; ignore.

---

## 10. Testing Strategy

- **Unit** (`pytest`): spring no-overshoot convergence; state transitions;
  pupil math; dead-zone logic. Pure logic, no GUI.
- **Manual smoke** (per milestone): run EXE, verify topmost/transparency/clicks.
- **Perf check**: task-manager CPU at idle vs. cursor-moving; confirm < 1 %.
- **Multi-monitor / DPI / auto-hide**: manual checklist in M7.

---

## 11. Future Enhancements (post-v1)

- v2: taskbar in-bar placement via `ABM_NEW` (character literally inside taskbar).
- v3: pixel-art sprites + frame animation; particle effects on interaction.
- v4: multiple characters / themes; speech bubbles; config file.
- v5: C++/Direct2D port if < 1 % CPU becomes a hard requirement.

---

## 12. Risks & Mitigations

| Risk | Mitigation |
|---|---|
| Per-pixel hit-testing complexity | Ship v1 with tight bounding-box window; enable `UpdateLayeredWindow` later |
| Taskbar layout changes break anchoring | Re-anchor on `WM_SETTINGCHANGE`; keep taskbar module isolated |
| Python startup latency (~0.3 s) | Acceptable for a desktop app; document in README |
| Qt licensing (LGPL) | Dynamic linking via PySide6 satisfies LGPL for a desktop utility |
