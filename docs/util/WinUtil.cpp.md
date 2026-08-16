# src/util/WinUtil.cpp — RAII / DPI / Memory Implementation

## File role

Implements GDI+ startup/shutdown, working-set trimming, and DPI helpers.

## ScopeGdiplus

```cpp
#include "util/WinUtil.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")   // or link via CMake (preferred)

namespace util {

ScopeGdiplus::ScopeGdiplus() {
  Gdiplus::GdiplusStartupInput in{};
  ok_ = Gdiplus::GdiplusStartup(&token_, &in, nullptr) == Gdiplus::Ok;
}
ScopeGdiplus::~ScopeGdiplus() {
  if (ok_) Gdiplus::GdiplusShutdown(token_);
}

bool ScopeGdiplus::Ok() const noexcept { return ok_; }
```

- Called once from `main.cpp`. If `Ok()` is false, the app must exit with an
  error (GDI+ is required).
- GDI+ is a system DLL (`gdiplus.dll`) — already mapped in most Windows
  sessions; its attributable working set (~1 MB) is budgeted in
  [memory-budget.md](../memory-budget.md).

## TrimWorkingSet

```cpp
void TrimWorkingSet(bool force) noexcept {
  static bool didTrim = false;
  if (!force && didTrim) return;      // only trim once unless forced
  didTrim = true;

  // Evict cold, clean pages. -1 = set to current size then let the OS page
  // back in on demand.
  ::SetProcessWorkingSetSize(::GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}
```

### When to call (from App.cpp)
1. ~2 s after startup (after the first frames settle).
2. After the context menu closes (`force=true` to allow repeated trims).

### Why it is safe
- It only discards *clean* (unmodified) pages: our code, shared DLL pages,
  cold heap. They reload on demand at negligible cost.
- It is **not** a memory-management hammer — do not call per frame (thrashing
  destroys the budget). See [memory-budget.md](../memory-budget.md) rule 8.

## DPI helpers

```cpp
int WindowDpiScale(HWND hwnd) noexcept {
  // Per-monitor v2 DPI: GetDpiForWindow returns 96/120/144/192...
  const UINT dpi = ::GetDpiForWindow(hwnd);
  return (dpi > 0) ? (dpi + 48) / 96 : 1;    // 96→1, 120→1, 144→1, 192→2
}
```
> Rounding note: 120 dpi (125 %) rounds to 1; a full-scale redesign should use
> floats. For a pet the size jitter at 125 % is acceptable in v1; documented
> limitation.

```cpp
int ScaleLogical(int value, int dpiScale) noexcept { return value * dpiScale; }

void ClientToScreen(HWND hwnd, int& x, int& y) noexcept {
  POINT p{ x, y };
  if (::ClientToScreen(hwnd, &p)) { x = p.x; y = p.y; }
}
```

## Error handling

- `GdiplusStartup` failure → `Ok()==false`; caller exits gracefully.
- DPI helpers return safe defaults (scale 1) on failure — never crash.
- `TrimWorkingSet` may fail harmlessly (returns BOOL, ignored).

## Performance / memory

- Trim only at low-frequency events.
- Helpers are tiny; no allocations.
- `Clamp`/`SanitizeDt` are inline in the header (no code emitted here).

## Testing

- `SanitizeDt`/`Clamp` covered implicitly by `test_math.cpp`.
- GDI+/DPI/trim are smoke-tested manually (multi-DPI: check the character scales
  and re-docks on a 150 % display).

## Related

- Header: [WinUtil.h.md](WinUtil.h.md)
- Consumer: [App.cpp.md](../App.cpp.md), [main.cpp.md](../main.cpp.md)
- Budget rules: [memory-budget.md](../memory-budget.md)
