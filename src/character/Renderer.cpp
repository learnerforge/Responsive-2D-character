#include "character/Renderer.h"
#include <wtypes.h>     // mingw-w64: declares PROPID used by gdiplus headers
#include <gdiplus.h>

namespace character {

Renderer::~Renderer() {
  Unlock();
  delete graphics_;
  delete bitmap_;
}

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

}  // namespace character
