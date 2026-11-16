#pragma once
#include <Arduino.h>

// Boot / auto-check helpers
void hardwareBegin();
void runAllAutoTests(bool showProgress);
bool keysReleased(uint32_t timeoutMs);
void ledHeartbeat();              // 1Hz PASS / 5Hz REPAIR
void ledTick();                   // 测试等待期间的心跳 + 'v' 串口兜底

// Six automated checks
void testStuckKeys();
void testQspi();
void testAccel();
void testAudio();
bool testRtl();
void testWifi(bool rtlOk);
void testWireless();              // Tab 2 re-run: RTL + scan + DHCP

// Shared hardware helpers for the interactive pages
bool imuBegin(bool force = false);
bool imuOnline();
bool imuRead(float &x, float &y, float &z);
uint8_t imuDeviceId();
void micCalibrate();
int micBias();
int readMicRaw();
int readLightRaw();
void toneStart(int hz);
void toneStop();
void playTone(int hz, int ms);
