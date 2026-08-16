#pragma once
#include <cstdint>

namespace Gdiplus { class Bitmap; class Graphics; }

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
  Gdiplus::Graphics* BeginFrame();

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
  Gdiplus::Bitmap* bitmap_ = nullptr;
  Gdiplus::Graphics* graphics_ = nullptr;
  void* scan0_ = nullptr;
  int width_ = 0, height_ = 0;
};

}  // namespace character
