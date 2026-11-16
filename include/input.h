#pragma once
#include <Arduino.h>

enum class InputEvent : uint8_t {
  None = 0,
  NavLeft,
  NavRight,
  NavUp,
  NavDown,
  NavPress,
  KeyA,
  KeyB,
  KeyC,
};

enum KeyIndex : uint8_t {
  K_A = 0, K_B, K_C, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_PRESS, K_COUNT
};

struct KeyDef { uint8_t pin; const char *name; };
extern const KeyDef KEY_DEFS[K_COUNT];

void inputBegin();
InputEvent pollInput();
bool isKeyDown(int pin);
