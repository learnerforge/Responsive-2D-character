# src/character/Character.cpp — Vector-Art Implementation

## File role

Implements the GDI+ drawing of the character from a `DrawState`.

## Setup per draw

```cpp
#include "character/Character.h"
#include "character/Controller.h"     // DrawState
#include <gdiplus.h>
#include <algorithm>                  // std::max
#include <cmath>                      // hypotf

using namespace Gdiplus;

namespace character {

namespace {
  constexpr float kHeadR = 26.0f;       // head radius (logical px)
  constexpr float kEyeX  = 10.0f;       // eye center x offset from head center
  constexpr float kEyeY  = 6.0f;        // eye center y offset (down)
  constexpr float kEyeRx = 7.0f, kEyeRy = 8.0f;
  constexpr float kPupilR = 4.0f, kPupilSurprisedR = 5.5f;
  constexpr float kBodyW = 34.0f, kBodyH = 30.0f;
}
```

## Draw (coordinate model)

Surface is `w × h` logical px; character center is `(w/2, h/2)`. All offsets are
measured from the **body center** `(cx, cy)`; the head pivots around the **neck**
point `neckY` below the head center.

Transform order (important — see [architecture.md](../architecture.md) for the
GDI+ transform rules):

1. **Hop** — translate the whole world by `-st.hopY` (everything moves up).
2. **Body** — drawn in world space, leans opposite to head tilt (`counter`),
   breathes via `sway`.
3. **Head + face** — one rotation transform about the neck, then head ellipse,
   eyes, pupils, and mouth are all drawn inside it so the **whole face tilts
   together** with the head.

```cpp
void Character::Draw(Graphics& g, const DrawState& st, int w, int h) {
  // 1. Hop: move the whole character up by st.hopY (screen y grows downward).
  if (st.hopY != 0.0f) g.TranslateTransform(0.0f, -st.hopY);

  const float cx = w / 2.0f, cy = h / 2.0f + 6.0f;   // body center
  const float neckY = cy + 18.0f;                    // pivot below head
  const float sway    = st.breath;                   // breath scale, ±1.6 px
  const float counter = -st.headTilt * 8.0f;         // body leans opposite

  // ---- 2. Body -------------------------------------------------------------
  RectF body(cx - kBodyW / 2 + counter, neckY,
             kBodyW + sway, kBodyH - sway * 0.5f);
  g.FillEllipse(&SolidBrush(Color(255, 240, 220, 200)), body);
  g.DrawEllipse(&Pen(Color(200, 180, 150, 140), 1.5f), body);

  // ---- 3. Head + face under one rotation about the neck ---------------------
  GraphicsState saved = g.Save();
  g.TranslateTransform(cx + counter, neckY);
  g.RotateTransform(st.headTilt * 180.0f / 3.14159f);
  g.TranslateTransform(-(cx + counter), -neckY);

  RectF head(cx + counter - kHeadR, neckY - kHeadR - 16.0f,
             2 * kHeadR, 2 * kHeadR);
  g.FillEllipse(&SolidBrush(Color(255, 255, 215, 190)), head);   // skin
  g.DrawEllipse(&Pen(Color(200, 200, 150, 140), 1.5f), head);    // outline

  const float ex = cx + counter, ey = neckY - 26.0f;   // face center

  // Eyes (blink = vertical scale; clamp to avoid zero-height ellipse).
  const float blinkY = std::max(st.blink, 0.05f);
  RectF eyeL(ex - kEyeX, ey - kEyeY, 2 * kEyeRx, 2 * kEyeRy * blinkY);
  RectF eyeR(ex + kEyeX - 2 * kEyeRx, ey - kEyeY, 2 * kEyeRx, 2 * kEyeRy * blinkY);
  g.FillEllipse(&SolidBrush(Color(255, 255, 255, 255)), eyeL);
  g.FillEllipse(&SolidBrush(Color(255, 255, 255, 255)), eyeR);

  // Pupils (pupil + glance; grow on surprise; clamp inside the eye).
  const float pr = st.surprised ? kPupilSurprisedR : kPupilR;
  float px = st.pupilX + st.glanceX;
  float py = st.pupilY + st.glanceY;
  const float maxD = kEyeRx - pr + 1.0f;
  float d = hypotf(px, py); if (d > maxD) { px *= maxD / d; py *= maxD / d; }

  g.FillEllipse(&SolidBrush(Color(255, 40, 30, 30)),
                RectF(ex - kEyeX + px - pr, ey + py - pr, 2 * pr, 2 * pr));
  g.FillEllipse(&SolidBrush(Color(255, 40, 30, 30)),
                RectF(ex + kEyeX + px - pr, ey + py - pr, 2 * pr, 2 * pr));
  g.FillEllipse(&SolidBrush(Color(220, 255, 255, 255)),        // highlight
                RectF(ex - kEyeX + px - 1.2f, ey + py - 1.2f, 2.4f, 2.4f));

  // Mouth: surprised "o" or a smile arc.
  if (st.surprised) {
    g.DrawEllipse(&Pen(Color(255, 80, 50, 50), 2.0f),
                  RectF(ex - 4, ey + 10, 8, 9));
  } else {
    g.DrawArc(&Pen(Color(255, 80, 50, 50), 2.0f),
              RectF(ex - 8, ey + 6, 16, 12), 200.0f, 140.0f);
  }

  g.Restore(saved);     // undo head rotation + hop for the next frame
}
```

> The `Restore` also unwinds the hop translate, so `Draw` leaves the Graphics
> transform as it found it (idempotence contract in
> [Character.h.md](Character.h.md)).

## HitTest

```cpp
bool Character::HitTest(float x, float y) const noexcept {
  // conservative: inside head circle or body ellipse
  // (uses un-scaled geometry; App passes DPI-scaled coords)
  return /* inside head disc radius kHeadR+2  OR  inside body ellipse */;
}
```
Because the overlay window's opaque pixels are already the only clickable area
(per-pixel alpha), `HitTest` is **defensive only** — it may err on the side of
"true".

## Rendering quality notes

- `Renderer` enables `SmoothingModeAntiAlias` before calling `Draw`; all shapes
  are anti-aliased by GDI+.
- Colors are hard-coded here (skin `(255,255,215,190)`, outline
  `(200,200,150,140)`, pupil `(40,30,30)`, mouth `(80,50,50)`). Future themes
  would move these to `config.h`.
- Every `RectF`/`SolidBrush`/`Pen` is a stack temporary — GDI+ objects created
  on the fly are fine because GDI+ handles their native objects internally;
  they add no persistent heap in our process.

## Performance / memory

- ~20 GDI+ draw calls per frame, only when dirty.
- No member state; no allocation beyond GDI+ internals.

## Testing

- Visual smoke only (see [overview.md](../overview.md)). Math it depends on
  (pupil clamp, angle→tilt) is in `test_math.cpp`.
- Manual check: blink looks symmetric; head tilt pivots at neck; surprise
  enlarges pupils; hop is a clean parabola.

## Related

- Header: [Character.h.md](Character.h.md)
- `DrawState`: [Controller.h.md](Controller.h.md)
- GDI+ surface: [Renderer.h.md](Renderer.h.md)
