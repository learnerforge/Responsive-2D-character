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
// startup settling and after the menu closes. Does NOT shrink hard-working
// memory.
void TrimWorkingSet(bool force = false) noexcept;

// DPI helpers --------------------------------------------------------------
// Logical->physical pixel scale for a window. v1 uses per-window DPI:
int WindowDpiScale(HWND hwnd) noexcept;        // 96->100%, 192->200% => 1/2
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
