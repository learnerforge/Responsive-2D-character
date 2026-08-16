# src/config.h — Design Tokens & Constants

## File role

Single home for every tunable constant. Header-only, zero dependencies, pure
`constexpr`. Changing a behavior should never require touching logic files.

## Content (complete)

```cpp
#pragma once

namespace cfg {

// Window / placement -------------------------------------------------------
inline constexpr int  kWindowWidth   = 128;   // logical px (scaled by DPI)
inline constexpr int  kWindowHeight  = 160;
inline constexpr int  kAnchorX       = 64;    // x from work-area left (px)
inline constexpr int  kAnchorGap     = 2;     // feet above taskbar top (px)

// Timing -------------------------------------------------------------------
inline constexpr int  kTickHz        = 30;                 // controller tick
inline constexpr int  kIdleTimeoutMs = 3000;               // Tracking→Idle
inline constexpr int  kInteractionDurationMs = 1200;

// Cursor → look ------------------------------------------------------------
inline constexpr float kEyeSpringTau  = 0.06f;   // s, eyes (fast)
inline constexpr float kHeadSpringTau = 0.12f;   // s, head (slow)
inline constexpr float kHeadTiltAngleScale = 0.15f;  // rad angle → tilt factor
inline constexpr float kHeadMaxTilt   = 0.30f;   // rad clamp
inline constexpr float kMaxEyeOffset  = 5.0f;    // px pupil travel
inline constexpr float kCursorDeadZonePx = 4.0f; // px before Tracking engages

// Idle animations ----------------------------------------------------------
inline constexpr int  kBlinkMinIntervalMs = 2400;
inline constexpr int  kBlinkMaxIntervalMs = 5200;
inline constexpr int  kBlinkDurationMs    = 110;
inline constexpr int  kSaccadeMinIntervalMs = 3800;
inline constexpr int  kSaccadeMaxIntervalMs = 9000;
inline constexpr int  kSaccadeDurationMs    = 220;
inline constexpr float kSaccadeMinPx = 2.0f, kSaccadeMaxPx = 5.0f;
inline constexpr float kBreathPeriodS    = 3.4f;
inline constexpr float kBreathAmplitudePx = 1.6f;

// Interaction --------------------------------------------------------------
inline constexpr float kHopHeightPx = 10.0f;

// App identity -------------------------------------------------------------
inline constexpr wchar_t kAppName[]        = L"Responsive 2D Character";
inline constexpr wchar_t kWindowClass[]    = L"R2DC_PetWindow";
inline constexpr wchar_t kAutostartValueName[] = L"Responsive2DCharacter";

// Repaint dirty thresholds ---------------------------------------------------
inline constexpr float kDirtyPupilPx    = 0.5f;
inline constexpr float kDirtyBlink      = 0.02f;
inline constexpr float kDirtyHeadRad    = 0.005f;
inline constexpr float kDirtyBreathPx   = 0.05f;
inline constexpr float kDirtyHopPx      = 0.1f;

}  // namespace cfg
```

## Design rules

- **Everything** visual or behavioral is a token here; no magic numbers in
  `.cpp` files.
- Integer timing in ms, float motion in px / radians / seconds — units are part
  of the name.
- Tokens are `inline constexpr` (C++17) → no ODR issues, compiled into each TU
  that uses them, zero runtime cost.

## Tuning table (who changes what)

| Token | Effect | Tune for |
|---|---|---|
| `kEyeSpringTau` / `kHeadSpringTau` | responsiveness of follow | feel: lower = snappier |
| `kMaxEyeOffset` | how far pupils travel | cuteness |
| `kHeadMaxTilt` / `kHeadTiltAngleScale` | head turn range | expressiveness |
| `kBlink*` | blink frequency/duration | naturalness |
| `kSaccade*` | idle glancing rate | liveliness when idle |
| `kBreath*` | body breathing | aliveness |
| `kIdleTimeoutMs` | when the pet relaxes | patience |
| `kAnchorX`/`kAnchorGap` | position on screen | layout / DPI behavior |
| `kWindowWidth/Height` | canvas size | character proportions + DPI |
| `kDirty*` | repaint thresholds | CPU vs smoothness tradeoff |

## Constraints

- Changing `kWindowWidth/Height` requires the same change in
  [Renderer.md](character/Renderer.h.md) usage and the DPI scaling in
  [App.cpp.md](App.cpp.md) — both read `cfg`, so they follow automatically.
- `kTickHz` must stay ≥ 20 to keep springs looking smooth; the 33 ms period is
  derived from it (`1000 / kTickHz`).

## Memory notes

- All `constexpr` → compiled into the image, no runtime storage, no static
  initializers. Zero working-set contribution.
