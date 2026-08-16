#pragma once
#include <windows.h>

namespace Gdiplus { class Graphics; }

namespace character {

struct DrawState;   // defined in Controller.h

class Character {
public:
  Character() = default;

  // Draw one frame of the character centered in the (w × h) surface.
  // g must be a valid GDI+ Graphics with AA enabled (Renderer provides it).
  void Draw(Gdiplus::Graphics& g, const DrawState& st, int w, int h);

  // Logical hit test: is the client point (px, physical px) on the character?
  // Currently unused by the app (the layered window's per-pixel alpha already
  // restricts clicks to opaque character pixels); kept as a defensive helper.
  bool HitTest(float x, float y) const noexcept;
};

}  // namespace character
