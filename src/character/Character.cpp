#include "character/Character.h"
#include "character/Controller.h"     // DrawState
#include <wtypes.h>     // mingw-w64: declares PROPID used by gdiplus headers
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
  {
    SolidBrush brush(Color(255, 240, 220, 200));
    Pen outline(Color(200, 180, 150, 140), 1.5f);
    g.FillEllipse(&brush, body);
    g.DrawEllipse(&outline, body);
  }

  // ---- 3. Head + face under one rotation about the neck ---------------------
  GraphicsState saved = g.Save();
  g.TranslateTransform(cx + counter, neckY);
  g.RotateTransform(st.headTilt * 180.0f / 3.14159f);
  g.TranslateTransform(-(cx + counter), -neckY);

  RectF head(cx + counter - kHeadR, neckY - kHeadR - 16.0f,
             2 * kHeadR, 2 * kHeadR);
  {
    SolidBrush skin(Color(255, 255, 215, 190));
    Pen outline(Color(200, 200, 150, 140), 1.5f);
    g.FillEllipse(&skin, head);
    g.DrawEllipse(&outline, head);
  }

  const float ex = cx + counter, ey = neckY - 26.0f;   // face center

  // Eyes (blink = vertical scale; clamp to avoid zero-height ellipse).
  const float blinkY = std::max(st.blink, 0.05f);
  RectF eyeL(ex - kEyeX, ey - kEyeY, 2 * kEyeRx, 2 * kEyeRy * blinkY);
  RectF eyeR(ex + kEyeX - 2 * kEyeRx, ey - kEyeY, 2 * kEyeRx, 2 * kEyeRy * blinkY);
  {
    SolidBrush white(Color(255, 255, 255, 255));
    g.FillEllipse(&white, eyeL);
    g.FillEllipse(&white, eyeR);
  }

  // Pupils (pupil + glance; grow on surprise; clamp inside the eye).
  const float pr = st.surprised ? kPupilSurprisedR : kPupilR;
  float px = st.pupilX + st.glanceX;
  float py = st.pupilY + st.glanceY;
  const float maxD = kEyeRx - pr + 1.0f;
  const float d = std::hypotf(px, py);
  if (d > maxD) { px *= maxD / d; py *= maxD / d; }

  {
    SolidBrush pupil(Color(255, 40, 30, 30));
    g.FillEllipse(&pupil, RectF(ex - kEyeX + px - pr, ey + py - pr, 2 * pr, 2 * pr));
    g.FillEllipse(&pupil, RectF(ex + kEyeX + px - pr, ey + py - pr, 2 * pr, 2 * pr));
  }
  {
    SolidBrush highlight(Color(220, 255, 255, 255));
    g.FillEllipse(&highlight,
                  RectF(ex - kEyeX + px - 1.2f, ey + py - 1.2f, 2.4f, 2.4f));
  }

  // Mouth: surprised "o" or a smile arc.
  if (st.surprised) {
    Pen mouth(Color(255, 80, 50, 50), 2.0f);
    g.DrawEllipse(&mouth, RectF(ex - 4, ey + 10, 8, 9));
  } else {
    Pen mouth(Color(255, 80, 50, 50), 2.0f);
    g.DrawArc(&mouth, RectF(ex - 8, ey + 6, 16, 12), 200.0f, 140.0f);
  }

  g.Restore(saved);     // undo head rotation + hop for the next frame
}

bool Character::HitTest(float x, float y) const noexcept {
  // Conservative: inside head disc (radius + 2 px) or the body ellipse.
  // Uses the un-scaled geometry; App passes DPI-scaled coordinates.
  const float hx = 0.0f, hy = 0.0f;               // head center (origin)
  const float dx = x - hx, dy = y - hy;
  if (dx * dx + dy * dy <= (kHeadR + 2.0f) * (kHeadR + 2.0f)) return true;

  // Body ellipse roughly at (0, neck offset), semi-axes kBodyW/2, kBodyH/2.
  const float bx = x, by = y - (kHeadR * 0.5f + 8.0f);
  const float ex = bx / (kBodyW / 2 + 2.0f), ey = by / (kBodyH / 2 + 2.0f);
  return ex * ex + ey * ey <= 1.0f;
}

}  // namespace character
