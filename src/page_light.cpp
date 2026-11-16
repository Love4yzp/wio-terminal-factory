#include "page.h"
#include "ui.h"
#include "tests.h"
#include "test_config.h"

const char *PageLight::title() const { return "Light & IO"; }
const char *PageLight::hint() const { return "A/B/C C4/E4/G4  Press reset  L/R tab"; }

static const int BAR_X = 10, BAR_Y = 54, BAR_W = 44, BAR_H = 124;
static const int BOX_W = 64, BOX_H = 22;
static const int BOX[K_COUNT][2] = {
  {78, 48}, {152, 48}, {226, 48},    // A B C
  {152, 78},                          // UP
  {152, 138},                         // DOWN
  {78, 108}, {226, 108},              // LEFT RIGHT
  {152, 108},                         // PRESS
};
static const char *LABEL[K_COUNT] = {"KEY_A", "KEY_B", "KEY_C", "U", "D", "L", "R", "PRESS"};

static uint8_t s_seen = 0;
static uint8_t s_lastDown = 0;
static int s_lastRaw = -1;

static void drawBox(int i, bool down) {
  bool seen = s_seen & (1 << i);
  int x = BOX[i][0], y = BOX[i][1];
  uint16_t fill = down ? C::ok : (seen ? 0x0320 : C::panel);
  tft.fillRoundRect(x, y, BOX_W, BOX_H, 4, fill);
  tft.drawRoundRect(x, y, BOX_W, BOX_H, 4, down ? C::text : (seen ? C::ok : C::dim));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(down ? C::bg : C::text, fill);
  tft.setTextSize(1);
  tft.drawString(LABEL[i], x + BOX_W / 2, y + BOX_H / 2);
}

static void drawAllBoxes() {
  for (int i = 0; i < K_COUNT; i++) drawBox(i, isKeyDown(KEY_DEFS[i].pin));
}

static void drawSeenCount() {
  int n = 0;
  for (int i = 0; i < K_COUNT; i++) if (s_seen & (1 << i)) n++;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d/8", n);
  tft.fillRect(112, 164, 60, 12, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(n == 8 ? C::ok : C::text, C::bg);
  tft.setTextSize(1);
  tft.drawString(buf, 112, 170);
}

static void drawLight(bool force) {
  int raw = readLightRaw();
  if (raw < 0) raw = 0;
  if (raw > 1023) raw = 1023;
  if (!force && raw == s_lastRaw) return;
  s_lastRaw = raw;

  int h = raw * BAR_H / 1023;
  tft.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, C::bg);
  for (int i = 0; i < h; i++) {
    uint8_t r = (uint8_t)((i * 255) / BAR_H);
    uint8_t g = (uint8_t)(255 - r);
    uint16_t col = tft.color565(r, g, 32);
    tft.drawFastHLine(BAR_X, BAR_Y + BAR_H - 1 - i, BAR_W, col);
  }

  tft.fillRect(BAR_X - 2, 184, 62, 32, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::text, C::bg);
  tft.setTextSize(2);
  char buf[8];
  snprintf(buf, sizeof(buf), "%4d", raw);
  tft.drawString(buf, BAR_X, 192);
  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  snprintf(buf, sizeof(buf), "%d%%", raw * 100 / 1023);
  tft.drawString(buf, BAR_X, 208);
}

static void drawFrame() {
  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);
  tft.drawRect(BAR_X - 2, BAR_Y - 2, BAR_W + 4, BAR_H + 4, C::panel);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  tft.drawString("LIGHT", BAR_X - 2, 36);

  drawAllBoxes();

  tft.setTextColor(C::dim, C::bg);
  tft.drawString("SEEN", 78, 170);
  tft.setTextColor(C::text, C::bg);
  tft.drawString("A/B/C = C4/E4/G4", 78, 186);
  tft.setTextColor(C::dim, C::bg);
  tft.drawString("NavPress resets seen", 78, 202);
}

void PageLight::enter() {
  s_seen = 0;
  s_lastDown = 0;
  s_lastRaw = -1;
  drawFrame();
  drawLight(true);
  drawSeenCount();
}

void PageLight::onNavigate(InputEvent e) {
  if (e == InputEvent::NavLeft) { s_seen |= 1 << K_LEFT; drawBox(K_LEFT, false); }
  if (e == InputEvent::NavRight) { s_seen |= 1 << K_RIGHT; drawBox(K_RIGHT, false); }
  if (e == InputEvent::NavLeft || e == InputEvent::NavRight) drawSeenCount();
}

void PageLight::update() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < LIGHT_FRAME_MS) return;
  last = now;

  uint8_t downMask = 0;
  bool changed = false;
  for (int i = 0; i < K_COUNT; i++) {
    bool down = isKeyDown(KEY_DEFS[i].pin);
    if (down) {
      downMask |= 1 << i;
      if (!(s_seen & (1 << i))) { s_seen |= 1 << i; changed = true; }
    }
  }
  if (downMask != s_lastDown) changed = true;
  s_lastDown = downMask;
  if (changed) { drawAllBoxes(); drawSeenCount(); }
  drawLight(false);
}

void PageLight::onInput(InputEvent e) {
  if (e == InputEvent::KeyA) {
    s_seen |= 1 << K_A;
    drawBox(K_A, true);
    playTone(262, 120);
    drawBox(K_A, isKeyDown(KEY_DEFS[K_A].pin));
    drawSeenCount();
  } else if (e == InputEvent::KeyB) {
    s_seen |= 1 << K_B;
    drawBox(K_B, true);
    playTone(330, 120);
    drawBox(K_B, isKeyDown(KEY_DEFS[K_B].pin));
    drawSeenCount();
  } else if (e == InputEvent::KeyC) {
    s_seen |= 1 << K_C;
    drawBox(K_C, true);
    playTone(392, 120);
    drawBox(K_C, isKeyDown(KEY_DEFS[K_C].pin));
    drawSeenCount();
  } else if (e == InputEvent::NavUp) {
    s_seen |= 1 << K_UP;
    drawBox(K_UP, true);
    drawSeenCount();
  } else if (e == InputEvent::NavDown) {
    s_seen |= 1 << K_DOWN;
    drawBox(K_DOWN, true);
    drawSeenCount();
  } else if (e == InputEvent::NavPress) {
    s_seen = 0;
    s_lastDown = 0;
    drawAllBoxes();
    s_seen |= 1 << K_PRESS;
    drawBox(K_PRESS, true);
    playTone(880, 30);
    drawBox(K_PRESS, isKeyDown(KEY_DEFS[K_PRESS].pin));
    drawSeenCount();
  } else if (e == InputEvent::NavLeft) {
    s_seen |= 1 << K_LEFT;
    drawBox(K_LEFT, true);
    drawSeenCount();
  } else if (e == InputEvent::NavRight) {
    s_seen |= 1 << K_RIGHT;
    drawBox(K_RIGHT, true);
    drawSeenCount();
  }
}
