# src/main.cpp — Application Entry Point

## File role

The `wWinMain` entry point: sets process DPI awareness, starts GDI+, creates the
`App`, runs the message loop, and unwinds cleanly. The only file that may
contain a `wWinMain` definition.

## Implementation

```cpp
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
```

## Responsibilities (in order)

1. **DPI awareness** — must be the very first thing; any window created before
   this gets virtualized coordinates. See [WinUtil.cpp.md](util/WinUtil.cpp.md).
   - Requires `_WIN32_WINNT=0x0A00` (set in [CMakeLists.txt.md](CMakeLists.txt.md))
     and Windows 10 1703+ for `SetProcessDpiAwarenessContext`. If support for
     older Windows were ever needed, fall back to
     `SetProcessDPIAware()` via `GetProcAddress` (documented v1.1 change).
2. **GDI+ startup** — `ScopeGdiplus` lives for the whole process; `App` is
   destroyed *before* it, so `Renderer` (which needs GDI+) is always torn down
   first.
3. **App lifetime** — `App` owns all subsystems (window, controller, renderer).
4. **Return code** — `0` clean quit, `1` GDI+ failure, `App::Run`'s own return
   otherwise.

## Why `wWinMain` (not `WinMain` / `main`)

- GUI subsystem (`/SUBSYSTEM:WINDOWS`) requires a wide-character entry when
  `UNICODE` is defined; `wWinMain` receives the command line as UTF-16.
- No console is allocated (saves ~1–2 MB working set; see
  [memory-budget.md](memory-budget.md) rule 11).
- The CRT startup is minimal (no iostream, no atexit registry bloat beyond CRT
  defaults).

## Command-line handling

v1 ignores the command line entirely. (Future flags like
`--autostart-on/--autostart-off` would be parsed here and forwarded to
`util::Autostart` before the loop.)

## Memory notes

- Nothing allocated here beyond the stack frame and the App object.
- `ScopeGdiplus` holds a single `ULONG_PTR` token.
- The `{ }` scope around `App` guarantees destruction order relative to
  `gdiplus` — important for a clean GDI+ shutdown.

## Testing

- Not unit-tested (process entry). Covered by smoke: app starts, window appears,
  clean exit via menu *Quit* (exit code 0).
- `scripts/package.ps1` launches the EXE to measure working set.

## Related

- [App.h.md](App.h.md) / [App.cpp.md](App.cpp.md)
- [WinUtil.h.md](util/WinUtil.h.md)
- [CMakeLists.txt.md](CMakeLists.txt.md) (subsystem/entry point)
