#pragma once
#include <Arduino.h>

enum JsonRead : uint8_t {
  JSON_MISSING = 0,
  JSON_OK = 1,
  JSON_BAD = 2
};

struct NanoResult {
  bool ok = false;
  bool timedOut = false;
  uint8_t errorCode = 0;
  String response;
};
