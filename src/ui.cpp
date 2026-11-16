#include "ui.h"
#include "test_config.h"

TFT_eSPI tft;

// PC05 在 SAMD51 variant 中是 NOT_ON_PWM，且 analogWrite 默认 12bit 分辨率：
// analogWrite(…,255) 会退化成 digitalWrite(LOW) 把背光关掉。所以只用数字开关。
void backlight(uint8_t level) { digitalWrite(LCD_BACKLIGHT, level ? HIGH : LOW); }

void uiInit() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  pinMode(LCD_BACKLIGHT, OUTPUT);
  backlight(255);
}

void uiProgress(const char *title, const char *msg) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 30, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(2);
  tft.drawString("Wio Factory Test", 6, 7);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(title, 10, 60);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString(msg, 10, 100);
}

void uiSummary(uint32_t totalMs) {
  bool ok = reportAllPass();
  tft.fillScreen(TFT_BLACK);
  uint16_t bg = ok ? TFT_DARKGREEN : TFT_RED;
  tft.fillRect(0, 0, 320, 44, bg);
  tft.setTextColor(TFT_WHITE, bg);
  tft.setTextSize(4);
  tft.drawString(ok ? "PASS" : "REPAIR", ok ? 112 : 88, 8);
  tft.setTextSize(1);
  int y = 52;
  for (auto &r : g_results) {
    bool p = r.status == ST_PASS;
    tft.setTextColor(p ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(p ? "[OK]  " : "[FAIL]", 4, y);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char line[60];
    snprintf(line, sizeof(line), "%-10s %.40s", r.name, r.detail);
    tft.drawString(line, 46, y);
    y += 17;
  }
  tft.drawFastHLine(0, 208, 320, TFT_DARKGREY);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  char buf[80];
  snprintf(buf, sizeof(buf), "MAC %s  RTL %.20s", g_mac, g_rtlVer);
  tft.drawString(buf, 4, 214);
  snprintf(buf, sizeof(buf), "Time %.1fs  %s", totalMs / 1000.0f, FW_VERSION);
  tft.drawString(buf, 4, 226);
}
