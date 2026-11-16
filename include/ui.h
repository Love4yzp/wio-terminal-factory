#pragma once
#include <TFT_eSPI.h>

namespace C {
  constexpr uint16_t bg      = 0x0841;
  constexpr uint16_t panel   = 0x18C3;
  constexpr uint16_t accent  = 0x07FF;
  constexpr uint16_t accent2 = 0xFD20;
  constexpr uint16_t ok      = 0x07E0;
  constexpr uint16_t err     = 0xF800;
  constexpr uint16_t dim     = 0x8410;
  constexpr uint16_t text    = 0xFFFF;
}

extern TFT_eSPI tft;

void uiInit();
void backlight(uint8_t level);  // PC05 is NOT_ON_PWM: digital HIGH/LOW only
void drawHeader(const char *title, int pageIdx, int pageCount);
void drawFooter(const char *hint);
void uiBootProgress(int step, int total, const char *title, const char *msg);
void uiBusy(const char *title, const char *msg);
int bodyTop();
int bodyBottom();
