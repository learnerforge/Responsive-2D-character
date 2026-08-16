#include "util/WinUtil.h"
#include <wtypes.h>     // mingw-w64: declares PROPID used by gdiplus headers
#include <gdiplus.h>

namespace util {

ScopeGdiplus::ScopeGdiplus() {
  Gdiplus::GdiplusStartupInput in{};
  ok_ = Gdiplus::GdiplusStartup(&token_, &in, nullptr) == Gdiplus::Ok;
}
ScopeGdiplus::~ScopeGdiplus() {
  if (ok_) Gdiplus::GdiplusShutdown(token_);
}

void TrimWorkingSet(bool force) noexcept {
  static bool didTrim = false;
  if (!force && didTrim) return;      // only trim once unless forced
  didTrim = true;

  // Evict cold, clean pages. -1 = set to current size then let the OS page
  // back in on demand.
  ::SetProcessWorkingSetSize(::GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

int WindowDpiScale(HWND hwnd) noexcept {
  // Per-monitor v2 DPI: GetDpiForWindow returns 96/120/144/192...
  // Resolve via GetProcAddress so the build is not dependent on a specific
  // SDK/toolchain declaring it; falls back to the primary monitor DPI.
  using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
  static const auto fn = reinterpret_cast<GetDpiForWindowFn>(
      ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));

  UINT dpi = 0;
  if (fn) {
    dpi = fn(hwnd);
  } else {
    HDC dc = ::GetDC(hwnd);
    dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX)) : 96;
    if (dc) ::ReleaseDC(hwnd, dc);
  }
  return (dpi > 0) ? (static_cast<int>(dpi) + 48) / 96 : 1;   // 96->1, 144->2
}

int ScaleLogical(int value, int dpiScale) noexcept { return value * dpiScale; }

void ClientToScreen(HWND hwnd, int& x, int& y) noexcept {
  POINT p{ x, y };
  if (::ClientToScreen(hwnd, &p)) { x = p.x; y = p.y; }
}

}  // namespace util
