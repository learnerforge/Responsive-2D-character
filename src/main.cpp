#include "App.h"
#include "util/WinUtil.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE,
                    PWSTR, int) {
  // Per-monitor v2 DPI awareness BEFORE any window is created, so coordinates
  // and GetDpiForWindow are accurate on mixed-DPI setups.
  ::SetProcessDpiAwarenessContext(
      DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  util::ScopeGdiplus gdiplus;          // required by Renderer
  if (!gdiplus.Ok()) return 1;         // GDI+ unavailable — cannot render

  {
    App app(hInstance);
    return app.Run();                  // message loop until WM_QUIT
  }  // app destroyed before GDI+ shutdown (object-lifetime ordering)
}      // gdiplus destroyed last
