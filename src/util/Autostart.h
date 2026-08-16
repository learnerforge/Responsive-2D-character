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
