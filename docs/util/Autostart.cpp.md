# src/util/Autostart.cpp — Registry Autostart Implementation

## File role

Implements reading/writing the HKCU `Run` key using the Win32 registry API.

## Implementation

```cpp
#include "util/Autostart.h"
#include <windows.h>

namespace util {

constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::wstring Autostart::CurrentExePath() {
  wchar_t buf[MAX_PATH]{};
  const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return (n > 0 && n < MAX_PATH) ? (std::wstring(L"\"") + buf + L"\"")
                                 : std::wstring();
}

bool Autostart::IsEnabled() {
  HKEY key = nullptr;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key)
      != ERROR_SUCCESS)
    return false;

  wchar_t buf[MAX_PATH]{};
  DWORD size = sizeof(buf);
  const LSTATUS st = ::RegGetValueW(key, nullptr, kValueName, RRF_RT_REG_SZ,
                                    nullptr, buf, &size);
  ::RegCloseKey(key);
  return st == ERROR_SUCCESS && size > 0;
}

bool Autostart::SetEnabled(bool enable) {
  HKEY key = nullptr;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr)
      != ERROR_SUCCESS)
    return false;

  LSTATUS st;
  if (enable) {
    const std::wstring path = CurrentExePath();
    st = ::RegSetValueExW(key, kValueName, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(path.c_str()),
                          DWORD((path.size() + 1) * sizeof(wchar_t)));
  } else {
    st = ::RegDeleteValueW(key, kValueName);
    if (st == ERROR_FILE_NOT_FOUND) st = ERROR_SUCCESS;   // already absent
  }
  ::RegCloseKey(key);
  return st == ERROR_SUCCESS;
}

}  // namespace util
```

## API notes

- `RegOpenKeyExW`/`RegCreateKeyExW` with `HKEY_CURRENT_USER` — per-user, no
  elevation needed.
- `RegGetValueW` with `RRF_RT_REG_SZ` ensures we only read a string value
  (avoids type confusion).
- `CurrentExePath` quotes the path for safety with spaces; returns empty on
  failure (then `SetEnabled(true)` writes an empty-ish value — `App` should
  guard: if path empty, treat toggle as failed).
- `MAX_PATH` is sufficient for normal installs; long-path installs
  (`\\?\` or >260 chars) are a known v2 limitation.

## Failure handling

| Scenario | Behavior |
|---|---|
| Run key missing | `IsEnabled=false`; `SetEnabled` creates it |
| Value missing | `IsEnabled=false`; delete returns ERROR_FILE_NOT_FOUND → treated as success |
| Registry access denied (policy) | functions return false; App shows a non-fatal message |
| Path longer than MAX_PATH | `CurrentExePath` returns empty; App reports toggle failed |

## Performance / memory

- Registry calls happen only on menu toggles / startup — off the hot path.
- One `std::wstring` temporary per call — transient, immediately freed. Fine per
  [memory-budget.md](../memory-budget.md) rule 2.

## Testing

- Manual: toggle from the menu → check `reg query
  HKCU\Software\Microsoft\Windows\CurrentVersion\Run`; reboot/relogon optional.
- `IsEnabled` after reboot shows the checkmark in the menu.

## Related

- Header: [Autostart.h.md](Autostart.h.md)
- Menu wiring: [App.cpp.md](../App.cpp.md), [Menu.cpp.md](../ui/Menu.cpp.md)
