// Wio Terminal Phase 1 售后检修 / 整机测试固件
// D-009: zero-human-blocking 6-item auto check + 6-tab interactive dashboard.
#include <Arduino.h>
#include "test_config.h"
#include "report.h"
#include "tests.h"
#include "ui.h"
#include "input.h"
#include "page.h"

static PageAuto   s_auto;
static PageWifi   s_wifi;
static PageImu    s_imu;
static PageAudio  s_audio;
static PageLight  s_light;
static PageSystem s_system;

static Page *s_pages[] = {&s_auto, &s_wifi, &s_imu, &s_audio, &s_light, &s_system};
static const int PAGE_COUNT = sizeof(s_pages) / sizeof(s_pages[0]);
static int s_page = 0;

static void switchTo(int idx, InputEvent via) {
  if (idx < 0) idx = PAGE_COUNT - 1;
  if (idx >= PAGE_COUNT) idx = 0;
  s_pages[s_page]->leave();
  s_page = idx;
  tft.fillScreen(C::bg);
  drawHeader(s_pages[s_page]->title(), s_page, PAGE_COUNT);
  drawFooter(s_pages[s_page]->hint());
  s_pages[s_page]->enter();
  s_pages[s_page]->onNavigate(via);
}

static void handleSerial() {
  while (Serial.available()) {
    char c = tolower(Serial.read());
    if (c == 'v') {
      printVersion();
    } else if (c == 'r') {
      Serial.println();
      reportPrintFinal(g_totalMs);
    } else if (c == 't') {
      Serial.println(F("REBOOT: NVIC_SystemReset"));
      Serial.flush();
      delay(30);
      NVIC_SystemReset();
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(115200);
  uiInit();
  uiBootProgress(0, AUTO_CHECK_ITEMS, "BOOT", "USB power only; no PC required");

  uint32_t t = millis();
  while (!Serial && millis() - t < BOOT_SERIAL_WAIT_MS) delay(10);  // 无串口也继续

  Serial.println();
  Serial.print(F("==== Wio Terminal Factory Test "));
  Serial.print(FW_VERSION);
  Serial.print(F(" ("));
  Serial.print(FW_BUILD_ID);
  Serial.println(F(") ===="));
  Serial.println(F("D-009: 6 auto checks then 6 tabs; no human blocking."));
  printVersion();

  hardwareBegin();
  runAllAutoTests(true);
  switchTo(0, InputEvent::None);
}

void loop() {
  ledHeartbeat();
  handleSerial();

  InputEvent e = pollInput();
  if (e == InputEvent::NavLeft) { switchTo(s_page - 1, e); return; }
  if (e == InputEvent::NavRight) { switchTo(s_page + 1, e); return; }
  if (e != InputEvent::None) s_pages[s_page]->onInput(e);

  if (s_pages[s_page]->takeFullRedraw()) { switchTo(s_page, InputEvent::None); return; }
  s_pages[s_page]->update();
  delay(2);
}
