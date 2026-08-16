#include "ui/Menu.h"

namespace ui {

namespace {
  constexpr UINT kIdAnimate          = 1;
  constexpr UINT kIdToggleAutostart  = 2;
  constexpr UINT kIdSettings         = 3;
  constexpr UINT kIdQuit             = 4;
}

void Menu::SetAutostartChecked(bool checked) noexcept {
  autostartChecked_ = checked;
}

HMENU Menu::Build() {
  HMENU h = CreatePopupMenu();
  if (!h) return nullptr;
  const UINT flagsAuto = MF_STRING | (autostartChecked_ ? MF_CHECKED : 0);
  AppendMenuW(h, MF_STRING, kIdAnimate, L"Animate");
  AppendMenuW(h, flagsAuto, kIdToggleAutostart, L"Auto-start with Windows");
  AppendMenuW(h, MF_STRING, kIdSettings, L"Settings\u2026");
  AppendMenuW(h, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(h, MF_STRING, kIdQuit, L"Quit");
  return h;
}

MenuChoice Menu::Show(HWND owner, int x, int y) {
  HMENU h = Build();
  if (!h) return MenuChoice::None;

  UINT cmd = TrackPopupMenu(h, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                            x, y, 0, owner, nullptr);
  DestroyMenu(h);                      // always destroy — no leak

  switch (cmd) {
    case kIdAnimate:         return MenuChoice::Animate;
    case kIdToggleAutostart: return MenuChoice::ToggleAutostart;
    case kIdSettings:        return MenuChoice::Settings;
    case kIdQuit:            return MenuChoice::Quit;
    default:                 return MenuChoice::None;
  }
}

}  // namespace ui
