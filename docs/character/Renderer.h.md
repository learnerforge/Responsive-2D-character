# src/character/Renderer.h — GDI+ Surface Contract

## File role

Declares the class that owns the off-screen GDI+ bitmap, lets the character
draw onto it, and exposes the raw premultiplied-ARGB pixels for the overlay
window to present. The only module that talks to GDI+.

## Public API

```cpp
#pragma once
#include <cstdint>

namespace gdiplus { class Bitmap; class Graphics; }

namespace character {

class Renderer {
public:
  Renderer() noexcept = default;
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  // Lazily create the w×h surface (32bpp premultiplied ARGB). Must be called
  // once before drawing; re-calls with a different size recreate the surface.
  // Returns false only on catastrophic GDI+ failure.
  bool EnsureSurface(int w, int h);

  // Begin a frame: returns a GDI+ Graphics* ready for Character::Draw.
  // nullptr if the surface is not ready. The graphics is reused across frames.
  gdiplus::Graphics* BeginFrame();

  // Finish the frame: returns a pointer to the locked pixel buffer
  // (premultiplied ARGB, top-down, stride = Width()*4) or nullptr on failure.
  // The lock is held until Present(); call Unlock() right after presenting.
  const std::uint8_t* LockForPresent();

  // Release the pixel lock. Call after OverlayWindow::Present.
  void Unlock();

  int  Width() const noexcept { return width_; }
  int  Height() const noexcept { return height_; }
  int  Stride() const noexcept { return width_ * 4; }
  bool Ready() const noexcept { return bitmap_ != nullptr; }

private:
  gdiplus::Bitmap* bitmap_ = nullptr;
  gdiplus::Graphics* graphics_ = nullptr;
  void* scan0_ = nullptr;
  int width_ = 0, height_ = 0;
};

}  // namespace character
```

## Contract / invariants

- **One surface, reused forever** — the single 128×160 (DPI-scaled) buffer is
  the process's main heap allocation; never allocate a bitmap per frame (this is
  a memory-budget rule, [memory-budget.md](../memory-budget.md)).
- `EnsureSurface` creates the bitmap with `PixelFormat32bppPARGB`
  (premultiplied alpha) so the pixels are already in the exact format
  `UpdateLayeredWindow` expects — no conversion pass, no scratch buffer.
- `BeginFrame` → `Character::Draw` → `LockForPresent` → `OverlayWindow::Present`
  → `Unlock` is the canonical per-frame sequence, driven by `App` (see
  [App.cpp.md](../App.cpp.md)).
- No thread-safety — UI thread only.

## Callers

- `App.cpp` — owns the sequence, re-creates the surface when DPI changes.
- `Character.cpp` — draws through the `Graphics*` handed out by `BeginFrame`.

## Design decisions

- The Graphics object is created **once** on the bitmap and reused, so GDI+
  internal state (AA mode, transform stack) persists and we never leak native
  objects per frame.
- `LockForPresent`/`Unlock` pair avoids keeping a `BitmapData` alive forever;
  the lock is short-lived (only across the present call).
