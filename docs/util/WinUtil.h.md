# src/util/WinUtil.h — RAII / DPI / Memory Helpers

## File role

Declares small, dependency-light utilities shared across modules: GDI+ startup
RAII, working-set trimming, DPI helpers, and coordinate conversion.

## Public API

```cpp
#pragma once
#include <windows.h>

namespace util {

// RAII for GdiplusStartup / GdiplusShutdown. One instance per process, created
// in main.cpp before the App. Enables GDI+ for Renderer.
class ScopeGdiplus {
public:
  ScopeGdiplus();
  ~ScopeGdiplus();
  bool Ok() const noexcept { return ok_; }
private:
  ULONG_PTR token_ = 0;
  bool ok_ = false;
};

// Ask the OS to trim the working set (evict cold, clean pages). Used after
// startup settling and after the menu closes. Returns previous/current sizes
// via out params if non-null. Does NOT shrink hard-working memory.
void TrimWorkingSet(bool force = false) noexcept;

// DPI helpers --------------------------------------------------------------
// Logical→physical pixel scale for a window. v1 uses per-window DPI:
int WindowDpiScale(HWND hwnd) noexcept;        // 96→100%, 192→200% => returns 1/2
int ScaleLogical(int value, int dpiScale) noexcept;

// Convert a client point (our window) to screen coordinates.
void ClientToScreen(HWND hwnd, int& x, int& y) noexcept;

// Clamp a value into [lo, hi].
template <typename T> inline constexpr T Clamp(T v, T lo, T hi) noexcept {
  return (v < lo) ? lo : (v > hi) ? hi : v;
}

// High-frequency tick: guard against dt that would make springs unstable.
constexpr float SanitizeDt(float dtSeconds) noexcept {
  return (dtSeconds > 0.0f && dtSeconds < 1.0f) ? dtSeconds : 0.016f;
}

}  // namespace util
```

## Contract / invariants

- `ScopeGdiplus` must be constructed **before** any `Renderer` and destroyed
  **after** it. Ordering: `main.cpp` holds it for the app's lifetime.
- `TrimWorkingSet`: non-throwing; designed to be called at low-frequency events
  only (startup settle, menu close). **Never** in the 30 Hz tick.
- `Clamp`, `SanitizeDt`: header-only, constexpr, allocation-free — usable in
  hot paths.

## Callers

| Function | Used by |
|---|---|
| `ScopeGdiplus` | `main.cpp` |
| `TrimWorkingSet` | `App.cpp` |
| `WindowDpiScale` / `ScaleLogical` | `App.cpp`, `OverlayWindow.cpp` |
| `ClientToScreen` | `App.cpp` (menu position) |
| `Clamp`, `SanitizeDt` | `Controller.cpp`, `Character.cpp` |

## Design decisions

- Kept free of GDI+ includes in the header: `ScopeGdiplus` hides `ULONG_PTR`
  token (a plain integer) so consumers need no `gdiplus.h`.
- Trim behavior is centralized here (not scattered) so the memory-budget rule
  has exactly one implementation point.
