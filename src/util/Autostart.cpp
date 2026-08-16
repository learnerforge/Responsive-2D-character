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
    if (path.empty()) {                       // cannot resolve our own path
      ::RegCloseKey(key);
      return false;
    }
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
