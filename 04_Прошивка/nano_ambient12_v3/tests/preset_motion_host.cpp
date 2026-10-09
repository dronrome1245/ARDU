#include <cassert>
#include <cstdint>
#include <cstdio>

// The CI step replaces these markers with the EXACT functions from the full
// Nano R3 .ino. No second independent implementation of the clock or renderers.
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
    const uint16_t t=advancePresetClock(60000,static_cast<uint8_t>(speed),q,false);
    if (speed>0) {
      assert(q>prevQ);
      assert(t>=prevT);
    }
    prevQ=q;prevT=t;
  }

  // Previously equivalent dynamics 182/190/225 must now be distinct.
  uint32_t q182=0,q190=0,q225=0;
  const uint16_t t182=advancePresetClock(10000,182,q182,false);
  const uint16_t t190=advancePresetClock(10000,190,q190,false);
  const uint16_t t225=advancePresetClock(10000,225,q225,false);
  assert(q182<q190 && q190<q225);
  assert(t182<t190 && t190<t225);

  // All other presets retain exactly the previous R2 rate, on every slider step.
  // P11 is always slower at 0..254, converging to exactly the R2 maximum.
  uint32_t previousBreathRate=0;
  for(unsigned dynamics=0;dynamics<256;++dynamics){
    uint32_t normalQ=0,breathQ=0;
    advancePresetClock(1000,static_cast<uint8_t>(dynamics),normalQ,false);
    advancePresetClock(1000,static_cast<uint8_t>(dynamics),breathQ,true);
    assert(normalQ==1000U*(512U+dynamics*6U));
    assert(breathQ<=normalQ);
    if(dynamics<255)assert(breathQ<normalQ);
    else assert(breathQ==normalQ);
    if(dynamics>0)assert(breathQ>previousBreathRate);
    previousBreathRate=breathQ;
  }
  // Full P11 pulse spans 128 virtual ticks, with no change at max speed.
  uint32_t p11MinQ=0,p11MaxQ=0,p11OldMinQ=0;
  advancePresetClock(1,0,p11MinQ,true);
  advancePresetClock(1,255,p11MaxQ,true);
  advancePresetClock(1,0,p11OldMinQ,false);
  const uint32_t lowPeriodMs=(128U*65536U)/p11MinQ;
  const uint32_t highPeriodMs=(128U*65536U)/p11MaxQ;
  const uint32_t oldPeriodMs=(128U*65536U)/p11OldMinQ;
  assert(p11MinQ==417U && p11OldMinQ==512U && p11MaxQ==2042U);
  assert(lowPeriodMs>=20000U && lowPeriodMs<=20200U);
  assert(highPeriodMs>=4000U && highPeriodMs<=4200U);
  assert(oldPeriodMs>=16000U && oldPeriodMs<=16500U);

  // Changing P11 speed must preserve phase (not restart the pulse).
  uint32_t breathPhase=0;
  advancePresetClock(4000,0,breathPhase,true);
  const uint32_t beforeAdjust=breathPhase;
  advancePresetClock(40,128,breathPhase,true);
  assert(breathPhase>beforeAdjust);
  assert(breathPhase-beforeAdjust==40U*1233U);

  // One 10-second call and 250 40ms updates must give the same clock.
  uint32_t one=0,frames=0;
  const uint16_t exact=advancePresetClock(10000,113,one,false);
  uint16_t loop=0;
  for(int k=0;k<250;++k)loop=advancePresetClock(40,113,frames,false);
  assert(one==frames && exact==loop);

  // Changing the slider rate must not reset the current animation phase.
  uint32_t dynamic=0;
  advancePresetClock(4000,20,dynamic,false);
  const uint32_t prior=dynamic;
  advancePresetClock(40,225,dynamic,false);
  assert(dynamic>prior);
  assert(dynamic-prior==40U*(512U+225U*6U));

  // The main loop uses unsigned subtraction across a millis() wrap.
  const uint32_t before=UINT32_MAX-20U, after=45U;
  const uint32_t elapsed=after-before;
  assert(elapsed==66U);
  uint32_t wrapQ=0;
  advancePresetClock(elapsed,255,wrapQ,false);
  assert(wrapQ==66U*(512U+255U*6U));

  // Every renderer consumes motion ticks; motion is not just a value read
  // back to Android. Static-looking Bali/Moon only have subtle luminance flow.
  uint32_t slowQ=0,fastQ=0;
  const uint16_t slowT=advancePresetClock(10000,0,slowQ,false);
  const uint16_t fastT=advancePresetClock(10000,255,fastQ,false);
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

  std::puts("PASS: R3 unchanged 11 preset rates, P11 20.1s min/4.1s max, 256 monotonic steps, no phase reset, 12 renderers");
}
