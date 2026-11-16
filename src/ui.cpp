#include "ui.h"
#include "test_config.h"

TFT_eSPI tft;

static const int HEADER_H = 28;
static const int FOOTER_H = 20;

// PC05 在 SAMD51 variant 中是 NOT_ON_PWM，且 analogWrite 默认 12bit：
// analogWrite(...,255) 会退化成 digitalWrite(LOW) 把背光关掉。只做数字开关。
void backlight(uint8_t level) { digitalWrite(LCD_BACKLIGHT, level ? HIGH : LOW); }

void uiInit() {
  pinMode(LCD_BACKLIGHT, OUTPUT);
  backlight(255);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(C::bg);
  tft.setTextDatum(TL_DATUM);
}

void drawHeader(const char *title, int pageIdx, int pageCount) {
  tft.fillRect(0, 0, 320, HEADER_H, C::panel);
  tft.drawFastHLine(0, HEADER_H, 320, C::accent);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::text, C::panel);
  tft.setTextSize(2);
  tft.drawString(title, 8, HEADER_H / 2);

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(C::dim, C::panel);
  tft.setTextSize(1);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d / %d", pageIdx + 1, pageCount);
  tft.drawString(buf, 312, HEADER_H / 2);

  for (int i = 0; i < pageCount; i++) {
    int cx = 216 + i * 11;
    tft.fillCircle(cx, HEADER_H / 2, 2, i == pageIdx ? C::accent : C::dim);
  }
}

void drawFooter(const char *hint) {
  int y = 240 - FOOTER_H;
  tft.fillRect(0, y, 320, FOOTER_H, C::panel);
  tft.drawFastHLine(0, y, 320, C::accent);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::dim, C::panel);
  tft.setTextSize(1);
  tft.drawString(hint, 8, y + FOOTER_H / 2);
}

void uiBootProgress(int step, int total, const char *title, const char *msg) {
  drawHeader("Auto Check", 0, total);
  drawFooter("USB power only - no PC required");

  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C::accent, C::bg);
  tft.setTextSize(3);
  tft.drawString("Wio Factory Test", 160, 58);

  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  tft.drawString("zero-human auto check  /  D-009", 160, 82);

  tft.fillRoundRect(12, 98, 296, 62, 6, C::panel);
  tft.drawRoundRect(12, 98, 296, 62, 6, C::accent);
  tft.setTextColor(C::text, C::panel);
  tft.setTextSize(2);
  tft.drawString(title, 160, 116);
  tft.setTextColor(C::dim, C::panel);
  tft.setTextSize(1);
  tft.drawString(msg, 160, 140);

  for (int i = 0; i < total; i++) {
    int x = 18 + i * 48;
    uint16_t col = i < step ? C::ok : (i == step ? C::accent2 : C::dim);
    tft.fillRoundRect(x, 176, 42, 8, 3, col);
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "step %d / %d", step + 1, total);
  tft.setTextColor(C::text, C::bg);
  tft.setTextSize(1);
  tft.drawString(buf, 160, 198);
  tft.setTextColor(C::dim, C::bg);
  tft.drawString(FW_VERSION, 160, 210);
}

void uiBusy(const char *title, const char *msg) {
  drawFooter("Please wait - device is working");
  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);
  tft.fillRoundRect(16, 84, 288, 76, 6, C::panel);
  tft.drawRoundRect(16, 84, 288, 76, 6, C::accent2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C::text, C::panel);
  tft.setTextSize(2);
  tft.drawString(title, 160, 108);
  tft.setTextColor(C::dim, C::panel);
  tft.setTextSize(1);
  tft.drawString(msg, 160, 136);
}

int bodyTop() { return HEADER_H + 2; }
int bodyBottom() { return 240 - FOOTER_H - 2; }
