# src/desktop/OverlayWindow.h — Layered Window Contract

## File role

Declares the always-on-top, click-through, transparent window that shows the
character. Owns the HWND and its WndProc wiring, exposes presentation and
input callbacks to `App`.

## Public API

```cpp
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
  // into a memory DC. Recreated only when (w,h) changes — never per frame
  // (memory-budget rule: no per-frame allocations).
  HDC     dibDc_   = nullptr;   // memory DC (created once)
  HBITMAP dibBmp_  = nullptr;   // 32bpp top-down DIB section
  HGDIOBJ dibOld_  = nullptr;   // bitmap previously selected in dibDc_
  void*   dibBits_ = nullptr;   // DIB backing buffer (Scan0 for the copy)
  int     dibW_ = 0, dibH_ = 0;
  bool EnsureDib(int w, int h); // create/resize the DIB, returns ready
};

}  // namespace desktop
```

## Contract / invariants

- Exactly **one** `OverlayWindow` exists, owned by `App`.
- All methods must be called from the UI thread.
- `Present` assumes `w`, `h`, `stride` match a renderer frame of the current
  window size (`kWindowWidth` × `kWindowHeight`, DPI-scaled by `App`).
- The window is created with `WS_POPUP` and extended styles
  `WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`.
  `WS_EX_TRANSPARENT` is **not** set — per-pixel hit testing (Win8+) already
  makes alpha=0 pixels click-through, while opaque character pixels stay
  clickable.
- `WndProc` is static; the instance is recovered from the `GWLP_USERDATA` slot
  that `Create` stores. `App` never touches `GWLP_USERDATA` — it receives
  messages through `onMessage` with its `ctx` pointer instead.
- The overlay window is deliberately **layered-window agnostic** about the app:
  it only calls the callbacks. All app logic (tick, re-anchor, menu) is
  reachable through `onMessage`.

## Presentation contract (per-pixel alpha)

- Pixels with `alpha == 0` → **click-through** (mouse passes to windows below).
- Pixels with `alpha > 0` → **clickable** (character body).
- This is the mechanism that lets the transparent surround coexist with a
  clickable character inside one window.

## Callers

- `App.cpp` — creates it, drives `Present` each dirty tick, calls `MoveTo`
  after re-anchoring, feeds clicks to the controller/menu.

## Design decisions

- The window is a **tight bounding box** around the character (128×160 logical
  px), so most of the surface is the character; no region tricks needed.
- No `WM_PAINT` handling is required for a layered window presented via
  `UpdateLayeredWindow` — the layered surface bypasses normal WM_PAINT
  painting. `WndProc` ignores `WM_PAINT`/`WM_ERASEBKGND` (`DefWindowProc`).
