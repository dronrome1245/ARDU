#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

// Firmware clock/renderer are injected from the exact R4 .ino in CI.
namespace Cfg { constexpr uint8_t LED_COUNT=44; }
struct CHSV {
  uint8_t h=0, s=0, v=0;
  CHSV()=default;
  CHSV(uint8_t hue,uint8_t saturation,uint8_t value):h(hue),s(saturation),v(value){}
  bool operator==(const CHSV& o)const{
    return h==o.h&&s==o.s&&v==o.v;
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

static void snapshot(CHSV* out){
  for(uint8_t i=0;i<Cfg::LED_COUNT;++i)out[i]=leds[i];
}
static unsigned changedPixels(const CHSV* prior){
  unsigned count=0;
  for(uint8_t i=0;i<Cfg::LED_COUNT;++i)
    if(!(prior[i]==leds[i]))++count;
  return count;
}
static uint32_t frameDistance(const CHSV* before){
  uint32_t score=0;
  for(uint8_t i=0;i<Cfg::LED_COUNT;++i){
    const CHSV& a=before[i];
    const CHSV& b=leds[i];
    // Cyclic hue distance; saturated/dim channels still contribute.
    const uint8_t hueGap=a.h>b.h? a.h-b.h : b.h-a.h;
    const uint8_t cyclic=hueGap>128? 256-hueGap : hueGap;
    const uint8_t satGap=a.s>b.s? a.s-b.s : b.s-a.s;
    const uint8_t valueGap=a.v>b.v? a.v-b.v : b.v-a.v;
    score+=cyclic+satGap+valueGap;
  }
  return score;
}
static uint16_t sceneTicks(uint8_t speed,uint32_t elapsed,uint8_t scene){
  uint32_t accumulator=0;
  return advancePresetClock(elapsed,speed,accumulator,scene==10);
}
int main(){
  // P11 low-end tuning remains the EXACT R3 behavior.
  uint32_t oldQ=0,breathQ=0;
  advancePresetClock(1000,0,oldQ,false);
  advancePresetClock(1000,0,breathQ,true);
  assert(oldQ==512000U && breathQ==416000U);
  uint32_t endOld=0,endBreath=0;
  advancePresetClock(1000,255,endOld,false);
  advancePresetClock(1000,255,endBreath,true);
  assert(endOld==endBreath);
  for(unsigned speed=1;speed<256;++speed){
    uint32_t prev=0,cur=0;
    advancePresetClock(1000,static_cast<uint8_t>(speed-1),prev,true);
    advancePresetClock(1000,static_cast<uint8_t>(speed),cur,true);
    assert(prev<cur);
  }

  // Wraparound and elapsed-delta accumulation remain correct.
  uint32_t phase=0;
  const uint32_t before=UINT32_MAX-22U,after=51U;
  advancePresetClock(after-before,120,phase,false);
  assert(phase==(74U*(512U+120U*6U)));
  uint32_t continuous=0,discrete=0;
  const auto tOne=advancePresetClock(2000,150,continuous,false);
  uint16_t tMany=0;
  for(int n=0;n<50;++n)tMany=advancePresetClock(40,150,discrete,false);
  assert(tOne==tMany && continuous==discrete);

  // This is the regression that had been missing: real rendered pixels must
  // visibly CHANGE even at dynamics 0, not only the abstract phase counter.
  const uint16_t base=sceneTicks(0,0,0);
  const uint16_t slow=sceneTicks(0,2000,0);
  const uint16_t fast=sceneTicks(255,2000,0);
  for(uint8_t scene=0;scene<12;++scene){
    renderPreset(scene,base);
    CHSV first[Cfg::LED_COUNT];snapshot(first);
    assert(requestedBrightness==presetCfg.brightness[scene]);
    assert(frameDirty);
    renderPreset(scene,slow);
    const unsigned changed=changedPixels(first);
    const uint32_t distance=frameDistance(first);
    if(changed<3U || distance<60U){
      std::fprintf(stderr,"Scene P%02u inactive at minimum: pixels=%u change=%lu\n",
                   scene+1,changed,(unsigned long)distance);
      return 1;
    }
    renderPreset(scene,fast);
    const unsigned highChanged=changedPixels(first);
    const uint32_t highDelta=frameDistance(first);
    if(highChanged<3U || highDelta<60U){
      std::fprintf(stderr,"Scene P%02u inactive at maximum: pixels=%u change=%lu\n",
                   scene+1,highChanged,(unsigned long)highDelta);
      return 2;
    }
    // The slider must impact LEDs within two seconds, not just an EEPROM value.
    renderPreset(scene,slow);
    CHSV slowFrame[Cfg::LED_COUNT];snapshot(slowFrame);
    renderPreset(scene,fast);
    if(frameDistance(slowFrame)<60U){
      std::fprintf(stderr,"Scene P%02u insensitive to dynamics\n",scene+1);
      return 3;
    }
    std::printf("P%02u: low-change=%lu high-change=%lu speed-change=%lu pixels=%u\n",
      scene+1,(unsigned long)distance,(unsigned long)highDelta,
      (unsigned long)frameDistance(slowFrame),changed);
  }

  // R3's P11 must keep its uniform whole-ring breathing profile exactly.
  for(const uint16_t tick : {uint16_t(0),uint16_t(17),uint16_t(63),uint16_t(128),uint16_t(255)}){
    renderPreset(10,tick);
    for(uint8_t i=1;i<Cfg::LED_COUNT;++i)assert(leds[0]==leds[i]);
    assert(leds[0].h==118 && leds[0].s==175);
  }
  std::puts("PASS: all twelve LED renderers visibly change at dynamics 0/255; sliders move pixels; P11 preserved");
}
