#pragma once
#include <windows.h>

namespace desktop {

// Callbacks invoked by the window from the message loop. All on the UI thread.
struct WindowCallbacks {
  // Right/left click on an opaque pixel. pt = client coordinates (physical px).
  void (*onLeftClick)(void* ctx, int x, int y) = nullptr;
  void (*onRightClick)(void* ctx, int x, int y) = nullptr;

  // Called for every message the overlay does not handle itself (WM_TIMER,
  // WM_DISPLAYCHANGE, WM_SETTINGCHANGE, WM_DPICHANGED, WM_CLOSE, ...).
  // Return value is returned to the system. If null, DefWindowProcW is used.
  LRESULT (*onMessage)(void* ctx, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) =
      nullptr;

  void* ctx = nullptr;
};

class OverlayWindow {
public:
  OverlayWindow() = default;

  // Register the window class and create the layered, topmost, no-focus window.
  // hInstance is the process HINSTANCE passed to wWinMain. Returns false on
  // failure.
  bool Create(HINSTANCE hInstance, const WindowCallbacks& cb);

  // Present an ARGB frame. bits must be w*h*4 bytes, premultiplied alpha,
  // top-down (first pixel = top-left). stride = bytes per row.
  // Implemented with UpdateLayeredWindow (per-pixel alpha).
  void Present(const void* bits, int w, int h, int stride);

  // Move/resize the window to screen rect. Preferred over SetWindowPos from App.
  void MoveTo(int x, int y, int w, int h);

  HWND Handle() const noexcept { return hwnd_; }
  bool   Created() const noexcept { return hwnd_ != nullptr; }

  // Window procedure registered with the window class. Routes clicks to the
  // callbacks, every other message to cb_.onMessage, and unhandled messages to
  // DefWindowProcW.
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  ~OverlayWindow();

private:
  HINSTANCE hinst_ = nullptr;
  HWND      hwnd_  = nullptr;
  WindowCallbacks cb_{};

  // Cached presentation DIB: UpdateLayeredWindow needs a DIB section selected
  // into a memory DC. Recreated only when (w,h) changes - never per frame.
  HDC     dibDc_   = nullptr;   // memory DC (created once)
  HBITMAP dibBmp_  = nullptr;   // 32bpp top-down DIB section
  HGDIOBJ dibOld_  = nullptr;   // bitmap previously selected in dibDc_
  void*   dibBits_ = nullptr;   // DIB backing buffer (Scan0 for the copy)
  int     dibW_ = 0, dibH_ = 0;
  bool EnsureDib(int w, int h); // create/resize the DIB, returns ready
};

}  // namespace desktop
