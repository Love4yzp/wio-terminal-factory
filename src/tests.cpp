#include <Arduino.h>
#include <Wire.h>
#include <sfud.h>
#include <LIS3DHTR.h>
#include <rpcWiFi.h>
#include "erpc/rpc_system.h"
#include "test_config.h"
#include "report.h"
#include "tests.h"
#include "ui.h"

// ---------------- 公共 ----------------
struct KeyDef { uint8_t pin; const char *name; };
static const KeyDef KEYS[8] = {
  {WIO_KEY_A, "KEY_A"}, {WIO_KEY_B, "KEY_B"}, {WIO_KEY_C, "KEY_C"},
  {WIO_5S_UP, "5S_UP"}, {WIO_5S_DOWN, "5S_DOWN"}, {WIO_5S_LEFT, "5S_LEFT"},
  {WIO_5S_RIGHT, "5S_RIGHT"}, {WIO_5S_PRESS, "5S_PRESS"},
};
static inline bool pressed(uint8_t i) { return digitalRead(KEYS[i].pin) == LOW; }

void ledTick() {
  if (Serial.available() && tolower(Serial.peek()) == 'v') { Serial.read(); printVersion(); }  // 任何等待中都能查版本
  static uint32_t t = 0;
  if (millis() - t >= 150) { t = millis(); digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }
}

char serialCmd() {
  while (Serial.available()) {
    char c = tolower(Serial.read());
    if (c == 'v') printVersion();
    else if (c == 'p' || c == 'f' || c == 's') return c;
  }
  return 0;
}

static void waitRelease(uint32_t maxMs) {
  uint32_t t = millis();
  while (millis() - t < maxMs) {
    bool any = false;
    for (int i = 0; i < 8; i++) any |= pressed(i);
    if (!any) break;
    ledTick();
  }
  delay(30);
}

// ---------------- 1. 按键粘连 ----------------
void testStuckKeys() {
  for (auto &k : KEYS) pinMode(k.pin, INPUT_PULLUP);
  delay(20);
  uint8_t stuckMask = 0xFF;
  uint32_t t = millis();
  while (millis() - t < STUCK_CHECK_MS) {    // 持续按下才算粘连
    for (int i = 0; i < 8; i++) if (!pressed(i)) stuckMask &= ~(1 << i);
    delay(5);
  }
  if (!stuckMask) { reportSet(T_STUCK, true, "no key held at boot"); return; }
  char buf[96] = "stuck:";
  for (int i = 0; i < 8; i++) if (stuckMask & (1 << i)) { strlcat(buf, " ", sizeof buf); strlcat(buf, KEYS[i].name, sizeof buf); }
  reportSet(T_STUCK, false, "%s", buf);
}

// ---------------- 2. QSPI Flash ----------------
static uint8_t s_buf[QSPI_TEST_SIZE];
#if QSPI_RESTORE_SECTOR
static uint8_t s_backup[QSPI_TEST_SIZE];
#endif
void testQspi() {
  if (sfud_init() != SFUD_SUCCESS) { reportSet(T_QSPI, false, "sfud_init failed (no JEDEC response)"); return; }
#ifdef SFUD_USING_QSPI
  if (sfud_qspi_fast_read_enable(sfud_get_device(SFUD_W25Q32_DEVICE_INDEX), 2) != SFUD_SUCCESS) {
    reportSet(T_QSPI, false, "qspi fast_read_enable failed"); return;
  }
#endif
  const sfud_flash *f = sfud_get_device(SFUD_W25Q32_DEVICE_INDEX);
  if (!f || f->chip.capacity < QSPI_TEST_ADDR + QSPI_TEST_SIZE) {
    reportSet(T_QSPI, false, "capacity %lu too small", f ? (unsigned long)f->chip.capacity : 0UL); return;
  }
#if QSPI_RESTORE_SECTOR
  if (sfud_read(f, QSPI_TEST_ADDR, QSPI_TEST_SIZE, s_backup) != SFUD_SUCCESS) { reportSet(T_QSPI, false, "backup read err"); return; }
#endif
  uint32_t seed = micros() | 1, x = seed;
  for (size_t i = 0; i < QSPI_TEST_SIZE; i++) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; s_buf[i] = x; }
  sfud_err e = sfud_erase_write(f, QSPI_TEST_ADDR, QSPI_TEST_SIZE, s_buf);
  bool ok = e == SFUD_SUCCESS;
  size_t badAt = 0;
  if (ok) {
    static uint8_t rd[256];
    for (size_t off = 0; ok && off < QSPI_TEST_SIZE; off += sizeof rd) {
      if (sfud_read(f, QSPI_TEST_ADDR + off, sizeof rd, rd) != SFUD_SUCCESS) { ok = false; badAt = off; break; }
      for (size_t i = 0; i < sizeof rd; i++) if (rd[i] != s_buf[off + i]) { ok = false; badAt = off + i; break; }
    }
  }
#if QSPI_RESTORE_SECTOR
  sfud_erase_write(f, QSPI_TEST_ADDR, QSPI_TEST_SIZE, s_backup);
#endif
  if (!ok) reportSet(T_QSPI, false, e != SFUD_SUCCESS ? "erase/write err %d" : "verify mismatch @+%u", e != SFUD_SUCCESS ? (int)e : (int)badAt);
  else reportSet(T_QSPI, true, "%s %luKB, 4KB @0x%06lX ok", f->chip.name ? f->chip.name : "flash",
                 (unsigned long)(f->chip.capacity / 1024), (unsigned long)QSPI_TEST_ADDR);
}

// ---------------- 3. 加速度计 ----------------
static LIS3DHTR<TwoWire> s_acc;
void testAccel() {
  s_acc.begin(Wire1);
  delay(50);
  uint8_t id = s_acc.getDeviceID();
  if (id != ACC_WHO_AM_I) { reportSet(T_ACC, false, "WHO_AM_I 0x%02X != 0x%02X", id, ACC_WHO_AM_I); return; }
  s_acc.setOutputDataRate(LIS3DHTR_DATARATE_100HZ);
  s_acc.setFullScaleRange(LIS3DHTR_RANGE_2G);
  delay(100);
  float sum = 0;
  for (int i = 0; i < 10; i++) {
    float x, y, z; s_acc.getAcceleration(&x, &y, &z);
    sum += sqrtf(x * x + y * y + z * z); delay(12);
  }
  float g = sum / 10;
  reportSet(T_ACC, g >= ACC_G_MIN && g <= ACC_G_MAX, "id 0x33 |a|=%.2fg (%.2f~%.2f)", g, ACC_G_MIN, ACC_G_MAX);
}

// ---------------- 4. 蜂鸣器 + 麦克风 ----------------
static int micP2P(uint32_t ms) {
  int lo = 4095, hi = 0;
  uint32_t t = millis();
  while (millis() - t < ms) { int v = analogRead(WIO_MIC); lo = min(lo, v); hi = max(hi, v); }
  return hi - lo;
}
void testAudio() {
  pinMode(WIO_MIC, INPUT);
  pinMode(WIO_BUZZER, OUTPUT);
  micP2P(20);
  int noise = micP2P(AUDIO_NOISE_MS);
  tone(WIO_BUZZER, AUDIO_TONE_HZ);
  delay(20);
  int sig = micP2P(AUDIO_SAMPLE_MS);
  noTone(WIO_BUZZER);
  digitalWrite(WIO_BUZZER, LOW);
  float ratio = (float)sig / max(noise, 1);
  bool ok = sig >= AUDIO_MIN_P2P && ratio >= AUDIO_RATIO_MIN;
  reportSet(T_AUDIO, ok, "noise p2p %d, tone p2p %d, x%.1f%s", noise, sig, ratio, ok ? "" : " (buzzer or mic bad)");
}

// ---------------- 5/6. RTL8720DN + Wi-Fi ----------------
static bool macValid(const uint8_t *m) {
  bool all0 = true, allF = true;
  for (int i = 0; i < 6; i++) { all0 &= m[i] == 0; allF &= m[i] == 0xFF; }
  return !all0 && !allF;
}
void testRtlAndWifi() {
  // 注意：若 RTL8720 完全无响应，eRPC 调用可能阻塞；进入前已打印日志，串口停在此处即判定 RTL8720 故障
  Serial.println(F("[ .. ] RTL8720    eRPC handshake (hang here = RTL8720 dead)"));
  const char *ver = rpc_system_version();
  if (ver && *ver) strlcpy(g_rtlVer, ver, sizeof g_rtlVer);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(g_mac, sizeof g_mac, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  bool rtlOk = ver && *ver && macValid(mac);
  reportSet(T_RTL, rtlOk, "ver %s mac %s", g_rtlVer, g_mac);
  if (!rtlOk && !(ver && *ver)) { reportSet(T_WIFI, false, "skipped: RTL8720 no response"); return; }

  const bool targeted = strlen(WIFI_SSID_2G) || strlen(WIFI_SSID_5G);
  g_wifiFallback = !targeted;
  int best2 = -127, best5 = -127, n = 0;
  for (int attempt = 0; attempt < WIFI_SCAN_RETRIES; attempt++) {
    n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      int rssi = WiFi.RSSI(i), ch = WiFi.channel(i);
      String ssid = WiFi.SSID(i);
      bool is5 = ch > 14;
      Serial.printf("       scan: %-24.24s ch%3d %4d dBm\r\n", ssid.c_str(), ch, rssi);
      if (targeted) {
        if (!is5 && strlen(WIFI_SSID_2G) && ssid == WIFI_SSID_2G) best2 = max(best2, rssi);
        if (is5 && strlen(WIFI_SSID_5G) && ssid == WIFI_SSID_5G) best5 = max(best5, rssi);
      } else {
        if (is5) best5 = max(best5, rssi); else best2 = max(best2, rssi);
      }
    }
    WiFi.scanDelete();
    bool need2 = !targeted || strlen(WIFI_SSID_2G), need5 = !targeted || strlen(WIFI_SSID_5G);
    if ((!need2 || best2 >= WIFI_RSSI_MIN_DBM) && (!need5 || best5 >= WIFI_RSSI_MIN_DBM)) break;
  }
  bool need2 = !targeted || strlen(WIFI_SSID_2G), need5 = !targeted || strlen(WIFI_SSID_5G);
  bool ok2 = !need2 || best2 >= WIFI_RSSI_MIN_DBM, ok5 = !need5 || best5 >= WIFI_RSSI_MIN_DBM;
  Serial.printf("       scan result: 2G %d 5G %d (min %d)%s\r\n", best2, best5, WIFI_RSSI_MIN_DBM,
                targeted ? "" : " [fallback]");
  char scan[32];
  snprintf(scan, sizeof scan, "%s2G:%d 5G:%d", targeted ? "" : "fb ", best2, best5);
  bool scanOk = ok2 && ok5;

#if WIFI_CONNECT_ENABLE
  if (strlen(WIFI_CONNECT_SSID)) {
    Serial.printf("       connecting to %s ...\r\n", WIFI_CONNECT_SSID);
    WiFi.begin(WIFI_CONNECT_SSID, WIFI_CONNECT_PASS);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_CONNECT_TIMEOUT_MS) { ledTick(); delay(50); }
    int st = WiFi.status();
    bool conn = st == WL_CONNECTED;
    IPAddress ip = conn ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
    bool ipOk = conn && ip != IPAddress(0, 0, 0, 0);
    while (conn && !ipOk && millis() - t < WIFI_CONNECT_TIMEOUT_MS) {
      ledTick(); delay(100); ip = WiFi.localIP(); ipOk = ip != IPAddress(0, 0, 0, 0);
    }
    Serial.printf("       wifi status %d ip %s ", st, ip.toString().c_str());
    Serial.printf("gw %s rssi %d (%lu ms)\r\n", WiFi.gatewayIP().toString().c_str(),
                  conn ? (int)WiFi.RSSI() : 0, (unsigned long)(millis() - t));
    WiFi.disconnect(true);
    if (!conn) reportSet(T_WIFI, false, "%s conn %s", scan, millis() - t >= WIFI_CONNECT_TIMEOUT_MS ? "timeout" : "fail");
    else if (!ipOk) reportSet(T_WIFI, false, "%s no IP", scan);
    else reportSet(T_WIFI, scanOk, "%s IP:%s%s", scan, ip.toString().c_str(), scanOk ? "" : " RSSI<min");
    return;
  }
#endif
  reportSet(T_WIFI, scanOk, "%s (min %d)%s", scan, WIFI_RSSI_MIN_DBM, n <= 0 ? " no AP" : "");
}

// ---------------- 7. LCD + 背光 ----------------
void testLcd() {
  const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
  for (uint16_t c : colors) { tft.fillScreen(c); uint32_t t = millis(); while (millis() - t < LCD_COLOR_MS) ledTick(); }
  tft.fillScreen(TFT_WHITE);
  // 软件 PWM 渐变：1kHz 载波，占空比 0→100→0%，约 1.6s
  for (int step = 0; step <= 80; step++) {
    int duty = step <= 40 ? step * 25 : (80 - step) * 25;   // 0..1000 us / 1000 us 周期
    for (int cyc = 0; cyc < 20; cyc++) {
      if (duty > 0) { backlight(1); delayMicroseconds(duty); }
      if (duty < 1000) { backlight(0); delayMicroseconds(1000 - duty); }
    }
    ledTick();
  }
  backlight(255);
  waitRelease(1000);
  bool relA = digitalRead(WIO_KEY_A) == HIGH, relC = digitalRead(WIO_KEY_C) == HIGH;
  uiProgress("LCD / Backlight", "A = display OK    C = display BAD   (serial: p / f)");
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Colors & dimming OK?", 10, 140);
  uint32_t t0 = millis();
  while (millis() - t0 < LCD_CONFIRM_TIMEOUT_MS) {
    ledTick();
    if (digitalRead(WIO_KEY_A) == HIGH) relA = true;
    if (digitalRead(WIO_KEY_C) == HIGH) relC = true;
    char c = serialCmd();
    if ((relA && digitalRead(WIO_KEY_A) == LOW) || c == 'p') { reportSet(T_LCD, true, "operator confirmed"); waitRelease(1000); return; }
    if ((relC && digitalRead(WIO_KEY_C) == LOW) || c == 'f' || c == 's') { reportSet(T_LCD, false, "operator marked BAD"); waitRelease(1000); return; }
  }
  reportSet(T_LCD, false, "timeout: no confirmation");
}

// ---------------- 8. 按键 + 摇杆 ----------------
static const int16_t BOX[8][2] = {   // x,y (左上角)，上排 C B A 对应外壳从左到右
  {224, 40}, {132, 40}, {40, 40},
  {132, 110}, {132, 190}, {72, 150}, {192, 150}, {132, 150},
};
static void drawBox(int i, uint16_t c) {
  tft.fillRoundRect(BOX[i][0], BOX[i][1], 56, 34, 5, c);
  tft.setTextColor(TFT_BLACK, c); tft.setTextSize(1);
  tft.drawCentreString(KEYS[i].name, BOX[i][0] + 28, BOX[i][1] + 13, 1);
}
void testKeys() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setTextSize(1);
  tft.drawString("Press every key once (serial: p=ok f=fail)", 10, 8);
  // 粘连的键不能拿来算通过
  uint8_t done = 0;
  bool stuck[8] = {false};
  for (int i = 0; i < 8; i++) { stuck[i] = pressed(i); drawBox(i, stuck[i] ? TFT_ORANGE : TFT_DARKGREY); }
  bool released[8]; for (int i = 0; i < 8; i++) released[i] = !stuck[i];
  uint32_t t0 = millis();
  while (done != 0xFF && millis() - t0 < KEYS_TIMEOUT_MS) {
    ledTick();
    for (int i = 0; i < 8; i++) {
      bool p = pressed(i);
      if (!p) released[i] = true;
      if (p && released[i] && !(done & (1 << i))) { done |= 1 << i; drawBox(i, TFT_GREEN); Serial.printf("       key %s ok\r\n", KEYS[i].name); }
    }
    char c = serialCmd();
    if (c == 'p') { reportSet(T_KEYS, true, "serial confirmed"); return; }
    if (c == 'f' || c == 's') break;
    char cd[16]; snprintf(cd, sizeof cd, "%2lus ", (unsigned long)((KEYS_TIMEOUT_MS - (millis() - t0)) / 1000));
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.drawString(cd, 290, 8);
    delay(5);
  }
  if (done == 0xFF) { reportSet(T_KEYS, true, "all 8 keys triggered"); return; }
  char buf[96] = "no resp:";
  for (int i = 0; i < 8; i++) if (!(done & (1 << i))) { drawBox(i, TFT_RED); strlcat(buf, " ", sizeof buf); strlcat(buf, KEYS[i].name, sizeof buf); }
  reportSet(T_KEYS, false, "%s", buf);
  delay(800);
}

// ---------------- 9. 光敏 ----------------
static int lightAvg() { long s = 0; for (int i = 0; i < 16; i++) s += analogRead(WIO_LIGHT); return s / 16; }
void testLight() {
  pinMode(WIO_LIGHT, INPUT);
  int base = lightAvg();
  // 允许设备平放桌面导致初始遮挡：先记录峰值(需 >= LIGHT_MIN_BASELINE)，再检测相对峰值跌落
  uiProgress("Light sensor", "Lift/light back window, then cover it  (serial: f=fail)");
  int maxV = base, minAfterPeak = base;
  uint32_t t0 = millis();
  while (millis() - t0 < LIGHT_TIMEOUT_MS) {
    ledTick();
    int v = lightAvg();
    if (v > maxV) { maxV = v; minAfterPeak = v; }
    if (v < minAfterPeak) minAfterPeak = v;
    char s[44]; snprintf(s, sizeof s, "now %4d  peak %4d  ", v, maxV);
    tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.drawString(s, 10, 140);
    int target = (int)(maxV * (1.0f - LIGHT_DROP_RATIO));
    if (maxV >= LIGHT_MIN_BASELINE && minAfterPeak <= target) {
      reportSet(T_LIGHT, true, "peak %d -> %d (drop>=%d%%)", maxV, minAfterPeak, (int)(LIGHT_DROP_RATIO * 100));
      return;
    }
    char c = serialCmd();
    if (c == 'p') { reportSet(T_LIGHT, true, "serial confirmed"); return; }
    if (c == 'f' || c == 's') break;
    delay(20);
  }
  reportSet(T_LIGHT, false, "timeout: peak %d low %d (need>=%d)", maxV, minAfterPeak, LIGHT_MIN_BASELINE);
}
