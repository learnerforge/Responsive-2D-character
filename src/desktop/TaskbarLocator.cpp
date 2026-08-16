#include "desktop/TaskbarLocator.h"
#include "config.h"
#include <shellapi.h>

namespace desktop {

TaskbarInfo TaskbarLocator::Locate() const {
  TaskbarInfo out{};
  APPBARDATA abd{};
  abd.cbSize = sizeof(abd);

  // Current edge + rect.
  if (::SHAppBarMessage(ABM_GETTASKBARPOS, &abd) == 0) {
    out.visible = false;
    return out;                      // fall back to work area
  }
  switch (abd.uEdge) {
    case ABE_LEFT:   out.edge = TaskbarEdge::Left;   break;
    case ABE_TOP:    out.edge = TaskbarEdge::Top;    break;
    case ABE_RIGHT:  out.edge = TaskbarEdge::Right;  break;
    case ABE_BOTTOM: out.edge = TaskbarEdge::Bottom; break;
    default:         out.edge = TaskbarEdge::Unknown;
  }
  out.rect = abd.rc;

  // Auto-hide state.
  APPBARDATA state{};
  state.cbSize = sizeof(state);
  if (::SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE)
    out.autoHidden = true;

  return out;
}

RECT TaskbarLocator::WorkArea() const {
  RECT wa{};
  ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);   // primary monitor
  return wa;
}

DockRect TaskbarLocator::DockBottom(const TaskbarInfo& info,
                                    int charW, int charH) const {
  DockRect d;
  d.w = charW; d.h = charH;
  RECT wa = WorkArea();

  if (info.visible && info.edge == TaskbarEdge::Bottom && !info.autoHidden) {
    // Feet on the taskbar top edge.
    d.x = wa.left + cfg::kAnchorX;                 // 64 px from work-area left
    d.y = info.rect.top - charH - cfg::kAnchorGap; // 2 px above the taskbar
  } else {
    // Auto-hidden / unknown: sit at the bottom of the work area.
    d.x = wa.left + cfg::kAnchorX;
    d.y = wa.bottom - charH - cfg::kAnchorGap;
  }
  return d;
}

}  // namespace desktop
