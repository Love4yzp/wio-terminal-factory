// Wio Terminal 售后检修 / 整机测试 —— 全部可调参数集中于此
#pragma once

// ---------- Wi-Fi（修改目标 AP 在这里） ----------
// 留空 "" 则降级：只要扫描到任意 2.4G 与 5G 信号 >= 门限的 AP 即判 PASS（报告会注明 fallback）
#define WIFI_SSID_2G        "SEEED-Guest"
#define WIFI_SSID_5G        "SEEED-Guest"
#define WIFI_RSSI_MIN_DBM   (-70)
#define WIFI_SCAN_RETRIES   2
// 扫描后实际连网（拿到有效 IP 才算 PASS）；密码不会写入 dist/manifest.json
#define WIFI_CONNECT_ENABLE     1
#define WIFI_CONNECT_SSID       "SEEED-Guest"
#define WIFI_CONNECT_PASS       "SeeedGuest22"
#define WIFI_CONNECT_TIMEOUT_MS 12000

// ---------- QSPI Flash ----------
// 外部 4MB QSPI 不存放程序（程序在 SAMD51 内部 Flash），测最后一个 4KB 扇区，测试前备份、测试后恢复
#define QSPI_TEST_ADDR      (4UL * 1024 * 1024 - 4096)
#define QSPI_TEST_SIZE      4096
#define QSPI_RESTORE_SECTOR 1

// ---------- 加速度计 ----------
#define ACC_WHO_AM_I        0x33
#define ACC_G_MIN           0.85f
#define ACC_G_MAX           1.15f

// ---------- 声学闭环 ----------
#define AUDIO_TONE_HZ       4000
#define AUDIO_SAMPLE_MS     300
#define AUDIO_NOISE_MS      200
#define AUDIO_MIN_P2P       120     // 发声期峰峰值最小 ADC 计数（10bit）
#define AUDIO_RATIO_MIN     2.0f    // 发声期峰峰值 / 底噪峰峰值 最小倍数

// ---------- 光敏 ----------
#define LIGHT_DROP_RATIO    0.40f   // 相对基线下降 >= 40% 即通过
#define LIGHT_MIN_BASELINE  30      // 基线过低（环境太暗/开路）直接 FAIL
#define LIGHT_TIMEOUT_MS    10000

// ---------- 人工交互超时 ----------
#define LCD_COLOR_MS        700
#define LCD_CONFIRM_TIMEOUT_MS 10000  // 超时未确认记 FAIL(timeout)
#define KEYS_TIMEOUT_MS     15000
#define STUCK_CHECK_MS      300

// ---------- Phase 2 / 待定项（默认关闭，目前无实现） ----------
#define ENABLE_SD_TEST      0
#define ENABLE_BLE_TEST     0
#define ENABLE_GPIO_LOOPBACK 0
#define ENABLE_IR_TEST      0

#ifndef FW_BUILD_ID
#define FW_BUILD_ID "dev"   // 正常由 scripts/build_id.py 注入
#endif
#define FW_VERSION          "wio-factory-p1-0.1.0"
