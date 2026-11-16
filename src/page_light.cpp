#include "page.h"
#include "ui.h"
#include "tests.h"
#include "test_config.h"

const char *PageLight::title() const { return "Light & IO"; }

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

static const int BADGE_X = 196, BADGE_Y = 30, BADGE_W = 114, BADGE_H = 14;

static uint8_t s_seen = 0;
static uint8_t s_lastDown = 0;
static int s_lastRaw = -1;
static bool s_locked = false;
static bool s_victoryPlayed = false;

const char *PageLight::hint() const {
  return s_locked ? "KEY LOCKED: test L/R/all 8   PRESS unlock"
                  : "PRESS key-lock   A/B/C tones   L/R tab";
}

bool PageLight::capturesNav() const { return s_locked; }

static int seenCount() {
  int n = 0;
  for (int i = 0; i < K_COUNT; i++) if (s_seen & (1 << i)) n++;
  return n;
}

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
  int n = seenCount();
  char buf[16];
  if (n == K_COUNT) snprintf(buf, sizeof(buf), "%d/%d PASS", n, K_COUNT);
  else snprintf(buf, sizeof(buf), "%d/%d", n, K_COUNT);

  tft.fillRect(112, 164, 76, 12, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(n == K_COUNT ? C::ok : C::dim, C::bg);
  tft.drawString("SEEN", 78, 170);
  tft.setTextColor(n == K_COUNT ? C::ok : C::text, C::bg);
  tft.drawString(buf, 112, 170);
}

static void checkVictory() {
  const uint8_t allSeen = (uint8_t)((1u << K_COUNT) - 1u);
  if (s_seen == allSeen && !s_victoryPlayed) {
    s_victoryPlayed = true;
    playTone(523, 60);
    playTone(659, 60);
    playTone(784, 90);
    drawSeenCount();
  }
}

static void markSeen(int i) {
  s_seen |= 1 << i;
  drawBox(i, isKeyDown(KEY_DEFS[i].pin));
  drawSeenCount();
  checkVictory();
}

static void drawLockBadge() {
  tft.fillRect(78, 30, 116, 12, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(s_locked ? C::accent2 : C::dim, C::bg);
  tft.drawString(s_locked ? "L/R captured" : "PRESS: lock L/R", 78, 36);

  tft.fillRoundRect(BADGE_X, BADGE_Y, BADGE_W, BADGE_H, 4, C::panel);
  tft.drawRoundRect(BADGE_X, BADGE_Y, BADGE_W, BADGE_H, 4, s_locked ? C::accent2 : C::dim);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(s_locked ? C::accent2 : C::dim, C::panel);
  tft.drawString(s_locked ? "[KEY LOCK]" : "[UNLOCKED]", BADGE_X + BADGE_W / 2, BADGE_Y + BADGE_H / 2);
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
  tft.drawString("PRESS toggles lock; re-enter clears", 78, 202);
}

void PageLight::enter() {
  s_seen = 0;
  s_lastDown = 0;
  s_lastRaw = -1;
  s_locked = false;
  s_victoryPlayed = false;
  drawFrame();
  drawLockBadge();
  drawLight(true);
  drawSeenCount();
}

void PageLight::leave() {
  s_locked = false;
}

void PageLight::onNavigate(InputEvent e) {
  if (e == InputEvent::NavLeft) markSeen(K_LEFT);
  if (e == InputEvent::NavRight) markSeen(K_RIGHT);
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
  if (changed) {
    drawAllBoxes();
    drawSeenCount();
    checkVictory();
  }
  drawLight(false);
}

void PageLight::onInput(InputEvent e) {
  if (e == InputEvent::KeyA) {
    s_seen |= 1 << K_A;
    drawBox(K_A, true);
    drawSeenCount();
    playTone(262, 120);
    drawBox(K_A, isKeyDown(KEY_DEFS[K_A].pin));
    checkVictory();
  } else if (e == InputEvent::KeyB) {
    s_seen |= 1 << K_B;
    drawBox(K_B, true);
    drawSeenCount();
    playTone(330, 120);
    drawBox(K_B, isKeyDown(KEY_DEFS[K_B].pin));
    checkVictory();
  } else if (e == InputEvent::KeyC) {
    s_seen |= 1 << K_C;
    drawBox(K_C, true);
    drawSeenCount();
    playTone(392, 120);
    drawBox(K_C, isKeyDown(KEY_DEFS[K_C].pin));
    checkVictory();
  } else if (e == InputEvent::NavUp) {
    markSeen(K_UP);
  } else if (e == InputEvent::NavDown) {
    markSeen(K_DOWN);
  } else if (e == InputEvent::NavLeft) {
    markSeen(K_LEFT);
  } else if (e == InputEvent::NavRight) {
    markSeen(K_RIGHT);
  } else if (e == InputEvent::NavPress) {
    s_seen |= 1 << K_PRESS;
    s_locked = !s_locked;
    playTone(880, 30);
    drawLockBadge();
    drawFooter(hint());
    drawBox(K_PRESS, isKeyDown(KEY_DEFS[K_PRESS].pin));
    drawSeenCount();
    checkVictory();
  }
}
