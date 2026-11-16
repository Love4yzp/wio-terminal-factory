#pragma once
#include <Arduino.h>

enum TestId { T_STUCK, T_QSPI, T_ACC, T_AUDIO, T_RTL, T_WIFI, T_LCD, T_KEYS, T_LIGHT, T_COUNT };
enum Status { ST_PENDING, ST_PASS, ST_FAIL };

struct TestResult { const char *name; Status status; char detail[96]; uint32_t ms; };

extern TestResult g_results[T_COUNT];
extern char g_mac[24];
extern char g_rtlVer[48];
extern bool g_wifiFallback;

void reportSet(TestId id, bool pass, const char *fmt, ...);
bool reportAllPass();
void reportPrintFinal(uint32_t totalMs);
