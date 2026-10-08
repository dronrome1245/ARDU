// Native host tests; not compiled into Nano firmware.
// The mock stores HSV as byte triples to inspect physical frame structure.
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <set>

struct CHSV {
  uint8_t h, s, v;
  CHSV(uint8_t h_, uint8_t s_, uint8_t v_) : h(h_), s(s_), v(v_) {}
};
struct CRGB {
  uint8_t r = 0, g = 0, b = 0;
  CRGB() = default;
  CRGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
  CRGB& operator=(const CHSV& hsv) {
    r = hsv.h; g = hsv.s; b = hsv.v;
    return *this;
  }
  bool operator==(const CRGB& c) const {
    return r == c.r && g == c.g && b == c.b;
  }
};

#include "../ambient_preset_probe.h"

using Frame = std::array<CRGB, 46>;
static Frame frameAt(uint8_t scene, uint32_t ms, uint8_t dynamics) {
  Frame f{};
  f[0] = CRGB(0xEE, 0xDD, 0xCC);
  f[45] = CRGB(0xAA, 0xBB, 0xCC);
  assert(ArduAmbientProbe::render(scene, &f[1], 44, ms, dynamics));
  assert((f[0] == CRGB(0xEE, 0xDD, 0xCC)));
  assert((f[45] == CRGB(0xAA, 0xBB, 0xCC)));
  for (unsigned i = 1; i <= 44; ++i) assert(f[i].b <= 245U);
  return f;
}
static bool different(const Frame& a, const Frame& b) {
  for (unsigned i = 1; i <= 44; ++i) if (!(a[i] == b[i])) return true;
  return false;
}
int main() {
  Frame scenes[3];
  for (uint8_t scene = 0; scene < 3; ++scene) {
    scenes[scene] = frameAt(scene, 1234U, 128U);
    const Frame repeat = frameAt(scene, 1234U, 128U);
    const Frame later = frameAt(scene, 7777U, 128U);
    assert(!different(scenes[scene], repeat)); // deterministic, no RNG RAM
    assert(different(scenes[scene], later));   // time-varying pattern
    const Frame fast = frameAt(scene, 1234U, 220U);
    assert(different(scenes[scene], fast));    // dynamics alters motion
    (void)frameAt(scene, UINT32_MAX, 255U);
    (void)frameAt(scene, 0U, 0U);
  }
  assert(different(scenes[0], scenes[1]));
  assert(different(scenes[1], scenes[2]));
  assert(different(scenes[0], scenes[2]));
  std::set<uint8_t> auroraHues, fireHues;
  unsigned stars = 0;
  for (unsigned i = 1; i <= 44; ++i) {
    auroraHues.insert(scenes[0][i].r);
    fireHues.insert(scenes[2][i].r);
    if (scenes[1][i].b > 10U) ++stars;
    assert(scenes[2][i].r >= 6U && scenes[2][i].r <= 37U);
  }
  assert(auroraHues.size() > 6U);
  assert(fireHues.size() > 4U);
  assert(stars >= 1U && stars <= 3U);
  Frame untouched = scenes[0];
  assert(!ArduAmbientProbe::render(3U, &untouched[1], 44U, 1000U, 128U));
  assert(!different(untouched, scenes[0]));
  assert(!ArduAmbientProbe::render(0U, &untouched[1], 0U, 1000U, 128U));
  assert(!ArduAmbientProbe::render(0U, nullptr, 44U, 1000U, 128U));
  std::cout << "PASS: 3 distinct 44-LED frames, deterministic animation, "
               "bounds, dynamics and no buffer overrun\n";
}
