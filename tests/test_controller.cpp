// Unit tests for character::Controller using an injected fake cursor source.
// Console executable; the behavior spec lives in
// docs/character/Controller.h.md / Controller.cpp.md.
#include "character/Controller.h"   // also pulls in config.h for cfg tokens
#include <cmath>
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++g_failures;                                     \
       std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } }  \
  while (0)

// Fake cursor source whose clock/position the tests drive directly.
struct FakeCursor : character::ICursorSource {
  int x = 1000, y = 1000;      // start at the window center (dead-zone baseline)
  bool available = true;
  bool Get(int& cx, int& cy) override {
    if (!available) return false;
    cx = x; cy = y; return true;
  }
};

// A fully wired rig: fresh controller + fake cursor starting at the window
// center (so the dead-zone baseline equals the startup cursor position).
struct Rig {
  FakeCursor cur;
  character::Controller ctl;
  long long now = 0;
  Rig() { ctl.SetCursorSource(&cur); ctl.SetWindowCenter(1000, 1000); }
};

static void Tick(Rig& r, int n) {
  for (int i = 0; i < n; ++i) { r.now += 33; r.ctl.Tick(r.now); }
}

static void test_dead_zone() {
  Rig r;
  r.ctl.Tick(0);
  r.cur.x = 1003; r.cur.y = 1001;        // within 4 px dead zone
  r.ctl.Tick(33);
  CHECK(r.ctl.State() == character::CharState::Idle);
}

static void test_idle_to_tracking() {
  Rig r;
  r.ctl.Tick(0);
  r.cur.x = 1200; r.cur.y = 1050;        // beyond dead zone
  r.ctl.Tick(33);
  CHECK(r.ctl.State() == character::CharState::Tracking);
}

static void test_tracking_to_idle() {
  Rig r;
  r.ctl.Tick(0);
  r.cur.x = 1200; r.cur.y = 1050;
  r.ctl.Tick(33);                        // -> Tracking
  Tick(r, 100);                          // 3.3 s of stillness
  CHECK(r.ctl.State() == character::CharState::Idle);
}

static void test_pupil_direction() {
  Rig r;
  r.ctl.Tick(0);
  r.cur.x = 1200; r.cur.y = 1000;        // directly right of center
  r.ctl.Tick(33);
  auto st = r.ctl.Current();
  CHECK(st.pupilX > 0.0f && std::fabs(st.pupilY) < 0.5f);

  r.cur.x = 1000; r.cur.y = 1300;        // directly below
  Tick(r, 6);                            // let the spring settle (~200 ms)
  st = r.ctl.Current();
  CHECK(st.pupilY > 0.0f && std::fabs(st.pupilX) < 0.5f);
}

static void test_pupil_clamp() {
  Rig r;
  r.ctl.Tick(0);
  r.cur.x = 5000; r.cur.y = 1000;        // very far away
  Tick(r, 30);                           // springs settle
  auto st = r.ctl.Current();
  CHECK(std::hypotf(st.pupilX, st.pupilY) <= cfg::kMaxEyeOffset + 0.01f);
}

static void test_interaction() {
  Rig r;
  r.ctl.Tick(0);
  r.ctl.OnLeftClick();
  r.ctl.Tick(33);
  CHECK(r.ctl.State() == character::CharState::Interacting);
  CHECK(r.ctl.Current().surprised == true);
  Tick(r, 40);                           // 1.32 s > 1.2 s interaction
  CHECK(r.ctl.State() == character::CharState::Idle);
  CHECK(r.ctl.Current().surprised == false);
  CHECK(r.ctl.Current().hopY == 0.0f);
}

static void test_blink_scheduling() {
  Rig r;
  r.ctl.Tick(0);                         // schedules the first blink window
  bool saw = false;
  long long first = -1;
  for (long long t = 33; t < 60'000; t += 33) {
    r.ctl.Tick(t);
    if (r.ctl.Current().blink < 0.999f && !saw) { saw = true; first = t; }
  }
  CHECK(saw);
  CHECK(first >= cfg::kBlinkMinIntervalMs);
  // Tick granularity: a blink starting at most kBlinkMaxIntervalMs is first
  // OBSERVED (blink < 1.0) on the following tick, hence the +100 ms slack.
  CHECK(first <= cfg::kBlinkMaxIntervalMs + 100);
}

static void test_cursor_unavailable() {
  Rig r;
  r.ctl.Tick(0);
  r.cur.available = false;
  r.ctl.Tick(33);                        // must not throw / change state
  r.cur.available = true;
  CHECK(r.ctl.State() == character::CharState::Idle);
}

static void test_dirty_flag() {
  Rig r;
  r.ctl.Tick(0);
  r.cur.x = 1200; r.cur.y = 1000;        // enter Tracking, settle springs
  Tick(r, 10);
  while (r.ctl.Current().blink < 1.0f) Tick(r, 1);   // wait out any blink

  r.ctl.ClearRepaint();
  r.now += 33; r.ctl.Tick(r.now);        // quiet tick: nothing measurable
  CHECK(!r.ctl.WantsRepaint());

  r.cur.y += 200;                        // angle change -> pupil targets move
  r.now += 33; r.ctl.Tick(r.now);
  CHECK(r.ctl.WantsRepaint());
}

int main() {
  test_dead_zone();
  test_idle_to_tracking();
  test_tracking_to_idle();
  test_pupil_direction();
  test_pupil_clamp();
  test_interaction();
  test_blink_scheduling();
  test_cursor_unavailable();
  test_dirty_flag();

  std::printf("test_controller: %s (%d failure(s))\n",
              g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
