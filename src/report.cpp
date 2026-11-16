#include "report.h"
#include "test_config.h"
#include <stdarg.h>

TestResult g_results[T_COUNT] = {
  {"STUCK_KEY", ST_PENDING, "", 0}, {"QSPI_FLASH", ST_PENDING, "", 0},
  {"ACCEL", ST_PENDING, "", 0},     {"BUZZER_MIC", ST_PENDING, "", 0},
  {"RTL8720", ST_PENDING, "", 0},   {"WIFI_2G5G", ST_PENDING, "", 0},
};
char g_mac[24] = "N/A";
char g_rtlVer[48] = "N/A";
bool g_wifiFallback = false;
int g_rssi2 = -127;
int g_rssi5 = -127;
char g_ssid[33] = "-";
char g_ip[20] = "-";
char g_gw[20] = "-";
char g_wifiStatus[48] = "not tested";
uint32_t g_qspiCapacity = 0;
uint32_t g_totalMs = 0;

void reportSet(TestId id, bool pass, const char *fmt, ...) {
  TestResult &r = g_results[id];
  r.status = pass ? ST_PASS : ST_FAIL;
  va_list ap; va_start(ap, fmt);
  vsnprintf(r.detail, sizeof(r.detail), fmt, ap);
  va_end(ap);
  // SAMD Print::printf 只有 80 字节缓冲：前缀短、详情单独 println
  Serial.printf("[%s] %-10s ", pass ? " OK " : "FAIL", r.name);
  Serial.println(r.detail);
}

bool reportAllPass() {
  for (auto &r : g_results) if (r.status != ST_PASS) return false;
  return true;
}

static void jsonStr(const char *s) {
  Serial.print('"');
  for (; *s; s++) { if (*s == '"' || *s == '\\') Serial.print('\\'); Serial.print(*s); }
  Serial.print('"');
}

void printVersion() { Serial.printf("FW_VER:%s|%s\r\n", FW_VERSION, FW_BUILD_ID); }

void reportPrintFinal(uint32_t totalMs) {
  g_totalMs = totalMs;
  bool ok = reportAllPass();
  Serial.println(F(""));
  Serial.println(F("+------------+------+--------+------------------------------------------------+"));
  Serial.println(F("| ITEM       | RES  | ms     | DETAIL                                         |"));
  Serial.println(F("+------------+------+--------+------------------------------------------------+"));
  for (auto &r : g_results) {
    Serial.printf("| %-10s | %-4s | %6lu | ", r.name,
                  r.status == ST_PASS ? "OK" : "FAIL", (unsigned long)r.ms);
    Serial.printf("%-46.46s |\r\n", r.detail);
  }
  Serial.println(F("+------------+------+--------+------------------------------------------------+"));
  Serial.print(F("RESULT: ")); Serial.print(ok ? F("PASS") : F("REPAIR"));
  Serial.print(F("  MAC: ")); Serial.println(g_mac);
  Serial.print(F("RTL FW: ")); Serial.print(g_rtlVer);
  Serial.print(F("  TIME: ")); Serial.print((unsigned long)totalMs);
  Serial.print(F(" ms  FW: ")); Serial.println(FW_VERSION);
  if (g_wifiFallback) Serial.println(F("NOTE: WIFI SSID not configured -> fallback band check"));

  Serial.print(F("JSON:{\"fw\":"));
  jsonStr(FW_VERSION);
  Serial.print(F(",\"build_id\":"));
  jsonStr(FW_BUILD_ID);
  Serial.print(F(",\"result\":\""));
  Serial.print(ok ? "PASS" : "REPAIR");
  Serial.print(F("\",\"total_ms\":"));
  Serial.print((unsigned long)totalMs);
  Serial.print(F(",\"mac\":"));
  jsonStr(g_mac);
  Serial.print(F(",\"rtl_fw\":"));
  jsonStr(g_rtlVer);
  Serial.print(F(",\"wifi_fallback\":"));
  Serial.print(g_wifiFallback ? F("true") : F("false"));
  Serial.print(F(",\"rssi_2g\":"));
  Serial.print(g_rssi2);
  Serial.print(F(",\"rssi_5g\":"));
  Serial.print(g_rssi5);
  Serial.print(F(",\"ssid\":"));
  jsonStr(g_ssid);
  Serial.print(F(",\"ip\":"));
  jsonStr(g_ip);
  Serial.print(F(",\"gw\":"));
  jsonStr(g_gw);
  Serial.print(F(",\"wifi_status\":"));
  jsonStr(g_wifiStatus);
  Serial.print(F(",\"items\":["));
  for (int i = 0; i < T_COUNT; i++) {
    if (i) Serial.print(',');
    Serial.print(F("{\"name\":\""));
    Serial.print(g_results[i].name);
    Serial.print(F("\",\"pass\":"));
    Serial.print(g_results[i].status == ST_PASS ? F("true") : F("false"));
    Serial.print(F(",\"ms\":"));
    Serial.print((unsigned long)g_results[i].ms);
    Serial.print(F(",\"detail\":"));
    jsonStr(g_results[i].detail);
    Serial.print('}');
  }
  Serial.println(F("]}"));
}
