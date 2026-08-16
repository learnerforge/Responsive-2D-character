#pragma once
#include "desktop/OverlayWindow.h"
#include "desktop/CursorTracker.h"
#include "desktop/TaskbarLocator.h"
#include "character/Controller.h"
#include "character/Character.h"
#include "character/Renderer.h"
#include "ui/Menu.h"

// Bridge: presents desktop::CursorTracker to character::ICursorSource. Keeps the
// layering clean (desktop never depends on character).
struct CursorSourceAdapter : character::ICursorSource {
  desktop::CursorTracker& tracker;
  explicit CursorSourceAdapter(desktop::CursorTracker& t) noexcept : tracker(t) {}
  bool Get(int& x, int& y) override { return tracker.GetPosition(x, y); }
};

class App {
public:
  explicit App(HINSTANCE hInstance);
  ~App();

  // Runs the message loop until WM_QUIT. Returns the exit code.
  int Run();

private:
  bool CreateOverlay();          // owns OverlayWindow::Create + SetTimer
  // Called by OverlayWindow::WndProc for every message it does not consume.
  LRESULT OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  void OnTick();                  // WM_TIMER: controller → render → present
  void OnLeftClick(int x, int y);
  void OnRightClick(int x, int y);
  void OnDisplayChange();
  void OnSettingChange();
  void OnDpiChanged(int newDpi);
  void HandleMenuChoice(ui::MenuChoice choice);
  void Reanchor();                // taskbar locator → MoveTo + SetWindowCenter
  void PresentFrame();            // renderer → overlay window

  // Subsystems (composition, declared in dependency order).
  desktop::CursorTracker   cursor_;
  CursorSourceAdapter      cursorSource_{cursor_};
  desktop::TaskbarLocator  taskbar_;
  desktop::OverlayWindow   window_;
  character::Controller    controller_;
  character::Character     character_;
  character::Renderer      renderer_;
  ui::Menu                 menu_;

  HINSTANCE hinst_ = nullptr;
  HWND      hwnd_   = nullptr;
  bool      reanchorPending_ = false;
  int       lastW_ = 0, lastH_ = 0;
};
