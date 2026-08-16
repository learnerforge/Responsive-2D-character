// Unit tests for character::Spring (see docs/character/Animator.h.md).
// Console executable, no test framework - prints failures and returns non-zero
// so CTest fails loudly.
#include "character/Animator.h"
#include <cmath>
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++g_failures;                                     \
       std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } }  \
  while (0)

static bool Close(float a, float b, float eps) { return std::fabs(a - b) < eps; }

static void test_convergence() {
  character::Spring s(0.06f);
  for (int i = 0; i < 1000; ++i) s.Tick(10.0f, 1.0f / 60.0f);
  CHECK(Close(s.Value(), 10.0f, 0.01f));
}

static void test_no_overshoot() {
  character::Spring s(0.06f);
  s.Reset(0.0f);
  float prev = s.Value();
  for (int i = 0; i < 300; ++i) {
    float v = s.Tick(5.0f, 1.0f / 60.0f);
    CHECK(v >= prev);        // strictly monotonic approach upward
    CHECK(v <= 5.0f);        // never exceeds target
    prev = v;
  }

  character::Spring t(0.06f);
  t.Reset(5.0f);
  prev = t.Value();
  for (int i = 0; i < 300; ++i) {
    float v = t.Tick(0.0f, 1.0f / 60.0f);
    CHECK(v <= prev);        // strictly monotonic approach downward
    CHECK(v >= 0.0f);        // never below target
    prev = v;
  }
}

static void test_frame_independence() {
  character::Spring a(0.06f), b(0.06f);
  for (int i = 0; i < 60; ++i) a.Tick(7.0f, 1.0f / 60.0f);
  for (int i = 0; i < 30; ++i) b.Tick(7.0f, 1.0f / 30.0f);
  CHECK(Close(a.Value(), b.Value(), 1e-3f));   // same total time, same result
}

static void test_dt_noop() {
  character::Spring s(0.06f);
  s.Reset(3.0f);
  float v = s.Tick(9.0f, 0.0f);
  CHECK(Close(v, 3.0f, 1e-6f));
  v = s.Tick(9.0f, -1.0f);
  CHECK(Close(v, 3.0f, 1e-6f));
}

static void test_reset() {
  character::Spring s(0.06f);
  s.Tick(5.0f, 1.0f / 30.0f);
  s.Reset(2.0f);
  CHECK(Close(s.Value(), 2.0f, 1e-6f));
}

static void test_tau_clamp() {
  character::Spring s(0.06f);
  s.SetTau(0.0f);
  CHECK(s.Tau() > 0.0f);
}

int main() {
  test_convergence();
  test_no_overshoot();
  test_frame_independence();
  test_dt_noop();
  test_reset();
  test_tau_clamp();

  std::printf("test_animator: %s (%d failure(s))\n",
              g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
