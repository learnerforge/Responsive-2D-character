# src/util/Autostart.h — Registry Autostart Contract

## File role

Declares the utility that reads and writes the "run at Windows logon" registry
value (`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`). No UI, no
interaction — pure registry logic.

## Public API

```cpp
#pragma once
#include <string>

namespace util {

class Autostart {
public:
  // Is the autostart value currently present (and pointing at our exe)?
  static bool IsEnabled();

  // Add or remove the autostart value. Returns false on registry failure.
  static bool SetEnabled(bool enable);

  // The path we write: current module path of the running exe (quoted).
  static std::wstring CurrentExePath();

  // Registry value name (matches cfg::kAutostartValueName).
  static constexpr wchar_t kValueName[] = L"Responsive2DCharacter";
};

}  // namespace util
```

## Contract / invariants

- Works for the **current user only** (`HKCU`) — no admin rights, no UAC.
- `IsEnabled` must not throw; returns false on any read failure.
- The value we write is the **quoted** absolute path to the running executable
  (e.g. `"C:\Users\me\dist\r2dc.exe"`), so a path with spaces is valid.
- Writing happens in `App` when the user toggles the menu item — never during
  the tick loop.

## Registry path

```
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
    "Responsive2DCharacter" = "\"...\r2dc.exe\""
```

This is the standard per-user autostart mechanism:
- runs at logon, before/independent of Explorer restarts,
- visible in Task Manager → Startup apps,
- removable by the user at any time (and by our `SetEnabled(false)`).

## Callers

- `ui/Menu` → checkmark state via `App`.
- `App.cpp` — `ToggleAutostart` action.

## Design decisions

- Header uses `std::wstring` (cold path only — see
  [memory-budget.md](../memory-budget.md) rule 2: STL allowed off the hot path).
- No `ShellExecute`/shortcut approaches — the Run key is the smallest, most
  standard solution.
- Static-only class (no instances needed).
