# src/desktop/OverlayWindow.cpp — Layered Window Implementation

## File role

Implements `OverlayWindow`: window-class registration, creation with the correct
extended styles, `UpdateLayeredWindow` presentation, and the WndProc that
routes clicks.

## Creation

```cpp
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
    // E0308: register may fail if a previous instance left the class; ignore
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
  return true;
}
```

### Extended-style rationale

| Flag | Purpose |
|---|---|
| `WS_EX_LAYERED` | Enables per-pixel alpha + `UpdateLayeredWindow` |
| `WS_EX_TOPMOST` | Stays above normal windows |
| `WS_EX_TOOLWINDOW` | Hidden from Alt-Tab / taskbar buttons |
| `WS_EX_NOACTIVATE` | Never takes keyboard focus when clicked |
| (absent) `WS_EX_TRANSPARENT` | We want clicks on the character, so we keep hit-testing on opaque pixels |

## Presentation

```cpp
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

bool OverlayWindow::EnsureDib(int w, int h) {
  if (dibBmp_ && dibW_ == w && dibH_ == h) return true;
  if (dibBmp_) {                              // size changed → rebuild
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
```

### Notes on the presentation path
- `UpdateLayeredWindow` blends with **premultiplied alpha**. The renderer
  produces a `PixelFormat32bppPARGB` bitmap, so the pixels copied into the DIB
  are already premultiplied — the row copy is pure data movement, no conversion.
- **DIB caching:** the DIB section + memory DC are created once and reused for
  the process lifetime (rebuilt only when the surface size changes, i.e. on
  DPI change). This avoids a per-frame `CreateDIBSection`, which would churn GDI
  memory and violate the memory-budget rule ([memory-budget.md](../memory-budget.md)
  rule 5). The only transient object per present is the screen DC.
- **HDC hygiene:** the memory DC and DIB are owned by `OverlayWindow` and
  released in the destructor (`SelectObject` back, `DeleteObject`, `DeleteDC`);
  the screen DC is released immediately after `UpdateLayeredWindow`. Leaking
  DCs/HBITMAPs is a classic GDI working-set killer — see the failure checklist
  in [memory-budget.md](../memory-budget.md).
- `MoveTo` calls `::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE)`;
  `Present` reads the live position via `GetWindowRect`, so the two never drift.

## Destructor

```cpp
OverlayWindow::~OverlayWindow() {
  if (dibBmp_) {
    ::SelectObject(dibDc_, dibOld_);       // restore before deleting
    ::DeleteObject(dibBmp_);
  }
  if (dibDc_) ::DeleteDC(dibDc_);
  if (hwnd_) ::DestroyWindow(hwnd_);       // window class lives for process lifetime
}
```

> Needs `#include <cstring>` for `std::memcpy` in `Present`.

## Input routing (WndProc)

```cpp
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
```

- All input arrives only on **opaque** pixels (per-pixel hit testing); no manual
  alpha check needed on Win10/11.
- **WM_NCHITTEST is deliberately NOT handled.** On Windows 8+, the system's
  default hit-testing for a layered window considers the alpha channel and
  reports `HTTRANSPARENT` for alpha=0 pixels automatically. Overriding it (e.g.
  forcing `HTCLIENT`) would make the whole 128×160 rectangle block clicks —
  breaking click-through. `DefWindowProcW` handles it correctly.
- `App` receives every message the overlay doesn't consume through the single
  `onMessage` callback (`ctx` = the App instance). See [App.cpp.md](../App.cpp.md)
  for the message table and `App::OnMessage`.

## Error handling

| Condition | Behavior |
|---|---|
| `RegisterClassExW` fails (other than already-exists) | `Create` returns false |
| `CreateWindowExW` fails | returns false; caller logs and exits |
| `Present` with null bits / no hwnd | no-op (never crash) |

## Performance / memory

- `UpdateLayeredWindow` is called only when a frame is dirty (~a few times/s
  when idle-calm, 30×/s max while tracking).
- One 82 KB DIB (the copy target) + one memory DC, created once and reused —
  no per-frame GDI allocation. The per-present cost is a 82 KB `memcpy` + one
  `UpdateLayeredWindow` call; both negligible at these sizes.
- Window is small (128×160); DWM handles the layered surface with negligible
  cost.

## Related

- Header: [OverlayWindow.h.md](OverlayWindow.h.md)
- Renderer producing the buffer: [Renderer.h.md](../character/Renderer.h.md)
- Config: [config.h.md](../config.h.md)
