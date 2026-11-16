#include "input.h"

const KeyDef KEY_DEFS[K_COUNT] = {
  {WIO_KEY_A, "KEY_A"}, {WIO_KEY_B, "KEY_B"}, {WIO_KEY_C, "KEY_C"},
  {WIO_5S_UP, "U"}, {WIO_5S_DOWN, "D"}, {WIO_5S_LEFT, "L"},
  {WIO_5S_RIGHT, "R"}, {WIO_5S_PRESS, "PRESS"},
};

struct Button {
  int pin;
  InputEvent evt;
  uint32_t lastChange;
  bool wasDown;
};

static Button s_btns[] = {
  {WIO_5S_LEFT,  InputEvent::NavLeft,  0, false},
  {WIO_5S_RIGHT, InputEvent::NavRight, 0, false},
  {WIO_5S_UP,    InputEvent::NavUp,    0, false},
  {WIO_5S_DOWN,  InputEvent::NavDown,  0, false},
  {WIO_5S_PRESS, InputEvent::NavPress, 0, false},
  {WIO_KEY_A,    InputEvent::KeyA,     0, false},
  {WIO_KEY_B,    InputEvent::KeyB,     0, false},
  {WIO_KEY_C,    InputEvent::KeyC,     0, false},
};

static const uint32_t DEBOUNCE_MS = 25;

void inputBegin() {
  uint32_t now = millis();
  for (auto &b : s_btns) {
    pinMode(b.pin, INPUT_PULLUP);
    b.wasDown = (digitalRead(b.pin) == LOW);
    b.lastChange = now;
  }
}

InputEvent pollInput() {
  uint32_t now = millis();
  for (auto &b : s_btns) {
    bool down = (digitalRead(b.pin) == LOW);
    if (down != b.wasDown && (now - b.lastChange) > DEBOUNCE_MS) {
      b.lastChange = now;
      b.wasDown = down;
      if (down) return b.evt;
    }
  }
  return InputEvent::None;
}

bool isKeyDown(int pin) { return digitalRead(pin) == LOW; }
