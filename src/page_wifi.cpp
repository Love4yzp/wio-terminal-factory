#include "page.h"
#include "ui.h"
#include "report.h"
#include "tests.h"
#include "test_config.h"

const char *PageWifi::title() const { return "Wi-Fi / Net"; }
const char *PageWifi::hint() const { return "A re-scan + DHCP   L/R pages"; }

static void rssiBar(int y, int rssi) {
  const int x = 94, w = 216, h = 12;
  tft.fillRect(x, y, w, h, C::panel);
  int pct = (rssi + 100) * 100 / 70;  // -100 dBm .. -30 dBm
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  int fill = w * pct / 100;
  uint16_t col = rssi >= -60 ? C::ok : (rssi >= WIFI_RSSI_MIN_DBM ? C::accent2 : C::err);
  if (fill > 0) tft.fillRect(x, y, fill, h, col);
  // -70 dBm 门限刻度
  int mark = x + w * 30 / 70;
  tft.drawFastVLine(mark, y - 1, h + 2, C::text);
}

static void drawAll() {
  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);

  bool wifiOk = g_results[T_WIFI].status == ST_PASS;
  tft.fillRoundRect(8, 34, 304, 26, 5, C::panel);
  tft.fillCircle(18, 47, 5, wifiOk ? C::ok : C::err);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(wifiOk ? C::text : C::err, C::panel);
  tft.setTextSize(1);
  char status[42];
  snprintf(status, sizeof(status), "%.35s", g_wifiStatus);
  tft.drawString(status, 30, 47);

  tft.setTextColor(C::dim, C::bg);
  tft.drawString("RTL8720", 10, 70);
  tft.drawString("MAC", 10, 86);
  tft.setTextColor(C::text, C::bg);
  char tmp[44];
  snprintf(tmp, sizeof(tmp), "%.40s", g_rtlVer);
  tft.drawString(tmp, 52, 70);
  tft.drawString(g_mac, 52, 86);

  tft.setTextColor(C::dim, C::bg);
  tft.drawString("2.4G", 10, 106);
  tft.drawString("5G", 10, 132);
  tft.setTextColor(C::text, C::bg);
  char r2[16], r5[16];
  snprintf(r2, sizeof(r2), "%d dBm", g_rssi2);
  snprintf(r5, sizeof(r5), "%d dBm", g_rssi5);
  tft.drawString(r2, 44, 106);
  tft.drawString(r5, 44, 132);
  rssiBar(100, g_rssi2);
  rssiBar(126, g_rssi5);

  tft.setTextColor(C::dim, C::bg);
  tft.drawString("SSID", 10, 156);
  tft.drawString("IP", 10, 174);
  tft.drawString("GW", 10, 192);
  tft.setTextColor(C::text, C::bg);
  char ssid[36];
  snprintf(ssid, sizeof(ssid), "%.30s", g_ssid);
  tft.drawString(ssid, 52, 156);
  tft.drawString(g_ip, 52, 174);
  tft.drawString(g_gw, 52, 192);
}

void PageWifi::enter() { drawAll(); }
void PageWifi::update() {}

void PageWifi::onInput(InputEvent e) {
  if (e == InputEvent::KeyA) {
    uiBusy("Wi-Fi / Net", "2.4G/5G scan + DHCP connect ...");
    testWireless();
    printVersion();
    reportPrintFinal(g_totalMs);
    _redraw = true;
  }
}
