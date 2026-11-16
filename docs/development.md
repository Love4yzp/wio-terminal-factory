# Phase 1 固件开发说明

## 构建
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
- **Wi-Fi**：在 `include/test_config.h` 修改 `WIFI_SSID_2G` / `WIFI_SSID_5G` / `WIFI_RSSI_MIN_DBM`。两个 SSID 都留空时降级为“任意 2.4G 与 5G AP（channel>14 视为 5G）都 ≥ 门限”，报告中带 `[fallback]` 和 `wifi_fallback:true`。只配一个 SSID 时只校验该频段。
- **QSPI**：测试 4MB 外部 Flash 最后一个 4KB 扇区，测前备份、测后写回（`QSPI_RESTORE_SECTOR`）。程序在 SAMD51 内部 Flash，不受影响。
- **声学**：`AUDIO_MIN_P2P` / `AUDIO_RATIO_MIN` 需要用好机实测后再定标。
- **光敏**：`LIGHT_DROP_RATIO`，环境太暗（基线 < `LIGHT_MIN_BASELINE`）直接 FAIL。

## 串口兜底
人工项（LCD、按键、光敏）运行时，串口发 `p`=通过、`f`=失败、`s`=跳过（记 FAIL）。每个人工项都有超时，超时记 FAIL 并继续下一项。测试结束后发 `r` 可重新打印报告。

## 已知限制
- RTL8720 完全无响应时 eRPC 调用可能阻塞，固件无法自行超时。进入前串口会打印 `eRPC handshake (hang here = RTL8720 dead)`；如果停在这一行，就判定为 RTL8720 故障。
- 背光渐变用 `analogWrite(LCD_BACKLIGHT)`；如果该引脚在当前 core 上没有 PWM，就只能看到亮/灭，需要实机确认。
- 阈值为初值，都没有经过实机验证。
