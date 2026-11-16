#include <Arduino.h>
#include <Wire.h>
#include <sfud.h>
#include <LIS3DHTR.h>
#include <rpcWiFi.h>
#include "erpc/rpc_system.h"
#include "test_config.h"
#include "report.h"
#include "tests.h"
#include "input.h"
#include "ui.h"

// ---------------- 公共 ----------------
static inline bool pressed(int i) { return digitalRead(KEY_DEFS[i].pin) == LOW; }

static LIS3DHTR<TwoWire> s_acc;
static bool s_imuReady = false;
static uint8_t s_imuId = 0;
static int s_micBias = 512;

void hardwareBegin() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(WIO_MIC, INPUT);
  pinMode(WIO_LIGHT, INPUT);
  pinMode(WIO_BUZZER, OUTPUT);
  digitalWrite(WIO_BUZZER, LOW);
  inputBegin();
  // 统一 10bit ADC：AUDIO_MIN_P2P、WIO_LIGHT、WIO_MIC 波形全部按 0..1023
  analogReadResolution(10);
  micCalibrate();
}

void ledHeartbeat() {
  static uint32_t last = 0;
  static bool level = false;
  uint32_t period = reportAllPass() ? 500u : 100u;  // 1Hz PASS / 5Hz REPAIR
  if (millis() - last >= period) {
    last = millis();
    level = !level;
    digitalWrite(LED_BUILTIN, level ? HIGH : LOW);
  }
}

void ledTick() {
  if (Serial.available() && tolower(Serial.peek()) == 'v') { Serial.read(); printVersion(); }
  static uint32_t t = 0;
  if (millis() - t >= 150) { t = millis(); digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }
}

bool keysReleased(uint32_t timeoutMs) {
  uint32_t t = millis();
  while (millis() - t < timeoutMs) {
    bool any = false;
    for (int i = 0; i < K_COUNT; i++) any |= pressed(i);
    if (!any) return true;
    ledTick();
    delay(5);
  }
  return false;
}

// ---------------- 1. 按键粘连 ----------------
void testStuckKeys() {
  for (auto &k : KEY_DEFS) pinMode(k.pin, INPUT_PULLUP);
  delay(20);
  uint8_t stuckMask = 0xFF;
  uint32_t t = millis();
  while (millis() - t < STUCK_CHECK_MS) {    // 持续按下才算粘连
    for (int i = 0; i < K_COUNT; i++) if (!pressed(i)) stuckMask &= ~(1 << i);
    delay(5);
  }
  if (!stuckMask) { reportSet(T_STUCK, true, "no key held at boot"); return; }
  char buf[96] = "stuck:";
  for (int i = 0; i < K_COUNT; i++) {
    if (stuckMask & (1 << i)) { strlcat(buf, " ", sizeof buf); strlcat(buf, KEY_DEFS[i].name, sizeof buf); }
  }
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
  g_qspiCapacity = f->chip.capacity;
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
bool imuBegin(bool force) {
  if (s_imuReady && !force) return true;
  s_imuReady = false;
  s_acc.begin(Wire1);
  delay(50);
  s_imuId = s_acc.getDeviceID();
  if (s_imuId != ACC_WHO_AM_I) return false;
  s_acc.setOutputDataRate(LIS3DHTR_DATARATE_100HZ);
  s_acc.setFullScaleRange(LIS3DHTR_RANGE_2G);
  delay(10);
  s_imuReady = true;
  return true;
}

bool imuOnline() { return s_imuReady; }
uint8_t imuDeviceId() { return s_imuId; }

bool imuRead(float &x, float &y, float &z) {
  if (!s_imuReady) return false;
  s_acc.getAcceleration(&x, &y, &z);
  return true;
}

void testAccel() {
  if (!imuBegin(true)) {
    reportSet(T_ACC, false, "WHO_AM_I 0x%02X != 0x%02X", s_imuId, ACC_WHO_AM_I);
    return;
  }
  float sum = 0;
  for (int i = 0; i < 10; i++) {
    float x = 0, y = 0, z = 0;
    imuRead(x, y, z);
    sum += sqrtf(x * x + y * y + z * z);
    delay(12);
  }
  float g = sum / 10;
  reportSet(T_ACC, g >= ACC_G_MIN && g <= ACC_G_MAX, "id 0x33 |a|=%.2fg (%.2f~%.2f)", g, ACC_G_MIN, ACC_G_MAX);
}

// ---------------- 4. 蜂鸣器 + 麦克风 ----------------
void micCalibrate() {
  uint32_t acc = 0;
  const int n = 256;
  for (int i = 0; i < n; i++) {
    acc += analogRead(WIO_MIC);
    delayMicroseconds(120);
  }
  s_micBias = acc / n;
}

int micBias() { return s_micBias; }
int readMicRaw() { return analogRead(WIO_MIC); }
int readLightRaw() { return analogRead(WIO_LIGHT); }

void toneStart(int hz) { tone(WIO_BUZZER, hz); }
void toneStop() { noTone(WIO_BUZZER); digitalWrite(WIO_BUZZER, LOW); }
void playTone(int hz, int ms) {
  if (hz <= 0 || ms <= 0) return;
  toneStart(hz);
  delay(ms);
  toneStop();
}

static int micP2P(uint32_t ms) {
  int lo = 1023, hi = 0;
  uint32_t t = millis();
  while (millis() - t < ms) { int v = analogRead(WIO_MIC); lo = min(lo, v); hi = max(hi, v); }
  return hi - lo;
}

void testAudio() {
  pinMode(WIO_MIC, INPUT);
  pinMode(WIO_BUZZER, OUTPUT);
  micP2P(20);
  int noise = micP2P(AUDIO_NOISE_MS);
  toneStart(AUDIO_TONE_HZ);
  delay(20);
  int sig = micP2P(AUDIO_SAMPLE_MS);
  toneStop();
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

bool testRtl() {
  // 注意：若 RTL8720 完全无响应，eRPC 调用可能阻塞；进入前已打印日志，串口停在此处即判定 RTL8720 故障
  Serial.println(F("[ .. ] RTL8720    eRPC handshake (hang here = RTL8720 dead)"));
  const char *ver = rpc_system_version();
  snprintf(g_rtlVer, sizeof g_rtlVer, "N/A");
  if (ver && *ver) strlcpy(g_rtlVer, ver, sizeof g_rtlVer);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(g_mac, sizeof g_mac, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  bool rtlOk = ver && *ver && macValid(mac);
  reportSet(T_RTL, rtlOk, "ver %s mac %s", g_rtlVer, g_mac);
  return rtlOk;
}

void testWifi(bool rtlOk) {
  g_rssi2 = g_rssi5 = -127;
  g_wifiFallback = false;
  snprintf(g_ssid, sizeof g_ssid, "-");
  snprintf(g_ip, sizeof g_ip, "-");
  snprintf(g_gw, sizeof g_gw, "-");

  if (!rtlOk) {
    snprintf(g_wifiStatus, sizeof g_wifiStatus, "skipped: RTL8720 offline");
    reportSet(T_WIFI, false, "skipped: RTL8720 no response");
    return;
  }

  const bool targeted = strlen(WIFI_SSID_2G) || strlen(WIFI_SSID_5G);
  g_wifiFallback = !targeted;
  snprintf(g_wifiStatus, sizeof g_wifiStatus, "scanning 2.4G/5G");
  int best2 = -127, best5 = -127, n = 0;
  char bestName2[33] = "", bestName5[33] = "";
  for (int attempt = 0; attempt < WIFI_SCAN_RETRIES; attempt++) {
    n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      int rssi = WiFi.RSSI(i), ch = WiFi.channel(i);
      String ssid = WiFi.SSID(i);
      bool is5 = ch > 14;
      Serial.printf("       scan: %-24.24s ch%3d %4d dBm\r\n", ssid.c_str(), ch, rssi);
      if (targeted) {
        if (!is5 && strlen(WIFI_SSID_2G) && ssid == WIFI_SSID_2G && rssi > best2) {
          best2 = rssi; strlcpy(bestName2, ssid.c_str(), sizeof bestName2);
        }
        if (is5 && strlen(WIFI_SSID_5G) && ssid == WIFI_SSID_5G && rssi > best5) {
          best5 = rssi; strlcpy(bestName5, ssid.c_str(), sizeof bestName5);
        }
      } else {
        if (is5 && rssi > best5) { best5 = rssi; strlcpy(bestName5, ssid.c_str(), sizeof bestName5); }
        if (!is5 && rssi > best2) { best2 = rssi; strlcpy(bestName2, ssid.c_str(), sizeof bestName2); }
      }
    }
    WiFi.scanDelete();
    bool need2 = !targeted || strlen(WIFI_SSID_2G), need5 = !targeted || strlen(WIFI_SSID_5G);
    if ((!need2 || best2 >= WIFI_RSSI_MIN_DBM) && (!need5 || best5 >= WIFI_RSSI_MIN_DBM)) break;
  }
  g_rssi2 = best2;
  g_rssi5 = best5;
  bool need2 = !targeted || strlen(WIFI_SSID_2G), need5 = !targeted || strlen(WIFI_SSID_5G);
  bool ok2 = !need2 || best2 >= WIFI_RSSI_MIN_DBM, ok5 = !need5 || best5 >= WIFI_RSSI_MIN_DBM;
  Serial.printf("       scan result: 2G %d 5G %d (min %d)%s\r\n", best2, best5, WIFI_RSSI_MIN_DBM,
                targeted ? "" : " [fallback]");
  char scan[40];
  snprintf(scan, sizeof scan, "%s2G:%d 5G:%d", targeted ? "" : "fb ", best2, best5);
  bool scanOk = ok2 && ok5;

#if WIFI_CONNECT_ENABLE
  if (strlen(WIFI_CONNECT_SSID)) {
    snprintf(g_wifiStatus, sizeof g_wifiStatus, "connecting %s", WIFI_CONNECT_SSID);
    Serial.print(F("       connecting to "));
    Serial.print(WIFI_CONNECT_SSID);
    Serial.println(F(" ..."));
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
    IPAddress gw = conn ? WiFi.gatewayIP() : IPAddress(0, 0, 0, 0);
    if (conn) {
      snprintf(g_ip, sizeof g_ip, "%s", ip.toString().c_str());
      snprintf(g_gw, sizeof g_gw, "%s", gw.toString().c_str());
      strlcpy(g_ssid, WIFI_CONNECT_SSID, sizeof g_ssid);
    } else if (bestName2[0]) {
      strlcpy(g_ssid, bestName2, sizeof g_ssid);
    }
    Serial.print(F("       wifi status "));
    Serial.print(st);
    Serial.print(F(" ip "));
    Serial.print(ip.toString().c_str());
    Serial.print(F(" gw "));
    Serial.print(gw.toString().c_str());
    Serial.print(F(" rssi "));
    Serial.print(conn ? (int)WiFi.RSSI() : 0);
    Serial.print(F(" ("));
    Serial.print((unsigned long)(millis() - t));
    Serial.println(F(" ms)"));
    WiFi.disconnect(true);
    if (!conn) {
      snprintf(g_wifiStatus, sizeof g_wifiStatus, "connect %s",
               millis() - t >= WIFI_CONNECT_TIMEOUT_MS ? "timeout" : "fail");
      reportSet(T_WIFI, false, "%s conn %s", scan,
                millis() - t >= WIFI_CONNECT_TIMEOUT_MS ? "timeout" : "fail");
    } else if (!ipOk) {
      snprintf(g_wifiStatus, sizeof g_wifiStatus, "connected, no DHCP IP");
      reportSet(T_WIFI, false, "%s no IP", scan);
    } else {
      snprintf(g_wifiStatus, sizeof g_wifiStatus, "connected, DHCP ok");
      reportSet(T_WIFI, scanOk, "%s IP:%s%s", scan, g_ip, scanOk ? "" : " RSSI<min");
    }
    return;
  }
#endif
  snprintf(g_wifiStatus, sizeof g_wifiStatus, "scan only");
  if (bestName2[0]) strlcpy(g_ssid, bestName2, sizeof g_ssid);
  else if (bestName5[0]) strlcpy(g_ssid, bestName5, sizeof g_ssid);
  reportSet(T_WIFI, scanOk, "%s (min %d)%s", scan, WIFI_RSSI_MIN_DBM, n <= 0 ? " no AP" : "");
}

void testWireless() {
  uint32_t ts = millis();
  bool rtlOk = testRtl();
  g_results[T_RTL].ms = millis() - ts;
  ts = millis();
  testWifi(rtlOk);
  g_results[T_WIFI].ms = millis() - ts;
}

// ---------------- 自动检查总流程 ----------------
void runAllAutoTests(bool showProgress) {
  if (showProgress) uiBootProgress(0, AUTO_CHECK_ITEMS, "STUCK_KEY", "sampling 8 keys before boot");
  // 重测时 A 键可能仍被按住，先等操作员松手，避免误判粘连
  keysReleased(KEYS_RELEASE_WAIT_MS);
  uint32_t t0 = millis(), ts;

  ts = millis(); testStuckKeys(); g_results[T_STUCK].ms = millis() - ts;

  if (showProgress) uiBootProgress(1, AUTO_CHECK_ITEMS, "QSPI_FLASH", "erase / write / verify 4KB sector");
  ts = millis(); testQspi(); g_results[T_QSPI].ms = millis() - ts;

  if (showProgress) uiBootProgress(2, AUTO_CHECK_ITEMS, "ACCEL", "LIS3DHTR @ Wire1  +/-2g static check");
  ts = millis(); testAccel(); g_results[T_ACC].ms = millis() - ts;

  if (showProgress) uiBootProgress(3, AUTO_CHECK_ITEMS, "BUZZER_MIC", "4kHz tone -> WIO_MIC closed loop");
  ts = millis(); testAudio(); g_results[T_AUDIO].ms = millis() - ts;

  if (showProgress) uiBootProgress(4, AUTO_CHECK_ITEMS, "RTL8720", "eRPC handshake + MAC address");
  ts = millis(); bool rtlOk = testRtl(); g_results[T_RTL].ms = millis() - ts;

  if (showProgress) uiBootProgress(5, AUTO_CHECK_ITEMS, "WIFI_2G5G", "dual-band scan + DHCP connect");
  ts = millis(); testWifi(rtlOk); g_results[T_WIFI].ms = millis() - ts;

  g_totalMs = millis() - t0;
  printVersion();
  reportPrintFinal(g_totalMs);
}
