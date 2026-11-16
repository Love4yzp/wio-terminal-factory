#include "page.h"
#include "ui.h"
#include "report.h"
#include "test_config.h"

const char *PageSystem::title() const { return "LCD & Sys"; }
const char *PageSystem::hint() const { return "A color cycle + dim   L/R pages"; }

extern "C" char *sbrk(int i);
static int freeMemory() {
  char stackTop;
  return &stackTop - reinterpret_cast<char *>(sbrk(0));
}

static void drawSwatches() {
  const uint16_t cols[5] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
  const char *labels[5] = {"R", "G", "B", "W", "K"};
  for (int i = 0; i < 5; i++) {
    int x = 10 + i * 60;
    tft.fillRoundRect(x, 34, 54, 30, 4, cols[i]);
    tft.drawRoundRect(x, 34, 54, 30, 4, C::dim);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor((i == 0 || i == 1 || i == 3) ? C::bg : C::text, cols[i]);
    tft.setTextSize(2);
    tft.drawString(labels[i], x + 27, 49);
  }

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  tft.drawString("GRAYSCALE 0..255", 10, 72);
  for (int i = 0; i < 16; i++) {
    uint8_t v = (uint8_t)(i * 17);
    tft.fillRect(10 + i * 18, 82, 18, 16, tft.color565(v, v, v));
  }
  tft.drawRect(10, 82, 288, 16, C::dim);
}

static void drawValue(int y, const char *text, uint16_t color) {
  tft.fillRect(64, y, 246, 16, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, C::bg);
  tft.setTextSize(1);
  tft.drawString(text, 64, y + 8);
}

static void drawInfo() {
  const int y0 = 106, step = 18;
  const char *labels[6] = {"FW", "BUILD", "QSPI", "FREE RAM", "UPTIME", "RESULT"};
  for (int i = 0; i < 6; i++) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C::dim, C::bg);
    tft.setTextSize(1);
    tft.drawString(labels[i], 10, y0 + i * step + 8);
  }

  drawValue(y0, FW_VERSION, C::text);
  drawValue(y0 + step, FW_BUILD_ID, C::text);

  char buf[40];
  if (g_qspiCapacity) {
    snprintf(buf, sizeof(buf), "%luMB %s", (unsigned long)(g_qspiCapacity / (1024UL * 1024UL)),
             g_results[T_QSPI].status == ST_PASS ? "OK" : "FAIL");
  } else {
    snprintf(buf, sizeof(buf), "not detected");
  }
  drawValue(y0 + step * 2, buf, g_results[T_QSPI].status == ST_PASS ? C::ok : C::err);
  drawValue(y0 + step * 3, "-", C::text);
  drawValue(y0 + step * 4, "-", C::text);
  drawValue(y0 + step * 5, reportAllPass() ? "PASS" : "REPAIR", reportAllPass() ? C::ok : C::err);
}

static void updateDynamics() {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d bytes", freeMemory());
  drawValue(106 + 18 * 3, buf, C::text);

  uint32_t s = millis() / 1000;
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)(s / 3600),
           (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  drawValue(106 + 18 * 4, buf, C::text);
  drawValue(106 + 18 * 5, reportAllPass() ? "PASS" : "REPAIR", reportAllPass() ? C::ok : C::err);
}

void PageSystem::enter() {
  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);
  drawSwatches();
  drawInfo();
  updateDynamics();
}

void PageSystem::update() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < SYS_FRAME_MS) return;
  last = now;
  updateDynamics();
}

void PageSystem::onInput(InputEvent e) {
  if (e != InputEvent::KeyA) return;
  _redraw = true;

  const uint16_t colors[5] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
  for (int i = 0; i < 5; i++) {
    tft.fillScreen(colors[i]);
    delay(LCD_COLOR_MS);
  }

  // 1kHz 软件 PWM 背光渐变 0 -> 100 -> 0%，最后恢复 HIGH。
  // PC05 不是 PWM 引脚，不能用 analogWrite（SAMD51 默认 12bit 会误关背光）。
  tft.fillScreen(TFT_WHITE);
  for (int step = 0; step <= LCD_RAMP_STEPS; step++) {
    int duty = step <= LCD_RAMP_STEPS / 2 ? step * 25 : (LCD_RAMP_STEPS - step) * 25;
    for (int cyc = 0; cyc < LCD_RAMP_CYCLES; cyc++) {
      if (duty > 0) { backlight(1); delayMicroseconds(duty); }
      if (duty < LCD_RAMP_HALF_US) { backlight(0); delayMicroseconds(LCD_RAMP_HALF_US - duty); }
    }
  }
  backlight(255);
}
