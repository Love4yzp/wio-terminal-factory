#include "page.h"
#include "ui.h"
#include "report.h"
#include "tests.h"
#include "test_config.h"

const char *PageAuto::title() const { return "Auto Check"; }
const char *PageAuto::hint() const { return "A re-run all 6 checks   L/R pages"; }

static void drawBanner() {
  bool ok = reportAllPass();
  const int y = 34;
  tft.fillRoundRect(8, y, 304, 50, 6, C::panel);
  tft.drawRoundRect(8, y, 304, 50, 6, ok ? C::ok : C::err);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(ok ? C::ok : C::err, C::panel);
  tft.setTextSize(4);
  tft.drawString(ok ? "PASS" : "REPAIR", 18, y + 25);

  char buf[40];
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(C::text, C::panel);
  tft.setTextSize(2);
  snprintf(buf, sizeof(buf), "%.1fs", g_totalMs / 1000.0f);
  tft.drawString(buf, 302, y + 15);

  tft.setTextColor(C::dim, C::panel);
  tft.setTextSize(1);
  snprintf(buf, sizeof(buf), "build %s", FW_BUILD_ID);
  tft.drawString(buf, 302, y + 36);
}

static void drawRows() {
  int y = 92;
  for (int i = 0; i < T_COUNT; i++) {
    bool ok = g_results[i].status == ST_PASS;
    tft.fillCircle(14, y + 7, 5, ok ? C::ok : C::err);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C::text, C::bg);
    tft.setTextSize(1);
    tft.drawString(g_results[i].name, 26, y + 7);

    char detail[40];
    snprintf(detail, sizeof(detail), "%.31s", g_results[i].detail[0] ? g_results[i].detail : "-");
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(ok ? C::dim : C::err, C::bg);
    tft.drawString(detail, 312, y + 7);
    y += 21;
  }
}

void PageAuto::enter() {
  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);
  drawBanner();
  drawRows();
}

void PageAuto::update() {}

void PageAuto::onInput(InputEvent e) {
  if (e == InputEvent::KeyA) {
    runAllAutoTests(true);
    _redraw = true;
  }
}
