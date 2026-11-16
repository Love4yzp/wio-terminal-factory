# Wio Terminal 整机功能测试与检修项目 (wio-terminal-factory)

本项目用于对 **Seeed Studio Wio Terminal** 进行整机硬件功能检验，主要服务于 **用户使用后退回/返修设备的故障定位与检修筛查（RMA Triage）**，同时支持整机功能验收。

按照 **D-009**，测试固件现在是 **零人工阻塞** 的：插上 USB 供电后自动跑完 6 项硬件检查，立即在串口打印版本 / 汇总表 / 单行 JSON，并在屏幕上进入 **6-Tab 交互看板**。整体结论 `PASS` / `REPAIR` 只由 6 项自动检查决定，屏幕、按键、光敏等人工目检项移到独立 Tab，不再阻塞启动流程。

## 快速使用

### 测试工位（无需 pio / 无需 Python 第三方库）

```bash
python3 tools/auto_flash.py            # 默认 flash-only：插一台刷一台，刷完立即结束
python3 tools/auto_flash.py --once     # 只处理一台，0=成功/已最新，2=失败
python3 tools/auto_flash.py --retest   # 固件已最新时发送 t 重启重测
python3 tools/auto_flash.py --monitor  # 调试：刷完透传串口并等待 JSON 结果
python3 tools/auto_flash.py --monitor --save-log   # 调试并保存 logs/*.json/.log
python3 tools/auto_flash.py --self-test
```

默认流程（D-007 + D-009）：

1. 识别 Wio Terminal：USB 串口 VID `0x2886`（忽略 UF2 bootloader PID `0x002d`）或 `Arduino` UF2 盘。
2. 串口能读到 `FW_VER:<版本>|<build_id>` 且与 `dist/manifest.json` 一致时，直接报告“已是最新”，不进入 monitor、不等待测试。
3. 版本不一致或无响应时，用 1200 bps touch 踢入 UF2；6 秒内未出现盘符会提示双击侧边电源开关。
4. 写入 `dist/wio-factory-test.uf2`，等待 UF2 盘卸载并短暂确认应用串口重新枚举，然后立即结束：`烧录完成，设备已自动运行检测与多 Tab 看板`。
5. 默认 **不** 保存 `logs/`，也 **不** 等待测试结果；只有显式加 `--monitor` 才透传串口，`--save-log` 才留档。

设备端插电即自动运行，无需电脑：6 项检查完成后直接停在 Tab 1（Auto Check）。

### 开发者编译（需要 PlatformIO）

```bash
pio run            # 同步更新 dist/wio-factory-test.uf2 与 dist/manifest.json
pio run -t upload  # USB 直接烧录
pio device monitor # 115200 串口
```

详见 [固件开发与工位操作说明](docs/development.md)。

## 6-Tab 交互看板

通过 5 向摇杆 `LEFT` / `RIGHT` 循环切换页面；每页底部有当前按键提示。

| Tab | 页面 | 内容 | 按键 |
| :---: | :--- | :--- | :--- |
| 1 | **Auto Check** | 大号 `PASS`/`REPAIR`、总耗时、build_id、6 项自动检查明细 | `A` 重跑 6 项并刷新串口 |
| 2 | **Wi-Fi / Net** | RTL8720 固件、MAC、2.4G/5G 最佳 RSSI + 信号条、SSID、IP、网关、状态 | `A` 重扫 + DHCP |
| 3 | **IMU** | LIS3DHTR 气泡水平仪、X/Y/Z ±2g 条、`\|a\|` 读数 | `A` / `NavPress` 重置平滑并重初始化 |
| 4 | **Audio** | WIO_MIC 实时滚动波形（校准 micBias）+ VU 峰值保持 | `A` 冻结/恢复；`B` 4kHz 短鸣 |
| 5 | **Light & IO** | 实时光照渐变条 + ADC 读数；A/B/C 播放 C4/E4/G4；8 键实时点亮与 seen 锁存 | `NavPress` 清除 seen |
| 6 | **LCD & Sys** | R/G/B/W/K 色块 + 灰阶条；FW 版本、Build ID、QSPI 4MB、Free RAM、Uptime | `A` 全屏纯色轮播 + 1kHz 软件 PWM 背光渐变 |

状态说明：`PASS` 时蓝色 LED 1 Hz 慢闪，`REPAIR` 时 5 Hz 快闪。串口命令：`v` 版本、`r` 重打报告、`t` 重启。

## 文档索引

* [设计决策记录 (ADR)](docs/decisions.md) — 已确定范围、取舍与 D-009 交互契约
* [第一阶段最小可用测试方案 (Phase 1 MVP)](docs/phase1-mvp-plan.md) — 零治具硬件覆盖与测试项目
* [固件开发与工位操作说明](docs/development.md) — 工位流程、6-Tab 行为、参数配置、串口与打包

## 阶段规划概览

| 阶段 | 目标 | 依赖条件 | 状态 |
| :--- | :--- | :--- | :--- |
| **Phase 1 (MVP)** | 6 项零人工自动检查 + 6-Tab 现场看板，屏幕直读 PASS / 检修明细 | 仅需 USB-C 供电 + 2.4G/5G 测试 Wi-Fi AP（默认 `SEEED-Guest`） | **已完成（待实机复核 D-009）** |
| **Phase 2 (扩展)** | 补齐外部接口、红外、SD 卡槽与批量上位机留档 | 40-Pin/Grove 回环治具 PCB、红外接收头、测试 SD 卡 | **待定** |
