# Phase 1 固件开发说明

## D-009 启动流程（零人工阻塞）

固件插上 USB 供电即可独立运行，不依赖电脑或串口：

1. `setup()` 初始化 LCD / 背光 / ADC（统一 `analogReadResolution(10)`）与按键上拉，仅等待 USB 枚举最多 `BOOT_SERIAL_WAIT_MS`（800ms），没有无限阻塞。
2. 在 Auto Check 启动页依次执行 6 项自动检查：`STUCK_KEY` → `QSPI_FLASH` → `ACCEL`（LIS3DHTR @ `Wire1`）→ `BUZZER_MIC`（4kHz 闭环）→ `RTL8720`（eRPC + MAC）→ `WIFI_2G5G`（2.4G/5G 扫描 + `WiFi.begin` + DHCP）。
3. 6 项结束后立即向串口打印 `FW_VER:<版本>|<build_id>`、ASCII 汇总表和单行 `JSON:`，然后直接进入 **Tab 1（Auto Check）**。
4. `PASS` / `REPAIR` 只由上述 6 项决定；`LCD_BL`、`KEYS_5WAY`、`LIGHT` 不再作为启动阻塞项或总判定条件，它们只在 Tab 5 / Tab 6 中做人工目检与交互。

## 测试工位快速使用（无需 pio）

只需 Python 3 标准库，支持 Linux / macOS：

```bash
python3 tools/auto_flash.py                       # 默认 flash-only：常驻，插一台刷一台
python3 tools/auto_flash.py --once                # 只处理一台，退出码 0=成功/已最新 2=失败
python3 tools/auto_flash.py --retest              # 固件已最新时发 t 重启重测
python3 tools/auto_flash.py --monitor             # 调试：透传串口并等待 JSON 结果
python3 tools/auto_flash.py --monitor --save-log  # 调试并保存 logs/<时间>_<MAC>.json/.log
python3 tools/auto_flash.py --self-test
```

默认工位流程（D-007 + D-009）：

1. 识别串口（USB VID `0x2886`，跳过 UF2 Bootloader 的 PID `0x002d`）或 `Arduino` UF2 盘（`INFO_UF2.TXT` 含 SAMD51 / Wio Terminal / Seeed）。
2. 串口发送 `v` 读取 `FW_VER:<版本>|<build_id>`，与 `dist/manifest.json` 比对。一致且未加 `--retest` 时只报告“已是最新”，**不进入 monitor、不等待测试结果**。
3. 不一致或无响应时用 1200 bps touch 进入 UF2 模式；6 秒内没有出现盘符会提示双击侧边电源开关，再继续等待 `--uf2-timeout`（默认 60s）。
4. 写入 `dist/wio-factory-test.uf2`，等待 UF2 盘卸载（10s）并短暂确认应用串口重新枚举（8s），随后立即结束并打印 `烧录完成，设备已自动运行检测与多 Tab 看板`。
5. 默认 **不采集日志**；只有 `--monitor` 才透传串口，`--save-log` 才创建 `logs/`。`--save-log` 必须与 `--monitor` 一起使用。

Linux 需要当前用户有串口权限（`uucp` / `dialout` 组），并且 UF2 盘能自动挂载（桌面环境或 udisks）。

## 6-Tab 交互看板

`loop()` 通过 5 向摇杆 `LEFT` / `RIGHT` 循环切页，页面基类在 `include/page.h`，各页实现在 `src/page_*.cpp`。Header 28px + accent 线，Footer 20px + accent 线，Header 显示标题、`N / 6` 与 6 个页面指示点。

| Tab | 页面 | 主要行为 |
| :---: | :--- | :--- |
| 1 | Auto Check | 大号 PASS/REPAIR、总耗时、build_id、6 行明细；`A` 重跑 6 项（先等待按键释放，避免误判粘连）并刷新串口 |
| 2 | Wi-Fi / Net | RTL8720 版本、MAC、2.4G/5G RSSI 条、SSID、IP、网关、状态；`A` 重跑 RTL + 扫描 + DHCP，并重打报告 |
| 3 | IMU | 气泡水平仪 + X/Y/Z ±2g 条 + `\|a\|`；`A` / `NavPress` 重置平滑并强制重初始化 |
| 4 | Audio | WIO_MIC 实时滚动波形 + VU/peak hold；`A` 冻结/恢复，`B` 4kHz 短鸣（不阻塞采样，可看到麦克风尖峰） |
| 5 | Light & IO | 实时光照渐变条 + ADC；`A/B/C` 播放 C4/E4/G4；8 键实时变绿并锁存 seen；`PRESS` 切换 Key-Lock（锁定后 L/R 留在本页并计入 8 键），离开/重进清零 |
| 6 | LCD & Sys | R/G/B/W/K 色块 + 16 级灰阶条；FW、Build ID、QSPI 4MB、Free RAM、Uptime；`A` 全屏纯色轮播 + 1kHz 软件 PWM 背光渐变后重绘 |

Tab 5 默认是普通浏览模式，`LEFT` / `RIGHT` 立即切页；进入 Tab 5 的那个方向会记为已 seen。按 `PRESS` 进入 **Key-Lock** 后 `capturesNav()` 返回 true，`LEFT` / `RIGHT` 不再切页，而是点亮/锁存 `L` / `R`，可完成 8/8 键检；底部提示切换为 `KEY LOCKED: test L/R/all 8   PRESS unlock`，右侧 badge 变为橙色 `[KEY LOCK]`。再按 `PRESS` 解锁，锁定/解锁都不清空已 seen；只有离开或重新进入 Tab 5 才清零。首次达到 8/8 时播放 523/659/784 Hz 短提示音，并把 `SEEN 8/8 PASS` 标绿。

### 后台行为

* `LED_BUILTIN`：`PASS` 时 1 Hz 慢闪，`REPAIR` 时 5 Hz 快闪（`ledHeartbeat()`）。
* 串口命令：`v` 重打 `FW_VER:...`，`r` 重打汇总表 + JSON，`t` 通过 `NVIC_SystemReset()` 重启。
* 默认不阻塞等待任何人工确认；6 项自动检查完成后随时可按 `A` 重测。

## 发布产物 `dist/`（纳入 git）

每次 `pio run` 由 `scripts/uf2.py` 自动更新 `dist/wio-factory-test.uf2` 和 `dist/manifest.json`（字段包括 fw_version、build_id、payload_sha256、block_count、wifi_ssid_2g/5g 等，**不包含 Wi-Fi 密码**）。
`FW_BUILD_ID` 由 `scripts/build_id.py` 对 `src/`、`include/`、`platformio.ini` 求 SHA256 后取前 8 位，所以改 SSID、阈值或看板逻辑都会换指纹。**改完配置后请重新 `pio run`，并把 `dist/` 一起提交。**

## 开发者编译（需要 pio）

```bash
pio run            # 产物: .pio/build/wio_terminal/firmware.bin / firmware.uf2
pio run -t upload  # USB 直接烧录（bossac）
pio device monitor # 115200 串口日志
```

`scripts/uf2.py` 是 post-build 脚本，把 firmware.bin 转成 UF2（SAMD51，基址 0x4000，family 0x55114460），不依赖外部 uf2conv.py。

## 拖盘烧录

拨动电源开关快速双击（向下拨两次）进入 Bootloader，电脑上出现 `Arduino` 盘符后，把 `firmware.uf2` 拖进去即可，设备会自动重启运行自动检查并进入 Tab 1。

## 代码结构

| 文件 | 职责 |
| --- | --- |
| `include/test_config.h` | 阈值、Wi-Fi SSID/RSSI、启动/看板时序、Phase 2 开关 |
| `src/main.cpp` | `setup()` 自动检查、6 页切换、串口命令、后台心跳 |
| `src/tests.cpp` | 6 项自动检查 + IMU/麦克风/蜂鸣器/光照等共享硬件助手 |
| `src/report.cpp` | 6 项结果记录、串口表格 + 单行 `JSON:` |
| `src/ui.cpp` | 深色主题 Header/Footer、启动进度页、busy 页、背光数字开关 |
| `src/input.cpp` | 8 键去抖轮询与 `InputEvent` |
| `src/page_auto.cpp` ... `src/page_system.cpp` | 6 个交互 Tab 的绘制与按键行为 |

## 测试配置

* **Wi-Fi**：在 `include/test_config.h` 修改 `WIFI_SSID_2G` / `WIFI_SSID_5G` / `WIFI_RSSI_MIN_DBM`。两个 SSID 都留空时降级为“任意 2.4G 与 5G AP（channel > 14 视为 5G）都 ≥ 门限”，报告中带 `[fallback]` 和 `wifi_fallback:true`。只配一个 SSID 时只校验该频段。默认值为 `SEEED-Guest`。
* **Wi-Fi 实连**：`WIFI_CONNECT_ENABLE`（默认 1）、`WIFI_CONNECT_SSID` / `WIFI_CONNECT_PASS`（默认 `SEEED-Guest` / `SeeedGuest22`）、`WIFI_CONNECT_TIMEOUT_MS`（12000）。扫描完成后执行 `WiFi.begin()`，在超时内等待 `WL_CONNECTED` 并拿到非 0 IP，然后 `WiFi.disconnect(true)`。`WIFI_2G5G` 只有扫描达标**并且**拿到 IP 时才判 PASS。
* **QSPI**：测试 4MB 外部 Flash 最后一个 4KB 扇区，测前备份、测后写回（`QSPI_RESTORE_SECTOR`）。程序在 SAMD51 内部 Flash，不受影响。保留 `sfud_qspi_fast_read_enable(..., 2)`。
* **声学**：`AUDIO_MIN_P2P` / `AUDIO_RATIO_MIN` 按 10-bit ADC 设定，需用好机实测后再定标。
* **背光**：`LCD_BACKLIGHT`（`PC05`）是 `NOT_ON_PWM`，只能 `pinMode(OUTPUT)` + `digitalWrite(HIGH/LOW)`。Tab 6 的 1kHz 软件 PWM 渐变也只用数字开关。**不要** 用 `analogWrite`（SAMD51 默认 12-bit，`analogWrite(...,255)` 会变 LOW 关背光）。
* **ADC**：`hardwareBegin()` 统一 `analogReadResolution(10)`，与 `AUDIO_MIN_P2P`、光照条、麦克风波形一致。

## 串口输出

检查完成后依次打印：

```text
FW_VER:wio-factory-p1-0.1.0|<build_id>
+------------+------+--------+------------------------------------------------+
| ITEM       | RES  | ms     | DETAIL                                         |
... 6 行 ...
JSON:{"fw":"...","build_id":"...","result":"PASS|REPAIR",...,"items":[...]}
```

所有 `Serial.printf` 都保持短输出或拆成多个 `Serial.print`，避免 SAMD `Print::printf` 的 80 字节缓冲截断 `JSON:` 或表格行。

## 已知限制

* RTL8720 完全无响应时 eRPC 调用可能阻塞，固件无法自行超时。进入前串口会打印 `eRPC handshake (hang here = RTL8720 dead)`；如果停在这一行，就判定为 RTL8720 故障。
* 阈值为初值，尚未全部经过实机复核；D-009 的真实硬件行为需在 GPD 上复测。
* 默认 flash-only 模式不会保存测试结果；需要留档时必须显式使用 `--monitor --save-log`。
