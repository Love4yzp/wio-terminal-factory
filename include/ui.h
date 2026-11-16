#pragma once
#include <TFT_eSPI.h>
#include "report.h"
extern TFT_eSPI tft;
void uiInit();
void uiProgress(const char *title, const char *msg);
void uiSummary(uint32_t totalMs);
void backlight(uint8_t level);
