#include "report.h"
#include "test_config.h"
#include <stdarg.h>

TestResult g_results[T_COUNT] = {
  {"STUCK_KEY", ST_PENDING, "", 0}, {"QSPI_FLASH", ST_PENDING, "", 0},
  {"ACCEL", ST_PENDING, "", 0},     {"BUZZER_MIC", ST_PENDING, "", 0},
  {"RTL8720", ST_PENDING, "", 0},   {"WIFI_2G5G", ST_PENDING, "", 0},
  {"LCD_BL", ST_PENDING, "", 0},    {"KEYS_5WAY", ST_PENDING, "", 0},
  {"LIGHT", ST_PENDING, "", 0},
};
char g_mac[24] = "N/A";
char g_rtlVer[48] = "N/A";
bool g_wifiFallback = false;

void reportSet(TestId id, bool pass, const char *fmt, ...) {
  TestResult &r = g_results[id];
  r.status = pass ? ST_PASS : ST_FAIL;
  va_list ap; va_start(ap, fmt);
  vsnprintf(r.detail, sizeof(r.detail), fmt, ap);
  va_end(ap);
  // 避免 SAMD Print::printf 80 字节缓冲区截断换行
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
  bool ok = reportAllPass();
  Serial.println(F("\r\n+------------+------+--------+------------------------------------------------+"));
  Serial.println(F("| ITEM       | RES  | ms     | DETAIL                                         |"));
  Serial.println(F("+------------+------+--------+------------------------------------------------+"));
  for (auto &r : g_results) {
    Serial.printf("| %-10s | %-4s | %6lu | ", r.name,
                  r.status == ST_PASS ? "OK" : "FAIL", (unsigned long)r.ms);
    Serial.printf("%-46.46s |\r\n", r.detail);
  }
  Serial.println(F("+------------+------+--------+------------------------------------------------+"));
  Serial.printf("RESULT: %s  MAC: %s\r\n", ok ? "PASS" : "REPAIR", g_mac);
  Serial.printf("RTL FW: %s  TIME: %lu ms  FW: ", g_rtlVer, (unsigned long)totalMs);
  Serial.println(FW_VERSION);
  if (g_wifiFallback) Serial.println(F("NOTE: WIFI SSID not configured -> fallback band check"));
  Serial.print(F("JSON:{\"fw\":")); jsonStr(FW_VERSION);
  Serial.print(F(",\"build_id\":")); jsonStr(FW_BUILD_ID);
  Serial.printf(",\"result\":\"%s\",\"total_ms\":%lu,", ok ? "PASS" : "REPAIR", (unsigned long)totalMs);
  Serial.print(F("\"mac\":")); jsonStr(g_mac);
  Serial.print(F(",\"rtl_fw\":")); jsonStr(g_rtlVer);
  Serial.printf(",\"wifi_fallback\":%s,\"items\":[", g_wifiFallback ? "true" : "false");
  for (int i = 0; i < T_COUNT; i++) {
    Serial.printf("%s{\"name\":\"%s\",\"pass\":%s,", i ? "," : "", g_results[i].name,
                  g_results[i].status == ST_PASS ? "true" : "false");
    Serial.printf("\"ms\":%lu,\"detail\":", (unsigned long)g_results[i].ms);
    jsonStr(g_results[i].detail); Serial.print('}');
  }
  Serial.println(F("]}"));
}
