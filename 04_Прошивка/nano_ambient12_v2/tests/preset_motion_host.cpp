#include <cassert>
#include <cstdint>
#include <cstdio>

// The CI step replaces these markers with the EXACT functions from the full
// Nano R2 .ino. No second independent implementation of the clock or renderers.
namespace Cfg { constexpr uint8_t LED_COUNT = 44; }
struct CHSV {
  uint8_t h=0, s=0, v=0;
  CHSV() = default;
  CHSV(uint8_t hh,uint8_t ss,uint8_t vv):h(hh),s(ss),v(vv){}
  bool operator==(const CHSV& o) const {
    return h==o.h && s==o.s && v==o.v;
  }
};
CHSV leds[Cfg::LED_COUNT];
struct TestPresets {
  uint8_t brightness[12]={115,128,102,64,115,64,51,89,128,102,77,115};
  uint8_t dynamics[12]={90,20,95,40,100,55,0,70,115,85,80,120};
} presetCfg;
uint8_t requestedBrightness=0;
bool frameDirty=false;

// @INJECT_PRESET_CLOCK

// @INJECT_PRESET_RENDERER

int main() {
  // Every value of 0..255 results in a distinct increasing clock rate.
  uint32_t prevQ=0;
  uint16_t prevT=0;
  for (unsigned speed=0;speed<256;++speed) {
    uint32_t q=0;
    const uint16_t t=advancePresetClock(60000,static_cast<uint8_t>(speed),q);
    if (speed>0) {
      assert(q>prevQ);
      assert(t>=prevT);
    }
    prevQ=q;prevT=t;
  }

  // Previously equivalent dynamics 182/190/225 must now be distinct.
  uint32_t q182=0,q190=0,q225=0;
  const uint16_t t182=advancePresetClock(10000,182,q182);
  const uint16_t t190=advancePresetClock(10000,190,q190);
  const uint16_t t225=advancePresetClock(10000,225,q225);
  assert(q182<q190 && q190<q225);
  assert(t182<t190 && t190<t225);

  // One 10-second call and 250 40ms updates must give the same clock.
  uint32_t one=0,frames=0;
  const uint16_t exact=advancePresetClock(10000,113,one);
  uint16_t loop=0;
  for(int k=0;k<250;++k)loop=advancePresetClock(40,113,frames);
  assert(one==frames && exact==loop);

  // Changing the slider rate must not reset the current animation phase.
  uint32_t dynamic=0;
  advancePresetClock(4000,20,dynamic);
  const uint32_t prior=dynamic;
  advancePresetClock(40,225,dynamic);
  assert(dynamic>prior);
  assert(dynamic-prior==40U*(512U+225U*6U));

  // The main loop uses unsigned subtraction across a millis() wrap.
  const uint32_t before=UINT32_MAX-20U, after=45U;
  const uint32_t elapsed=after-before;
  assert(elapsed==66U);
  uint32_t wrapQ=0;
  advancePresetClock(elapsed,255,wrapQ);
  assert(wrapQ==66U*(512U+255U*6U));

  // Every renderer consumes motion ticks; motion is not just a value read
  // back to Android. Static-looking Bali/Moon only have subtle luminance flow.
  uint32_t slowQ=0,fastQ=0;
  const uint16_t slowT=advancePresetClock(10000,0,slowQ);
  const uint16_t fastT=advancePresetClock(10000,255,fastQ);
  for(uint8_t scene=0;scene<12;++scene){
    renderPreset(scene,slowT);
    // Compare all 44 HSV values without requiring FastLED in host builds.
    CHSV snapshot[Cfg::LED_COUNT];
    for(uint8_t i=0;i<Cfg::LED_COUNT;++i)snapshot[i]=leds[i];
    assert(frameDirty);
    assert(requestedBrightness==presetCfg.brightness[scene]);
    renderPreset(scene,fastT);
    bool different=false;
    for(uint8_t i=0;i<Cfg::LED_COUNT;++i)
      if(!(snapshot[i]==leds[i]))different=true;
    assert(different);
  }

  // Breathing (P11) should move over seconds rather than several minutes.
  renderPreset(10,0);
  const uint8_t a=leds[0].v;
  renderPreset(10,60);
  const uint8_t b=leds[0].v;
  assert(a!=b);

  std::puts("PASS: 256 distinct speed rates, no phase reset, rollover, 12 renderers, P11 pulse");
}
