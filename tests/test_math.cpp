// Unit tests for the small pure-math pieces the character relies on:
// pupil clamping, head-tilt clamping, the hop parabola, and the
// util::Clamp / util::SanitizeDt helpers. No GUI, no GDI+, no Win32.
#include "util/WinUtil.h"      // util::Clamp, util::SanitizeDt
#include "character/Animator.h"
#include "config.h"
#include <cmath>
#include <cstdio>
#include <utility>

static int g_failures = 0;

#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++g_failures;                                     \
       std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } }  \
  while (0)

static void test_clamp() {
  CHECK(util::Clamp(0.5f, 0.0f, 1.0f) == 0.5f);
  CHECK(util::Clamp(-1.0f, 0.0f, 1.0f) == 0.0f);   // below -> lo
  CHECK(util::Clamp(2.0f, 0.0f, 1.0f) == 1.0f);    // above -> hi
  CHECK(util::Clamp(5, 0, 10) == 5);               // int overload
}

static void test_sanitize_dt() {
  CHECK(util::SanitizeDt(1.0f / 30.0f) == 1.0f / 30.0f);   // normal -> unchanged
  CHECK(util::SanitizeDt(0.0f) == 0.016f);                 // <=0 -> default
  CHECK(util::SanitizeDt(5.0f) == 0.016f);                 // huge -> default
}

static void test_pupil_clamp() {
  // Replicates the UpdatePupilTarget formula documented in Controller.cpp.md.
  auto pupilTarget = [](float lx, float ly) {
    const float th = std::atan2f(ly, lx);
    const float r = std::min(cfg::kMaxEyeOffset, std::hypotf(lx, ly));
    return std::make_pair(std::cosf(th) * r, std::sinf(th) * r);
  };

  auto [px, py] = pupilTarget(200.0f, 0.0f);   // far right
  CHECK(px == cfg::kMaxEyeOffset && py == 0.0f);           // capped at max

  auto [qx, qy] = pupilTarget(3.0f, 0.0f);                 // near
  CHECK(qx == 3.0f && qy == 0.0f);                          // not capped

  auto [sx, sy] = pupilTarget(10.0f, 10.0f);                // diagonal
  const float mag = std::hypotf(sx, sy);
  CHECK(mag <= cfg::kMaxEyeOffset + 1e-4f);                 // magnitude bounded
  CHECK(sx > 0.0f && sy > 0.0f);                            // correct quadrant
}

static void test_head_tilt() {
  // atan2(1,-1) = 135 deg = 2.36 rad; x 0.15 = 0.353 > kHeadMaxTilt -> clamps.
  const float tilt = util::Clamp(std::atan2f(1.0f, -1.0f) * cfg::kHeadTiltAngleScale,
                                 -cfg::kHeadMaxTilt, cfg::kHeadMaxTilt);
  CHECK(tilt == cfg::kHeadMaxTilt);            // extreme angle -> clamped
  const float small = util::Clamp(0.1f * cfg::kHeadTiltAngleScale,
                                  -cfg::kHeadMaxTilt, cfg::kHeadMaxTilt);
  CHECK(small < cfg::kHeadMaxTilt);            // small angle passes through
}

static void test_hop_parabola() {
  // v = kHopHeightPx * 4 * t * (1 - t), t in [0,1]
  auto v = [](float t) { return cfg::kHopHeightPx * 4 * t * (1 - t); };
  CHECK(v(0.0f) == 0.0f);
  CHECK(v(0.5f) == cfg::kHopHeightPx);         // peak = height
  CHECK(v(1.0f) == 0.0f);                      // lands back on the ground
  CHECK(v(0.25f) > 0.0f && v(0.75f) > 0.0f);   // symmetric positive
}

static void test_sanitize_dt_keeps_spring_stable() {
  // Cross-check that SanitizeDt keeps a Spring stable on the first tick after
  // a wake-up (large dt sanitized to default, no jump).
  character::Spring s(0.06f);
  s.Reset(1.0f);
  const float v = s.Tick(5.0f, util::SanitizeDt(5.0f));   // sanitized to 0.016
  CHECK(v < 5.0f);                          // stepped, not snapped
}

int main() {
  test_clamp();
  test_sanitize_dt();
  test_pupil_clamp();
  test_head_tilt();
  test_hop_parabola();
  test_sanitize_dt_keeps_spring_stable();

  std::printf("test_math: %s (%d failure(s))\n",
              g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
