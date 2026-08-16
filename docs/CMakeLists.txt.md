# CMakeLists.txt — Build Configuration

## File role

Root build definition for the whole project. Produces:

- `r2dc.exe` — the GUI application (`/SUBSYSTEM:WINDOWS`, `wWinMain`).
- `test_animator`, `test_controller`, `test_math` — console test executables
  registered with CTest.

## Target skeleton

```cmake
cmake_minimum_required(VERSION 3.20)
project(R2DCharacter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 10 SDK surface: enables SetProcessDpiAwarenessContext,
# GetDpiForWindow and the DPI_AWARENESS_CONTEXT constants (see main.cpp).
add_definitions(-DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00)

if(MSVC)
  add_compile_options(/W4 /permissive-)
endif()
```

## App target

```cmake
add_executable(r2dc
  src/main.cpp
  src/App.cpp
  src/desktop/CursorTracker.cpp
  src/desktop/TaskbarLocator.cpp
  src/desktop/OverlayWindow.cpp
  src/character/Animator.cpp
  src/character/Controller.cpp
  src/character/Character.cpp
  src/character/Renderer.cpp
  src/ui/Menu.cpp
  src/util/WinUtil.cpp
  src/util/Autostart.cpp
)

target_link_libraries(r2dc PRIVATE
  gdiplus gdi32 user32 shell32 advapi32
)

target_include_directories(r2dc PRIVATE src)   # #include "desktop/...", "config.h"
```

> Order of sources is cosmetic.

## Release flags (memory-critical)

These come from [memory-budget.md](memory-budget.md) and must not be relaxed:

```cmake
if(MSVC)
  # /O1 favors size; use either /O1 or /O2, never default.
  target_compile_options(r2dc PRIVATE
    /O1
    /GL              # whole-program optimization (with /LTCG link)
    /GS-             # skip stack cookies (small, single-purpose app)
    /MT              # static CRT — no VCRUNTIME dll pages
    /Zc:inline
    /GR-             # no RTTI
    /EHsc            # keep exceptions off hot path; none thrown in app
  )
  target_link_options(r2dc PRIVATE
    /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS,6.0
  )
# strip release symbols
set_target_properties(r2dc PROPERTIES
  MAP_FILE OFF
)
endif()
```

> `/GS-` disables stack cookies — acceptable for this trusted local utility; if
> you prefer defense-in-depth, remove `/GS-` (costs ~a few KB, still under budget).

## Test targets

```cmake
enable_testing()

# Each test compiles the REAL sources it exercises (no copies, no mocks).
add_executable(test_animator
  tests/test_animator.cpp
  src/character/Animator.cpp)
add_executable(test_controller
  tests/test_controller.cpp
  src/character/Controller.cpp
  src/character/Animator.cpp)
add_executable(test_math
  tests/test_math.cpp
  src/character/Animator.cpp)

target_include_directories(test_animator test_controller test_math PRIVATE src)

add_test(NAME test_animator   COMMAND test_animator)
add_test(NAME test_controller COMMAND test_controller)
add_test(NAME test_math       COMMAND test_math)
```

Each test is a standalone console app (uses `<iostream>`, acceptable — tests
are not shipped). Notes:

- `test_math` uses only `util::Clamp` / `util::SanitizeDt`, which are **inline
  in the header** — no `WinUtil.cpp` link needed.
- `Controller.cpp` pulls in `config.h` and `util/WinUtil.h` (header-only), so
  the tests need no Win32 libraries.

## Subsystem / entry points

- `r2dc`: `/SUBSYSTEM:WINDOWS` → linker looks for `WinMain`/`wWinMain`.
  `wWinMain` is defined in `src/main.cpp`.
- Tests: console subsystem (default when using `add_executable` without flags).

## DPI and platform notes

- `UNICODE`/`_UNICODE` and `_WIN32_WINNT=0x0A00` are defined globally (see the
  target skeleton) so `wWinMain`, the `W`-suffixed Win32 APIs, and the DPI
  awareness APIs compile on a clean SDK.
- Target x86 (Win32) or x64 — either is fine; x86 slightly smaller working set
  on the shared tail. CMake generator `-A Win32` is the default recommendation
  (see [scripts/build.ps1.md](scripts/build.ps1.md)).

## Generated artifacts

- `build/r2dc/Release/r2dc.exe` (MSVC generator layout).
- Test EXEs under `build/tests/Release/`.

## Verification

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Size gate (see [scripts/package.ps1.md](scripts/package.ps1.md)):
`r2dc.exe` should be ≤ ~500 KB on disk (with `/O1` + LTCG + strip).
