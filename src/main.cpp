// Wio Terminal Phase 1 售后检修 / 整机测试固件
#include <Arduino.h>
#include "test_config.h"
#include "report.h"
#include "tests.h"
#include "ui.h"

static uint32_t s_total = 0;
static bool s_pass = false;

static void run(TestId id, const char *title, const char *msg, void (*fn)()) {
  Serial.printf("[ .. ] %-10s start\r\n", g_results[id].name);
  if (title) uiProgress(title, msg);
  uint32_t t = millis();
  fn();
  g_results[id].ms = millis() - t;
  if (g_results[id].status == ST_PENDING) reportSet(id, false, "no result");
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && millis() - t < 1500) ledTick();   // 无串口也继续
  uint32_t t0 = millis();
  Serial.printf("\r\n==== Wio Terminal Factory Test %s (%s) ====\r\n", FW_VERSION, FW_BUILD_ID);
  printVersion();
  Serial.println(F("Manual steps fallback via serial: p=pass f=fail s=skip(fail) v=version; after summary: r=report t=retest"));

  run(T_STUCK, nullptr, nullptr, testStuckKeys);    // 最先：未操作前采样
  uiInit();
  run(T_QSPI,  "Auto 1/4: QSPI Flash", "Erase / write / verify test sector", testQspi);
  run(T_ACC,   "Auto 2/4: Accelerometer", "Keep device still", testAccel);
  run(T_AUDIO, "Auto 3/4: Buzzer + Mic", "Beep 4kHz...", testAudio);
  uiProgress("Auto 4/4: RTL8720 / WiFi", "eRPC handshake + 2.4G/5G scan");
  {
    uint32_t t = millis();
    testRtlAndWifi();
    g_results[T_RTL].ms = g_results[T_WIFI].ms = millis() - t;
  }
  run(T_LCD,   nullptr, nullptr, testLcd);
  run(T_KEYS,  nullptr, nullptr, testKeys);
  run(T_LIGHT, nullptr, nullptr, testLight);

  s_total = millis() - t0;
  s_pass = reportAllPass();
  uiSummary(s_total);
  reportPrintFinal(s_total);
}

void loop() {
  // PASS 慢闪 (1Hz)，FAIL 快闪 (5Hz)
  static uint32_t t = 0;
  if (millis() - t >= (s_pass ? 500u : 100u)) { t = millis(); digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }
  // 串口输入 'r' 重新打印报告
  if (Serial.available()) {
    char c = tolower(Serial.read());
    if (c == 'r') reportPrintFinal(s_total);
    else if (c == 'v') printVersion();
    else if (c == 't') { Serial.println(F("RETEST: rebooting")); Serial.flush(); delay(50); NVIC_SystemReset(); }
  }
}
