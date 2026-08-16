#include "desktop/OverlayWindow.h"
#include "config.h"
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include <cstring>

namespace desktop {

bool OverlayWindow::Create(HINSTANCE hInstance, const WindowCallbacks& cb) {
  hinst_ = hInstance;
  cb_ = cb;

  WNDCLASSEXW wc{};
  wc.cbSize        = sizeof(wc);
  wc.lpfnWndProc   = &OverlayWindow::WndProc;
  wc.hInstance     = hinst_;
  wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
  wc.lpszClassName = cfg::kWindowClass;
  if (!::RegisterClassExW(&wc)) {
    // Register may fail if a previous instance left the class; ignore
    // ERROR_CLASS_ALREADY_EXISTS and continue.
    if (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  }

  hwnd_ = ::CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      cfg::kWindowClass, cfg::kAppName,
      WS_POPUP,
      0, 0, 1, 1,                 // size set by App via MoveTo
      nullptr, nullptr, hinst_, nullptr);

  if (!hwnd_) return false;
  ::SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  ::ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  return true;
}

bool OverlayWindow::EnsureDib(int w, int h) {
  if (dibBmp_ && dibW_ == w && dibH_ == h) return true;
  if (dibBmp_) {                              // size changed -> rebuild
    ::SelectObject(dibDc_, dibOld_);
    ::DeleteObject(dibBmp_);  dibBmp_ = nullptr;
  }
  if (!dibDc_) dibDc_ = ::CreateCompatibleDC(nullptr);
  if (!dibDc_) return false;

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth       = w;
  bmi.bmiHeader.biHeight      = -h;            // negative = top-down DIB
  bmi.bmiHeader.biPlanes      = 1;
  bmi.bmiHeader.biBitCount    = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  dibBmp_ = ::CreateDIBSection(dibDc_, &bmi, DIB_RGB_COLORS, &dibBits_, nullptr, 0);
  if (!dibBmp_) { dibBits_ = nullptr; return false; }
  dibOld_ = ::SelectObject(dibDc_, dibBmp_);
  dibW_ = w; dibH_ = h;
  return true;
}

void OverlayWindow::Present(const void* bits, int w, int h, int stride) {
  if (!hwnd_ || !bits || !EnsureDib(w, h)) return;

  // Copy premultiplied ARGB into the DIB (top-down, both 32bpp).
  // Formats match 1:1, so a row-wise copy is exact.
  const size_t row = size_t(w) * 4;
  const char* src = static_cast<const char*>(bits);
  char* dst = static_cast<char*>(dibBits_);
  for (int y = 0; y < h; ++y)
    std::memcpy(dst + y * row, src + y * stride, row);

  RECT rc{};  ::GetWindowRect(hwnd_, &rc);          // current position
  POINT pt{ rc.left, rc.top };
  SIZE  sz{ w, h };
  POINT srcP{ 0, 0 };
  BLENDFUNCTION bf{};
  bf.BlendOp = AC_SRC_OVER;
  bf.SourceConstantAlpha = 255;
  bf.AlphaFormat = AC_SRC_ALPHA;

  HDC screenDc = ::GetDC(nullptr);
  ::UpdateLayeredWindow(hwnd_, screenDc, &pt, &sz, dibDc_, &srcP, 0, &bf, ULW_ALPHA);
  ::ReleaseDC(nullptr, screenDc);
}

void OverlayWindow::MoveTo(int x, int y, int w, int h) {
  if (!hwnd_) return;
  ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<OverlayWindow*>(
      ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (self && self->cb_.ctx) {
    switch (msg) {
      case WM_LBUTTONDOWN:
        if (self->cb_.onLeftClick)
          self->cb_.onLeftClick(self->cb_.ctx,
                                GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
      case WM_RBUTTONDOWN:
        if (self->cb_.onRightClick)
          self->cb_.onRightClick(self->cb_.ctx,
                                 GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
      default:
        // System messages (WM_TIMER, WM_DISPLAYCHANGE, WM_SETTINGCHANGE,
        // WM_DPICHANGED, WM_CLOSE, ...) go to the app via onMessage.
        if (self->cb_.onMessage)
          return self->cb_.onMessage(self->cb_.ctx, hwnd, msg, wp, lp);
        break;
    }
  }
  return ::DefWindowProcW(hwnd, msg, wp, lp);
}

OverlayWindow::~OverlayWindow() {
  if (dibBmp_) {
    ::SelectObject(dibDc_, dibOld_);       // restore before deleting
    ::DeleteObject(dibBmp_);
  }
  if (dibDc_) ::DeleteDC(dibDc_);
  if (hwnd_) { ::DestroyWindow(hwnd_); hwnd_ = nullptr; }
}

}  // namespace desktop
