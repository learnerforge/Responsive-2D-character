cmake_minimum_required(VERSION 3.20)
project(R2DCharacter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 10 SDK surface: enables SetProcessDpiAwarenessContext,
# GetDpiForWindow and the DPI_AWARENESS_CONTEXT constants (see src/main.cpp).
# NOMINMAX avoids the windows.h min/max macros clashing with std::min/max/clamp.
add_definitions(-DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

if(MSVC)
  add_compile_options(/W4 /permissive-)
endif()

# ---------------------------------------------------------------------------
# App target
# ---------------------------------------------------------------------------
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

target_include_directories(r2dc PRIVATE src)

target_link_libraries(r2dc PRIVATE gdiplus gdi32 user32 shell32 advapi32)

if(MSVC)
  # Memory-critical release flags (see docs/memory-budget.md). /O1 favors size.
  target_compile_options(r2dc PRIVATE /O1 /GL /GS- /MT /Zc:inline /GR- /EHsc)
  target_link_options(r2dc PRIVATE /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS,6.0)
  set_target_properties(r2dc PROPERTIES MAP_FILE OFF)
else()
  # MinGW / non-MSVC: wWinMain entry requires -municode; -mwindows sets the
  # GUI subsystem (no console); keep the CRT static.
  target_link_options(r2dc PRIVATE -municode -mwindows -static)
endif()

# ---------------------------------------------------------------------------
# Tests (console executables, registered with CTest)
# ---------------------------------------------------------------------------
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

target_include_directories(test_animator PRIVATE src)
target_include_directories(test_controller PRIVATE src)
target_include_directories(test_math PRIVATE src)

add_test(NAME test_animator   COMMAND test_animator)
add_test(NAME test_controller COMMAND test_controller)
add_test(NAME test_math       COMMAND test_math)
