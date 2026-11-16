#pragma once
#include <Arduino.h>

// D-009: the overall PASS / REPAIR verdict is decided ONLY by these six
// automated checks.  LCD / keys / light are interactive dashboard pages and
// never block the boot flow.
enum TestId { T_STUCK, T_QSPI, T_ACC, T_AUDIO, T_RTL, T_WIFI, T_COUNT };
enum Status { ST_PENDING, ST_PASS, ST_FAIL };

struct TestResult { const char *name; Status status; char detail[96]; uint32_t ms; };

extern TestResult g_results[T_COUNT];
extern char g_mac[24];
extern char g_rtlVer[48];
extern bool g_wifiFallback;
extern int g_rssi2;
extern int g_rssi5;
extern char g_ssid[33];
extern char g_ip[20];
extern char g_gw[20];
extern char g_wifiStatus[48];
extern uint32_t g_qspiCapacity;
extern uint32_t g_totalMs;

void reportSet(TestId id, bool pass, const char *fmt, ...);
bool reportAllPass();
void printVersion();   // 单行: FW_VER:<FW_VERSION>|<FW_BUILD_ID>
void reportPrintFinal(uint32_t totalMs);
