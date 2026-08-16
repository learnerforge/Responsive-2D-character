# src/character/Renderer.cpp — GDI+ Surface Implementation

## File role

Implements the GDI+ bitmap lifecycle and the pixel-lock for presentation.

## Setup: GDI+ startup

GDI+ requires `GdiplusStartup` once per process. This is done in
[util/WinUtil.cpp.md](../util/WinUtil.cpp.md) (`ScopeGdiplus`) and called from
`main.cpp` — **not** here — so `Renderer` stays a plain owner.

## EnsureSurface

```cpp
bool Renderer::EnsureSurface(int w, int h) {
  if (bitmap_ && width_ == w && height_ == h) return true;

  // Free old surface first (avoid double-buffer working set).
  Unlock();
  delete graphics_;  graphics_ = nullptr;
  delete bitmap_;    bitmap_ = nullptr;
  width_ = height_ = 0;

  bitmap_ = new Gdiplus::Bitmap(w, h, PixelFormat32bppPARGB);  // premultiplied
  if (!bitmap_ || bitmap_->GetLastStatus() != Gdiplus::Ok) {
    delete bitmap_; bitmap_ = nullptr;
    return false;
  }
  graphics_ = new Gdiplus::Graphics(bitmap_);
  graphics_->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics_->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
  width_ = w; height_ = h;
  return true;
}
```

### Why `PixelFormat32bppPARGB`
- `UpdateLayeredWindow` blends the DIB using alpha as **premultiplied**.
- GDI+ renders into this format directly: every draw (Fill/Ellipse) already
  produces premultiplied pixels, so `LockBits` output is presentable verbatim.
- The alternative (`32bppARGB` straight alpha) would require a per-pixel
  premultiply pass — extra CPU and a scratch buffer. Avoided.

## BeginFrame / LockForPresent / Unlock

```cpp
Gdiplus::Graphics* Renderer::BeginFrame() {
  if (!graphics_) return nullptr;
  graphics_->ResetTransform();               // hop/tilt transforms don't leak
  graphics_->Clear(Gdiplus::Color(0, 0, 0, 0));   // fully transparent bg
  return graphics_;
}

const std::uint8_t* Renderer::LockForPresent() {
  if (!bitmap_) return nullptr;
  Gdiplus::Rect r(0, 0, width_, height_);
  Gdiplus::BitmapData bd{};
  if (bitmap_->LockBits(&r, Gdiplus::ImageLockModeRead,
                        PixelFormat32bppPARGB, &bd) != Gdiplus::Ok)
    return nullptr;
  scan0_ = bd.Scan0;                        // keep until Unlock
  return static_cast<const std::uint8_t*>(scan0_);
}

void Renderer::Unlock() {
  if (bitmap_ && scan0_) {
    bitmap_->UnlockBits(nullptr);
    scan0_ = nullptr;
  }
}
```

### Locking rules
- `LockBits(ImageLockModeRead)` is enough — we only read after drawing.
- The lock must be paired with `UnlockBits` before the **next** `BeginFrame`
  (or `EnsureSurface`), otherwise GDI+ refuses subsequent draws. `App` always
  calls `Unlock` right after `Present`.

## Destructor

```cpp
Renderer::~Renderer() { Unlock(); delete graphics_; delete bitmap_; }
```

## Error handling

| Condition | Behavior |
|---|---|
| `new Bitmap` fails / non-Ok status | return false from `EnsureSurface`; App logs and retries next tick |
| `LockBits` fails | return nullptr; App skips present that frame |
| `graphics_` null | `BeginFrame` returns nullptr; App skips draw |

Never throws; never asserts.

## Performance / memory

- Reuses one bitmap + one Graphics. `Clear` + ~20 draw calls when dirty.
- `LockBits` on a 128×160 surface copies nothing — it maps the internal buffer
  directly (Scan0 points into the bitmap). Cheap.
- Memory: 128 × 160 × 4 = **81,920 bytes** — the single largest heap block and
  well inside the budget ([memory-budget.md](../memory-budget.md)).
- GDI+ object count is constant (1 bitmap, 1 graphics) — no per-frame GDI leak.

## Related

- Header: [Renderer.h.md](Renderer.h.md)
- GDI+ bootstrap: [WinUtil.cpp.md](../util/WinUtil.cpp.md)
- Present: [OverlayWindow.cpp.md](../desktop/OverlayWindow.cpp.md)
- Drawing: [Character.cpp.md](Character.cpp.md)
