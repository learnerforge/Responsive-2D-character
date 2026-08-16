#pragma once
#include <windows.h>

namespace ui {

enum class MenuChoice {
  None,          // dismissed / no selection
  Animate,       // manually trigger the hop reaction
  ToggleAutostart,
  Settings,      // placeholder dialog in v1
  Quit,
};

class Menu {
public:
  Menu() = default;

  // Show the popup anchored at (screenX, screenY). Blocks until the menu is
  // dismissed (TrackPopupMenu is modal). Returns the selected choice.
  MenuChoice Show(HWND owner, int screenX, int screenY);

  // Whether the autostart entry currently shows a checkmark. Set by App before
  // Show() (queried from Autostart::IsEnabled()).
  void SetAutostartChecked(bool checked) noexcept;

private:
  HMENU Build();
  bool autostartChecked_ = false;
};

}  // namespace ui
