#pragma once
void testStuckKeys();
void testQspi();
void testAccel();
void testAudio();
void testRtlAndWifi();
void testLcd();
void testKeys();
void testLight();
void ledTick();          // 测试期间蓝灯闪烁
char serialCmd();        // 串口兜底：'p' pass / 'f' fail / 's' 跳过(记 FAIL)，无输入返回 0
