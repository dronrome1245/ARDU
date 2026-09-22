#include <Arduino.h>

constexpr uint8_t TEST_PIN = A2;

void setup() {
  Serial.begin(115200);
  analogReference(DEFAULT);
  delay(1000);
  Serial.println(F("ARDU ADC SANITY A2 READY"));
}

void loop() {
  uint16_t minValue = 1023;
  uint16_t maxValue = 0;
  uint32_t sum = 0;

  // Discard a few conversions after startup/reference selection.
  for (uint8_t i = 0; i < 8; ++i) {
    (void)analogRead(TEST_PIN);
  }

  for (uint16_t i = 0; i < 256; ++i) {
    const uint16_t sample = analogRead(TEST_PIN);
    if (sample < minValue) minValue = sample;
    if (sample > maxValue) maxValue = sample;
    sum += sample;
  }

  Serial.print(F("ADC_SANITY A2 AVG="));
  Serial.print(static_cast<uint16_t>(sum / 256UL));
  Serial.print(F(" MIN="));
  Serial.print(minValue);
  Serial.print(F(" MAX="));
  Serial.println(maxValue);

  delay(1000);
}
