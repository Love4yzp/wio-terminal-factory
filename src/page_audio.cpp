#include "page.h"
#include "ui.h"
#include "tests.h"
#include "test_config.h"

const char *PageAudio::title() const { return "Audio"; }
const char *PageAudio::hint() const { return "A freeze  B 4kHz beep  L/R pages"; }

static const int WX = 8, WY = 52, WW = 242, WH = 126;
static const int VX = 258, VY = 52, VW = 44, VH = 126;

static int16_t s_wave[WW];
static int s_writeIdx = 0;
static bool s_frozen = false;
static bool s_needDraw = true;
static float s_vu = 0;
static int s_peakHold = 0;
static uint32_t s_peakAt = 0;
static bool s_beeping = false;
static uint32_t s_beepUntil = 0;
static uint32_t s_lastFrame = 0;

static void drawFrame() {
  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  tft.drawString("WAVEFORM  WIO_MIC", WX, 36);
  tft.setTextDatum(ML_DATUM);
  char b[24];
  snprintf(b, sizeof(b), "bias %d", micBias());
  tft.drawString(b, 200, 36);
  tft.drawString("VU", VX, 36);

  tft.drawRect(WX - 1, WY - 1, WW + 2, WH + 2, C::panel);
  tft.drawFastHLine(WX, WY + WH / 2, WW, C::panel);
  tft.drawRect(VX, VY, VW, VH, C::panel);

  for (int i = 0; i < WW; i++) s_wave[i] = 0;
  s_writeIdx = 0;
  s_vu = 0;
  s_peakHold = 0;
  s_needDraw = true;
}

static void drawStatus(int maxAbs) {
  tft.fillRect(WX, 184, 300, 22, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  char buf[48];
  snprintf(buf, sizeof(buf), "peak %3d   vu %3d%%", maxAbs, (int)(s_vu * 100));
  tft.drawString(buf, WX, 194);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(s_frozen ? C::err : C::ok, C::bg);
  tft.drawString(s_frozen ? "FROZEN - A resume" : "LIVE", WX + 300, 194);
}

static void drawWave() {
  tft.fillRect(WX, WY, WW, WH, C::bg);
  tft.drawFastHLine(WX, WY + WH / 2, WW, C::panel);
  int prevY = WY + WH / 2;
  for (int x = 0; x < WW; x++) {
    int idx = (s_writeIdx + x) % WW;
    int v = s_wave[idx];
    int y = WY + WH / 2 - (v * (WH / 2)) / 512;
    if (y < WY) y = WY;
    if (y >= WY + WH) y = WY + WH - 1;
    if (x > 0) tft.drawLine(WX + x - 1, prevY, WX + x, y, C::accent);
    prevY = y;
  }

  const int barMax = VH - 4;
  tft.fillRect(VX + 1, VY + 1, VW - 2, VH - 2, C::bg);
  int barH = (int)(s_vu * barMax);
  if (barH < 0) barH = 0;
  if (barH > barMax) barH = barMax;
  uint16_t col = barH > barMax * 8 / 10 ? C::err : (barH > barMax * 5 / 10 ? C::accent2 : C::ok);
  tft.fillRect(VX + 2, VY + 2 + (barMax - barH), VW - 4, barH, col);

  int pkBar = (s_peakHold * barMax) / 512;
  if (pkBar > 0) tft.drawFastHLine(VX + 2, VY + 2 + (barMax - pkBar), VW - 4, C::text);
}

void PageAudio::enter() {
  toneStop();
  s_beeping = false;
  s_frozen = false;
  s_lastFrame = 0;
  micCalibrate();
  drawFrame();
}

void PageAudio::leave() {
  toneStop();
  s_beeping = false;
}

void PageAudio::update() {
  uint32_t now = millis();
  if (s_beeping && now >= s_beepUntil) {
    toneStop();
    s_beeping = false;
    s_needDraw = true;
  }
  if (now - s_lastFrame < AUDIO_FRAME_MS) return;
  s_lastFrame = now;

  if (!s_frozen) {
    int samples = 24;
    int maxAbs = 0;
    for (int i = 0; i < samples; i++) {
      int v = readMicRaw() - micBias();
      if (v < -512) v = -512;
      if (v > 511) v = 511;
      int a = v < 0 ? -v : v;
      if (a > maxAbs) maxAbs = a;
      s_wave[s_writeIdx] = (int16_t)v;
      s_writeIdx = (s_writeIdx + 1) % WW;
      delayMicroseconds(60);
    }
    s_vu = s_vu * 0.85f + (maxAbs / 512.0f) * 0.15f;
    if (maxAbs > s_peakHold) { s_peakHold = maxAbs; s_peakAt = now; }
    if (now - s_peakAt > 400 && s_peakHold > 0) s_peakHold -= 6;
    if (s_peakHold < 0) s_peakHold = 0;

    drawWave();
    drawStatus(maxAbs);
    s_needDraw = false;
  } else if (s_needDraw) {
    drawWave();
    drawStatus(s_peakHold);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(C::err, C::bg);
    tft.setTextSize(1);
    tft.drawString("FROZEN", WX + WW - 4, WY + 4);
    s_needDraw = false;
  }
}

void PageAudio::onInput(InputEvent e) {
  if (e == InputEvent::KeyA) {
    s_frozen = !s_frozen;
    s_needDraw = true;
  } else if (e == InputEvent::KeyB) {
    if (s_frozen) { s_frozen = false; s_needDraw = true; }
    s_beeping = true;
    s_beepUntil = millis() + AUDIO_BEEP_MS;
    toneStart(AUDIO_TONE_HZ);
  } else if (e == InputEvent::NavPress) {
    s_frozen = false;
    micCalibrate();
    s_needDraw = true;
  }
}
