#include "App.h"
#include "config.h"
#include "util/WinUtil.h"
#include "util/Autostart.h"
#include <windows.h>
#include <chrono>

namespace {
  constexpr UINT_PTR kTickTimerId = 1;    // 30 Hz simulation timer
  constexpr UINT_PTR kTrimTimerId = 2;    // one-shot working-set trim after settle

  long long steady_clock_ms() noexcept {  // monotonic clock in ms
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
        .count();
  }
}

App::App(HINSTANCE hInstance) : hinst_(hInstance) {}

App::~App() = default;

bool App::CreateOverlay() {
  desktop::WindowCallbacks cb;
  cb.ctx          = this;
  cb.onLeftClick  = [](void* c, int x, int y) { static_cast<App*>(c)->OnLeftClick(x, y); };
  cb.onRightClick = [](void* c, int x, int y) { static_cast<App*>(c)->OnRightClick(x, y); };
  cb.onMessage    = [](void* c, HWND h, UINT m, WPARAM w, LPARAM l) {
                      return static_cast<App*>(c)->OnMessage(h, m, w, l);
                    };

  if (!window_.Create(hinst_, cb)) return false;
  hwnd_ = window_.Handle();

  controller_.SetCursorSource(&cursorSource_);   // adapter over cursor_

  // 30 Hz tick timer.
  ::SetTimer(hwnd_, kTickTimerId, 1000 / cfg::kTickHz, nullptr);
  return true;
}

LRESULT App::OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_TIMER:
      if (wp == kTickTimerId) { OnTick(); return 0; }
      if (wp == kTrimTimerId) {
        ::KillTimer(hwnd, kTrimTimerId);       // one-shot
        util::TrimWorkingSet(false);
        return 0;
      }
      break;
    case WM_DISPLAYCHANGE:  OnDisplayChange();  return 0;  // monitor changed
    case WM_SETTINGCHANGE:  OnSettingChange();  return 0;  // taskbar moved/auto-hide
    case WM_DPICHANGED:     OnDpiChanged(LOWORD(wp)); return 0;  // new DPI
    case WM_CLOSE:          ::PostQuitMessage(0);       return 0;  // clean exit
  }
  return ::DefWindowProcW(hwnd, msg, wp, lp);
}

void App::OnDisplayChange() {
  reanchorPending_ = true;
}

void App::OnSettingChange() {
  reanchorPending_ = true;
}

void App::OnDpiChanged(int /*newDpi*/) {
  // Surface size is recomputed from the current DPI scale on every present;
  // re-dock so the pet stays glued to the taskbar.
  reanchorPending_ = true;
}

void App::OnTick() {
  const long long now = steady_clock_ms();
  controller_.Tick(now);

  if (reanchorPending_) Reanchor();             // cheap, only on system events
  if (controller_.WantsRepaint()) PresentFrame();
}

void App::PresentFrame() {
  const int scale = util::WindowDpiScale(hwnd_);
  const int w = cfg::kWindowWidth  * scale;
  const int h = cfg::kWindowHeight * scale;

  if (!renderer_.EnsureSurface(w, h)) return;   // surface ready (lazy, once)
  if (auto* g = renderer_.BeginFrame()) {
    character_.Draw(*g, controller_.Current(), w, h);
    if (const auto* bits = renderer_.LockForPresent()) {
      window_.Present(bits, w, h, renderer_.Stride());
      renderer_.Unlock();
    }
  }
  controller_.ClearRepaint();
}

void App::Reanchor() {
  const int scale = util::WindowDpiScale(hwnd_);
  const int w = cfg::kWindowWidth  * scale;
  const int h = cfg::kWindowHeight * scale;

  const auto info = taskbar_.Locate();
  const auto dock = taskbar_.DockBottom(info, w, h);
  window_.MoveTo(dock.x, dock.y, dock.w, dock.h);

  controller_.SetWindowCenter(dock.x + w / 2, dock.y + h / 2);
  reanchorPending_ = false;
  lastW_ = w; lastH_ = h;
}

void App::OnLeftClick(int, int) {
  controller_.OnLeftClick();
}

void App::OnRightClick(int clientX, int clientY) {
  int sx = clientX, sy = clientY;
  util::ClientToScreen(hwnd_, sx, sy);          // client → screen
  menu_.SetAutostartChecked(util::Autostart::IsEnabled());
  const auto choice = menu_.Show(hwnd_, sx, sy);
  HandleMenuChoice(choice);
  util::TrimWorkingSet(true);                    // shed menu pages
}

void App::HandleMenuChoice(ui::MenuChoice choice) {
  switch (choice) {
    case ui::MenuChoice::Animate:
      controller_.OnLeftClick();                 // reuse the hop reaction
      break;
    case ui::MenuChoice::ToggleAutostart:
      util::Autostart::SetEnabled(!util::Autostart::IsEnabled());
      break;
    case ui::MenuChoice::Settings:
      ::MessageBoxW(hwnd_, L"Settings dialog coming in v2.", cfg::kAppName,
                    MB_OK | MB_ICONINFORMATION);
      break;
    case ui::MenuChoice::Quit:
      ::PostQuitMessage(0);
      break;
    case ui::MenuChoice::None:
    default:
      break;
  }
}

int App::Run() {
  if (!CreateOverlay()) return 2;
  Reanchor();                        // initial dock before first tick
  ::SetTimer(hwnd_, kTrimTimerId, 2000, nullptr);   // one-shot settle trim

  MSG msg{};
  while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);        // messages go to OverlayWindow::WndProc
  }
  return (int)msg.wParam;
}
