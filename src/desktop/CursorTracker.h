#pragma once
#include <windows.h>   // POINT

namespace desktop {

class CursorTracker {
public:
  CursorTracker() = default;

  // Returns true and writes current global cursor coordinates into (x, y).
  // Returns false if the position could not be read (rare: session change,
  // input desktop switch, UAC secure desktop, remote desktop quirks).
  bool GetPosition(int& x, int& y) const noexcept;
};

}  // namespace desktop
