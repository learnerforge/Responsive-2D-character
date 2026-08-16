# src/character/Character.h — Vector-Art Contract

## File role

Declares the class that draws the character's body from a `DrawState`. Pure
GDI+ drawing — no Win32 window knowledge, no logic. It is the "view" of the
character.

## Public API

```cpp
#pragma once
#include <windows.h>

namespace gdiplus { class Graphics; }

namespace character {

struct DrawState;   // defined in Controller.h

class Character {
public:
  Character() = default;

  // Draw one frame of the character centered in the (w × h) surface.
  // g must be a valid GDI+ Graphics with AA enabled (Renderer provides it).
  void Draw(gdiplus::Graphics& g, const DrawState& st, int w, int h);

  // Logical hit test: is the client point (px, physical px) on the character?
  // Currently unused by the app (the layered window's per-pixel alpha already
  // restricts clicks to opaque character pixels); kept as a defensive helper.
  bool HitTest(float x, float y) const noexcept;
};

}  // namespace character
```

## Contract / invariants

- `Draw` must be idempotent: drawing the same `DrawState` twice yields identical
  output (important for the dirty-flag repaint logic).
- Drawing coordinates are derived from `w`/`h` (the surface) — the character
  scales with the surface, so DPI handling stays in `App`/`Renderer`.
- `HitTest` uses the **un-scaled** geometry constants from `config.h` and a
  caller-supplied DPI scale internally (or accepts scaled coords — documented
  in the .cpp). It is allowed to be conservative (return true over a slightly
  larger area); false positives only matter if the overlay overlaps another
  control.
- `Draw` never throws; GDI+ failures are silently tolerated (draw nothing
  on that element).
- No state stored between draws → fully re-entrant, trivially testable.

## Visual composition (drawn top-to-bottom, back-to-front)

1. **Body** — ellipse below the head; shifted by breath and counter-tilt.
2. **Head** — large ellipse; rotated by `headTilt` around the neck point.
3. **Eyes** — two white ellipses; vertical scale = `blink`.
4. **Pupils** — dark circles offset by `pupilX/Y + glanceX/Y`, radius grows when
   `surprised`.
5. **Mouth** — neutral smile arc; `surprised` → small "o".

Details and GDI+ calls live in [Character.cpp.md](Character.cpp.md).

## Callers

- `Renderer.cpp` — `Character::Draw` inside `BeginFrame`/`LockForPresent`.
- `tests` do not test `Character` directly (needs a live GDI+ Graphics); manual
  smoke covers visuals. The math it depends on is covered by `test_math.cpp`.

## Design decisions

- View-only: all animation values come in via `DrawState`; the character knows
  nothing about the cursor or timers.
- Header declares only what's needed; implementation keeps GDI+ includes local
  to the .cpp so headers stay light.
