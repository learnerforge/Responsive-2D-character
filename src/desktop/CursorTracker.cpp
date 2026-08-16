#include "desktop/CursorTracker.h"

namespace desktop {

bool CursorTracker::GetPosition(int& x, int& y) const noexcept {
  POINT pt{};
  if (!::GetCursorPos(&pt)) return false;
  x = pt.x;
  y = pt.y;
  return true;
}

}  // namespace desktop
