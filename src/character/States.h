#pragma once

namespace character {

enum class CharState {
  Idle,        // cursor still: blink, breathe, glance around
  Tracking,    // cursor moving: eyes + head follow the cursor
  Interacting, // reaction after a click: hop + surprised face (timer-bound)
};

// Debug / logging helper. Returns a stable short string.
inline const char* ToString(CharState s) noexcept {
  switch (s) {
    case CharState::Idle:        return "Idle";
    case CharState::Tracking:    return "Tracking";
    case CharState::Interacting: return "Interacting";
  }
  return "Unknown";
}

}  // namespace character
