# Phase 1 固件开发说明

## 测试工位快速使用（无需 pio）
只需 Python 3（标准库即可，无需 pip），Linux / macOS：
```bash
python3 tools/auto_flash.py            # 常驻：插一台测一台，拔下后插下一台
python3 tools/auto_flash.py --once     # 只测一台，退出码 0=拿到结果 2=失败
python3 tools/auto_flash.py --retest   # 已是最新固件且停在汇总页时，自动发 't' 重测
python3 tools/auto_flash.py --self-test
```
流程（D-007）：
1. 识别串口（USB VID `0x2886`）或 `Arduino` UF2 盘（`INFO_UF2.TXT` 含 SAMD51/Wio Terminal/Seeed）。
2. 串口发送 `v` 取 `FW_VER:<版本>|<build_id>`，与 `dist/manifest.json` 比对：一致就跳过烧录；不一致或无响应时用 1200bps touch 进入 UF2 模式。6s 内没出现盘符会提示操作员双击侧边开关。
3. 写入 `dist/wio-factory-test.uf2`，设备重启后自动开始测试，脚本把串口日志透传到终端。
4. 收到 `JSON:` 行后，终端醒目显示 `[PASS]` / `[REPAIR]` 和失败项，结果存到 `logs/<时间>_<MAC>.json/.log`。

Linux 需要当前用户有串口权限（`uucp` / `dialout` 组），并且 UF2 盘能自动挂载（桌面环境或 udisks）。

## 发布产物 `dist/`（纳入 git）
每次 `pio run` 由 `scripts/uf2.py` 自动更新 `dist/wio-factory-test.uf2` 和 `dist/manifest.json`（字段包括 fw_version、build_id、payload_sha256、block_count、wifi_ssid_2g/5g 等）。
`FW_BUILD_ID` 由 `scripts/build_id.py` 对 `src/`、`include/`、`platformio.ini` 求 SHA256 后取前 8 位，所以改 SSID 或阈值都会换指纹。**改完配置后请重新 `pio run`，并把 `dist/` 一起提交。**

## 开发者编译（需要 pio）

```bash
pio run            # 产物: .pio/build/wio_terminal/firmware.bin / firmware.uf2
pio run -t upload  # USB 直接烧录（bossac）
pio device monitor # 115200 串口日志
```
`scripts/uf2.py` 是 post-build 脚本，把 firmware.bin 转成 UF2（SAMD51，基址 0x4000，family 0x55114460），不依赖外部 uf2conv.py。

## 拖盘烧录
拨动电源开关快速双击（向下拨两次）进入 Bootloader，电脑上出现 `Arduino` 盘符后，把 `firmware.uf2` 拖进去即可，设备会自动重启运行测试。

## 代码结构
| 文件 | 职责 |
|---|---|
| `include/test_config.h` | 全部阈值、超时、Wi-Fi SSID/RSSI、Phase 2 开关（默认 0） |
| `src/main.cpp` | 测试顺序（状态机）、计时、结束后 LED 慢闪/快闪 |
| `src/tests.cpp` | 各测试项实现 |
| `src/ui.cpp` | LCD 进度页 / 汇总页、背光控制 |
| `src/report.cpp` | 结果记录、串口表格 + 单行 `JSON:` 汇总 |

## 测试配置
- **Wi-Fi**：在 `include/test_config.h` 修改 `WIFI_SSID_2G` / `WIFI_SSID_5G` / `WIFI_RSSI_MIN_DBM`。两个 SSID 都留空时降级为“任意 2.4G 与 5G AP（channel>14 视为 5G）都 ≥ 门限”，报告中带 `[fallback]` 和 `wifi_fallback:true`。只配一个 SSID 时只校验该频段。默认值为 `SEEED-Guest`（2.4G/5G 同名）。
- **Wi-Fi 实连**：`WIFI_CONNECT_ENABLE`（默认 1）、`WIFI_CONNECT_SSID` / `WIFI_CONNECT_PASS`（默认 `SEEED-Guest` / `SeeedGuest22`）、`WIFI_CONNECT_TIMEOUT_MS`（12000）。扫描完成后执行 `WiFi.begin()`，在超时内等待 `WL_CONNECTED` 并拿到非 0 IP，然后 `WiFi.disconnect(true)`。`WIFI_2G5G` 只有在扫描达标**并且**拿到 IP 时才判 PASS。屏幕显示紧凑格式，例如 `2G:-52 5G:-58 IP:10.1.2.3` / `... conn timeout` / `... no IP` / `... RSSI<min`。完整的 status、IP、网关、RSSI 见串口。`WIFI_CONNECT_SSID` 留空或 `ENABLE=0` 时只做扫描。密码只放在源码里，不写入 `dist/manifest.json`。
- **QSPI**：测试 4MB 外部 Flash 最后一个 4KB 扇区，测前备份、测后写回（`QSPI_RESTORE_SECTOR`）。程序在 SAMD51 内部 Flash，不受影响。
- **声学**：`AUDIO_MIN_P2P` / `AUDIO_RATIO_MIN` 需要用好机实测后再定标。
- **光敏**：`LIGHT_DROP_RATIO`，环境太暗（基线 < `LIGHT_MIN_BASELINE`）直接 FAIL。

## 串口兜底
人工项（LCD、按键、光敏）运行时，串口发 `p`=通过、`f`=失败、`s`=跳过（记 FAIL）。每个人工项都有超时，超时记 FAIL 并继续下一项。任何阶段发 `v` 都会输出版本行（RTL8720 eRPC 阻塞期间除外）。测试结束后发 `r` 重新打印报告，发 `t` 重启重测。

## 已知限制
- RTL8720 完全无响应时 eRPC 调用可能阻塞，固件无法自行超时。进入前串口会打印 `eRPC handshake (hang here = RTL8720 dead)`；如果停在这一行，就判定为 RTL8720 故障。
- 背光渐变用 `analogWrite(LCD_BACKLIGHT)`；如果该引脚在当前 core 上没有 PWM，就只能看到亮/灭，需要实机确认。
- 阈值为初值，都没有经过实机验证。
