#pragma once
#include <stdint.h>

// Experimental renderers for the future Ambient presets.
// NOT a wire protocol, EEPROM format, stable preset ID or release feature.
// CRGB/CHSV are provided by FastLED in the Nano firmware.
// No global LED buffer or persistent state.
namespace ArduAmbientProbe {

enum Scene : uint8_t {
  AURORA = 0,   // concept P01
  COSMOS = 1,   // concept P04
  FIREPLACE = 2 // concept P05
};

inline uint8_t triangle8(uint8_t x) {
  return x < 128U ? static_cast<uint8_t>(x * 2U)
                  : static_cast<uint8_t>((255U - x) * 2U);
}

inline uint8_t hash8(uint8_t x) {
  x ^= static_cast<uint8_t>(x << 3);
  x ^= static_cast<uint8_t>(x >> 5);
  return static_cast<uint8_t>(x * 29U);
}

// dynamics 0..255, higher means faster; time comes from millis().
// Brightness/current control stays with the caller (FastLED.setBrightness).
// One 44-pixel logical frame is mirrored by Nano on D6 and D7.
inline bool render(uint8_t scene, CRGB* out, uint8_t count,
                   uint32_t nowMs, uint8_t dynamics) {
  // Production geometry is exactly 44 LEDs per ring, nothing else.
  if (!out || count != 44U || scene > FIREPLACE) return false;
  const uint8_t flowShift = dynamics >= 170U ? 7U : (dynamics >= 85U ? 8U : 9U);

  if (scene == AURORA) {
    const uint8_t phase = static_cast<uint8_t>(nowMs >> flowShift);
    for (uint8_t i = 0; i < count; ++i) {
      const uint8_t flow = triangle8(static_cast<uint8_t>(i * 7U + phase));
      const uint8_t depth = triangle8(static_cast<uint8_t>(i * 11U - (phase >> 1)));
      // Wide moving green/cyan/blue/violet regions, not a uniform color.
      out[i] = CHSV(static_cast<uint8_t>(96U + (flow >> 1) + (depth >> 4)),
                    230U, static_cast<uint8_t>(135U + (depth >> 2)));
    }
    return true;
  }

  if (scene == COSMOS) {
    // One shared epoch, three independent soft fades. Avoid expensive
    // per-star 32-bit divisions/modulo, important on ATmega328P Flash.
    const uint8_t shift = static_cast<uint8_t>(flowShift + 4U);
    // One 32-bit shift yields both the slot and its 8-bit fade phase.
    const uint16_t timeline = static_cast<uint16_t>(nowMs >> (shift - 8U));
    const uint16_t epoch = static_cast<uint16_t>(timeline >> 8);
    const uint8_t age = static_cast<uint8_t>(timeline);
    for (uint8_t i = 0; i < count; ++i) out[i] = CRGB(1U, 2U, 10U);
    for (uint8_t star = 0; star < 3U; ++star) {
      const uint8_t phase = static_cast<uint8_t>(age + star * 85U);
      const uint8_t fade = triangle8(phase);
      const uint8_t slot = static_cast<uint8_t>(
          epoch + static_cast<uint8_t>(phase < age));
      const uint8_t seed = hash8(static_cast<uint8_t>(slot * 19U + star * 61U));
      const uint8_t index = static_cast<uint8_t>(
          (static_cast<uint16_t>(seed) * 44U) >> 8);
      out[index] = CHSV(static_cast<uint8_t>(155U + star * 13U), 80U,
                        static_cast<uint8_t>(10U + (fade >> 1)));
    }
    return true;
  }

  // P05: warm independent segments, smoothly interpolated between noise
  // targets. No per-frame random flashing and no per-LED runtime buffer.
  const uint8_t shift = flowShift;
  const uint16_t timeline = static_cast<uint16_t>(nowMs >> (shift - 4U));
  const uint16_t slot = static_cast<uint16_t>(timeline >> 4);
  const uint8_t fraction = static_cast<uint8_t>(timeline & 15U);
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t seed = static_cast<uint8_t>(i * 37U + slot * 19U);
    const uint8_t a = hash8(seed);
    const uint8_t b = hash8(static_cast<uint8_t>(seed + 19U));
    const uint8_t level = static_cast<uint8_t>(
        (static_cast<uint16_t>(a) * (16U - fraction) +
         static_cast<uint16_t>(b) * fraction) >> 4);
    out[i] = CHSV(static_cast<uint8_t>(6U + (level >> 3)), 245U,
                  static_cast<uint8_t>(105U + (level >> 1)));
  }
  return true;
}

} // namespace ArduAmbientProbe
