#pragma once
#include <windows.h>

namespace desktop {

enum class TaskbarEdge { Left, Top, Right, Bottom, Unknown };

struct TaskbarInfo {
  TaskbarEdge edge = TaskbarEdge::Unknown;  // which screen edge the taskbar is on
  RECT        rect{};                       // taskbar bounding rect (screen coords)
  bool        autoHidden = false;           // true if taskbar auto-hides
  bool        visible    = true;            // false if taskbar cannot be read at all
};

// Where the character (size w x h logical px, DPI-scaled) should be placed.
struct DockRect {
  int x = 0, y = 0, w = 0, h = 0;           // screen coords, physical px
};

class TaskbarLocator {
public:
  TaskbarLocator() = default;

  // Query the taskbar via SHAppBarMessage. Never throws; on any failure returns
  // TaskbarInfo with visible=false (caller falls back to the work area).
  TaskbarInfo Locate() const;

  // The work area (screen minus taskbar) for the monitor containing the taskbar.
  // Fallback used when Locate() fails or when the taskbar is auto-hidden.
  RECT WorkArea() const;

  // Compute the anchored window rect for a bottom-edge taskbar.
  // characterW/H are DPI-scaled physical pixel dimensions.
  DockRect DockBottom(const TaskbarInfo& info, int characterW, int characterH) const;
};

}  // namespace desktop
