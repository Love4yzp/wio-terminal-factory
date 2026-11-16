#!/usr/bin/env python3
"""Wio Terminal 测试工位：自动识别 → 版本比对 → 按需 UF2 烧录。

D-009 默认只刷机（flash-only）：不等待测试跑完、不采集 logs/。设备烧录完成后
自动运行 6 项自动检测并进入 6-Tab 交互看板。

  python3 tools/auto_flash.py                    # 常驻：插一台刷一台
  python3 tools/auto_flash.py --once             # 只处理一台
  python3 tools/auto_flash.py --retest           # 固件已最新时发 't' 重启重测
  python3 tools/auto_flash.py --monitor          # 调试：透传串口并等待 JSON
  python3 tools/auto_flash.py --monitor --save-log  # 调试并保存 logs/*.json/.log
  python3 tools/auto_flash.py --self-test        # 离线自检
"""
import argparse, glob, hashlib, json, os, re, select, struct, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")
LOGS = os.path.join(ROOT, "logs")
SEEED_VID = 0x2886
UF2_PID = 0x002D
UF2_MAGIC0, UF2_MAGIC1, UF2_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
IS_MAC = sys.platform == "darwin"

def c(txt, code):
    return "\033[%sm%s\033[0m" % (code, txt) if sys.stdout.isatty() else txt

def log(msg):
    print("[%s] %s" % (time.strftime("%H:%M:%S"), msg), flush=True)

# ---------------------------------------------------------------- manifest / UF2
def load_manifest(dist=DIST):
    with open(os.path.join(dist, "manifest.json")) as f:
        m = json.load(f)
    for k in ("build_id", "fw_version", "payload_sha256", "block_count", "uf2_file"):
        if k not in m:
            raise ValueError("manifest.json missing %s" % k)
    return m

def uf2_payload_hash(data, base, nblocks):
    """把 UF2 中 [base, base + nblocks*256) 范围内的块按地址拼接后求 SHA256。

    CURRENT.UF2 是整片 Flash 的转储（可能从 0 开始、含 bootloader），所以按地址过滤。
    返回 None 表示数据不完整。"""
    end = base + nblocks * 256
    chunks = {}
    for off in range(0, len(data) - 511, 512):
        b = data[off:off + 512]
        m0, m1, flags, addr, size = struct.unpack_from("<IIIII", b, 0)
        if m0 != UF2_MAGIC0 or m1 != UF2_MAGIC1 or struct.unpack_from("<I", b, 508)[0] != UF2_END:
            continue
        if flags & 0x1:  # not-main-flash
            continue
        if size == 256 and base <= addr < end:
            chunks[addr] = b[32:32 + 256]
        if len(chunks) == nblocks:
            break
    if len(chunks) != nblocks:
        return None
    h = hashlib.sha256()
    for a in range(base, end, 256):
        if a not in chunks:
            return None
        h.update(chunks[a])
    return h.hexdigest()

def parse_fw_ver(text):
    m = re.findall(r"FW_VER:([^|\r\n]+)\|([0-9A-Za-z_-]+)", text)
    return m[-1] if m else None

def is_up_to_date(ver, build_id):
    return bool(ver) and len(ver) == 2 and ver[1] == build_id

def done_message():
    return "烧录完成，设备已自动运行检测与多 Tab 看板"

# ---------------------------------------------------------------- UF2 盘
def mount_candidates():
    user = os.environ.get("USER") or os.environ.get("LOGNAME") or ""
    if IS_MAC:
        return sorted(glob.glob("/Volumes/*"))
    cands = set(glob.glob("/media/%s/*" % user) + glob.glob("/run/media/%s/*" % user) +
                glob.glob("/media/*") + glob.glob("/mnt/*"))
    try:
        with open("/proc/mounts") as f:
            for line in f:
                parts = line.split()
                if len(parts) > 2 and parts[2] in ("vfat", "msdos", "fat"):
                    cands.add(parts[1].replace("\\040", " "))
    except OSError:
        pass
    return sorted(cands)

def is_wio_uf2(info_text):
    t = info_text.lower()
    return "uf2" in t and any(k in t for k in ("samd51", "wio terminal", "seeed"))

def find_uf2_drive():
    for d in mount_candidates():
        info = os.path.join(d, "INFO_UF2.TXT")
        try:
            with open(info, errors="replace") as f:
                if is_wio_uf2(f.read()):
                    return d
        except OSError:
            continue
    return None

def check_drive_current(drive, man):
    try:
        need = (man.get("payload_base", 0x4000) + man["block_count"] * 256) // 256 + 64
        with open(os.path.join(drive, "CURRENT.UF2"), "rb") as f:
            data = f.read(need * 512)
    except OSError:
        return "unknown"
    h = uf2_payload_hash(data, man.get("payload_base", 0x4000), man["block_count"])
    if h is None:
        return "unknown"
    return "match" if h == man["payload_sha256"] else "old"

def flash_drive(drive, man):
    st = check_drive_current(drive, man)
    labels = {"match": "已匹配(仍写入以触发重启)", "old": "旧版/其他固件", "unknown": "未知"}
    log("UF2 盘 %s，盘内固件：%s" % (drive, labels[st]))
    src = os.path.join(DIST, man["uf2_file"])
    dst = os.path.join(drive, "wio-factory-test.uf2")
    with open(src, "rb") as f:
        data = f.read()
    written = 0
    try:
        fd = os.open(dst, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
        try:
            for i in range(0, len(data), 64 * 1024):
                written += os.write(fd, data[i:i + 64 * 1024])
            os.fsync(fd)
        finally:
            os.close(fd)
    except OSError as e:
        # 写完最后一块设备会立刻重启卸盘，fsync/close 出错属正常
        log("写入结束时盘符消失（%s），通常表示设备已重启" % e.strerror)
    if written == 0:
        log(c("没有写入任何数据，盘符可能已提前消失", "31"))
        return False
    log(c("已写入 %s（build %s），等待盘符卸载" % (man["uf2_file"], man["build_id"]), "36"))
    return True

# ---------------------------------------------------------------- 串口发现
def linux_ports(sys_root="/sys/class/tty"):
    out = []
    for tty in sorted(glob.glob(os.path.join(sys_root, "ttyACM*"))):
        p = os.path.realpath(os.path.join(tty, "device"))
        for _ in range(4):  # 向上找 idVendor（接口目录 → USB 设备目录）
            p = os.path.dirname(p)
            vf = os.path.join(p, "idVendor")
            if os.path.exists(vf):
                try:
                    vid = int(open(vf).read().strip(), 16)
                    pf = os.path.join(p, "idProduct")
                    pid = int(open(pf).read().strip(), 16) if os.path.exists(pf) else 0
                    # 跳过 UF2 Bootloader 自带的 CDC 口 (0x002D)，避免抢先连上即将重启的口
                    if vid == SEEED_VID and pid != UF2_PID:
                        out.append("/dev/" + os.path.basename(tty))
                except ValueError:
                    pass
                break
    return out

def mac_seeed_present(ioreg_text):
    return bool(re.search(r'"idVendor"\s*=\s*%d\b' % SEEED_VID, ioreg_text))

def mac_ports():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        return []
    try:
        txt = subprocess.run(["ioreg", "-p", "IOUSB", "-l", "-w", "0"], capture_output=True, text=True, timeout=5).stdout
    except Exception:
        return ports  # ioreg 不可用时兜底
    # ioreg 中 Seeed 设备存在才认；多台 usbmodem 时全部返回，打开后靠 FW_VER 再确认
    return ports if mac_seeed_present(txt) else []

def find_serial():
    ports = mac_ports() if IS_MAC else linux_ports()
    return ports[0] if ports else None

# ---------------------------------------------------------------- 纯 termios 串口
class Serial:
    def __init__(self, path, baud=115200):
        import termios
        self.t = termios
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attr = termios.tcgetattr(self.fd)
        attr[0] = 0                                    # iflag
        attr[1] = 0                                    # oflag
        attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL | termios.HUPCL
        attr[3] = 0                                    # lflag (raw)
        sp = getattr(termios, "B%d" % baud)
        attr[4] = attr[5] = sp
        attr[6][termios.VMIN] = 0
        attr[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attr)
        self._dtr(True)

    def _dtr(self, on):
        try:
            import fcntl
            TIOCMBIS = getattr(self.t, "TIOCMBIS", 0x8004746C if IS_MAC else 0x5416)
            TIOCMBIC = getattr(self.t, "TIOCMBIC", 0x8004746B if IS_MAC else 0x5417)
            fcntl.ioctl(self.fd, TIOCMBIS if on else TIOCMBIC, struct.pack("I", self.t.TIOCM_DTR))
        except Exception:
            pass

    def read(self, timeout=0.1):
        r, _, _ = select.select([self.fd], [], [], timeout)
        if not r:
            return b""
        try:
            d = os.read(self.fd, 4096)
        except BlockingIOError:
            return b""
        except OSError:
            raise EOFError("serial gone")
        if not d:
            raise EOFError("serial gone")
        return d

    def write(self, b):
        os.write(self.fd, b)

    def close(self):
        try:
            os.close(self.fd)
        except OSError:
            pass

def touch_1200(path):
    """1200 bps 打开并拉低 DTR 后关闭 → Seeed bootloader 进入 UF2 模式。"""
    try:
        s = Serial(path, 1200)
        s._dtr(False)
        time.sleep(0.1)
        s.close()
    except Exception as e:
        try:  # stty 兜底
            subprocess.run(["stty", "-f" if IS_MAC else "-F", path, "1200"], timeout=3)
        except Exception:
            log("1200bps touch 失败：%s" % e)

# ---------------------------------------------------------------- 主流程
def query_version(s, listen=1.2, tries=3):
    buf = ""
    end = time.time() + listen
    while time.time() < end:
        buf += s.read(0.1).decode(errors="replace")
    for _ in range(tries):
        if parse_fw_ver(buf):
            break
        try:
            s.write(b"v")
        except OSError:
            break
        end = time.time() + 1.0
        while time.time() < end:
            buf += s.read(0.1).decode(errors="replace")
            if parse_fw_ver(buf):
                break
    return parse_fw_ver(buf), buf

def save_result(json_line, raw):
    os.makedirs(LOGS, exist_ok=True)
    try:
        rep = json.loads(json_line)
    except ValueError:
        rep = {}
    mac = (rep.get("mac") or "unknown").replace(":", "")
    stem = os.path.join(LOGS, "%s_%s" % (time.strftime("%Y%m%d-%H%M%S"), mac))
    with open(stem + ".json", "w") as f:
        f.write(json_line + "\n")
    with open(stem + ".log", "w") as f:
        f.write(raw)
    return rep, stem

def monitor(s, initial, timeout, save_log=False):
    """透传串口直到拿到 JSON: 行。save_log 时才写 logs/。"""
    raw = initial
    sys.stdout.write(initial)
    sys.stdout.flush()
    json_re = re.compile(r"^JSON:(\{.*\})\r?\n", re.M)
    end = time.time() + timeout
    while time.time() < end:
        try:
            data = s.read(0.2).decode(errors="replace")
        except EOFError:
            log(c("串口断开（设备被拔出或重启）", "33"))
            return False
        if not data:
            continue
        sys.stdout.write(data)
        sys.stdout.flush()
        raw += data
        m = json_re.search(raw)
        if m:
            rep = {}
            stem = None
            if save_log:
                rep, stem = save_result(m.group(1), raw)
            else:
                try:
                    rep = json.loads(m.group(1))
                except ValueError:
                    rep = {}
            ok = rep.get("result") == "PASS"
            print()
            print(c("=" * 60, "1"))
            print(c("  [PASS]  全部通过" if ok else "  [REPAIR]  需检修", "1;42;30" if ok else "1;41;37"))
            for it in rep.get("items", []):
                if not it.get("pass"):
                    print(c("   FAIL %-10s %s" % (it.get("name"), it.get("detail")), "31"))
            print("  MAC %s  RTL %s  build %s" % (rep.get("mac"), rep.get("rtl_fw"), rep.get("build_id")))
            if stem:
                print("  已保存 %s.json / .log" % stem)
            else:
                print("  未保存日志（需要留档请加 --monitor --save-log）")
            print(c("=" * 60, "1"), flush=True)
            return True
    log(c("等待结果超时", "33"))
    return False

def wait_for(fn, timeout, interval=0.3):
    end = time.time() + timeout
    while time.time() < end:
        r = fn()
        if r:
            return r
        time.sleep(interval)
    return None

def _try_open(port):
    try:
        return Serial(port)
    except OSError:
        return None

def open_monitor(port, args):
    s = wait_for(lambda: _try_open(port), 5) if port else None
    if not s:
        log(c("无法打开串口进行监控", "31"))
        return False
    try:
        initial = ""
        end = time.time() + 0.5
        while time.time() < end:
            initial += s.read(0.1).decode(errors="replace")
        return monitor(s, initial, args.test_timeout, args.save_log)
    except EOFError:
        log(c("监控期间串口断开", "33"))
        return False
    finally:
        s.close()

def wait_drive_gone(drive, timeout):
    end = time.time() + timeout
    while time.time() < end:
        still = os.path.exists(os.path.join(drive, "INFO_UF2.TXT"))
        if not still and find_uf2_drive() is None:
            return True
        time.sleep(0.2)
    return False

def confirm_running(timeout=8):
    port = wait_for(find_serial, timeout)
    if port:
        log(c("设备已重启，串口 %s 已重新枚举" % port, "32"))
    else:
        log(c("未在 %ds 内看到应用串口；请目视屏幕确认已进入 Auto Check" % timeout, "33"))
    return port

def flash_and_finish(drive, man):
    if not flash_drive(drive, man):
        return False
    if not wait_drive_gone(drive, 10):
        log(c("UF2 盘未在 10s 内卸载，烧录可能未完成", "31"))
        return False
    log("UF2 盘已卸载（设备已离开 Bootloader）")
    confirm_running(8)
    log(c(done_message(), "1;42;30"))
    return True

def handle_uf2(drive, man, args):
    ok = flash_and_finish(drive, man)
    if not ok:
        return False
    if args.monitor:
        port = wait_for(find_serial, 10)
        return open_monitor(port, args)
    return True

def handle_serial(port, man, args):
    log("发现串口 %s，查询固件版本..." % port)
    try:
        s = Serial(port)
    except OSError as e:
        log(c("打开串口失败：%s" % e, "31"))
        return False
    try:
        ver, buf = query_version(s)
    except EOFError:
        log(c("查询版本时串口断开", "33"))
        s.close()
        return False

    if is_up_to_date(ver, man["build_id"]):
        log(c("固件已是最新（%s|%s），跳过烧录" % ver, "32"))
        if args.retest or args.monitor:
            log("发送 't' 重启，设备将重新执行自动检测")
            try:
                s.write(b"t")
            except OSError:
                pass
            s.close()
            time.sleep(1.2)
            if args.monitor:
                new_port = wait_for(find_serial, 10)
                return open_monitor(new_port, args)
            log(c("已触发重启；默认模式不等待测试结果", "36"))
            return True
        s.close()
        log(c("设备已是最新固件，保持自动检测/看板运行", "32"))
        return True

    log(c("固件不一致（%s）→ 1200bps 踢入 UF2" % ("%s|%s" % ver if ver else "无响应/非测试固件"), "33"))
    s.close()
    touch_1200(port)
    drive = wait_for(find_uf2_drive, 6)
    if not drive:
        log(c(">>> 未自动进入 UF2：请快速双击（向下拨两次）侧边电源开关 <<<", "1;33"))
        drive = wait_for(find_uf2_drive, args.uf2_timeout)
    if not drive:
        log(c("未检测到 Arduino 盘符，放弃本台", "31"))
        return False
    return handle_uf2(drive, man, args)

def run_one(man, args):
    log("等待 Wio Terminal 接入（串口 VID 0x2886 或 Arduino UF2 盘）...")
    while True:
        drive = find_uf2_drive()
        if drive:
            return handle_uf2(drive, man, args)
        port = find_serial()
        if port:
            return handle_serial(port, man, args)
        time.sleep(0.5)

def wait_unplug():
    log("请拔下设备，插入下一台...")
    while find_serial() or find_uf2_drive():
        time.sleep(0.5)

# ---------------------------------------------------------------- 参数 / 自检
def build_parser():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--once", action="store_true", help="只处理一台后退出")
    ap.add_argument("--retest", action="store_true", help="固件已最新时发 't' 重启重测")
    ap.add_argument("--monitor", action="store_true", help="调试：刷完后透传串口并等待 JSON 结果")
    ap.add_argument("--save-log", action="store_true", help="调试留档：与 --monitor 一起用时保存 logs/")
    ap.add_argument("--test-timeout", type=float, default=180, help="--monitor 等待 JSON 结果的秒数")
    ap.add_argument("--uf2-timeout", type=float, default=60, help="提示双击后继续等盘符的秒数")
    ap.add_argument("--self-test", action="store_true", help="离线自检")
    return ap

def args_error(args):
    if args.save_log and not args.monitor:
        return "--save-log 需要与 --monitor 一起使用"
    return None

def self_test():
    import tempfile
    ok = True
    def check(name, cond):
        nonlocal ok
        ok &= bool(cond)
        print("%s %s" % ("PASS" if cond else "FAIL", name))

    def mk_uf2(payload, base, flags=0x2000, fam=0x55114460):
        n = (len(payload) + 255) // 256
        out = b""
        for i in range(n):
            ch = payload[i * 256:(i + 1) * 256].ljust(256, b"\0")
            out += struct.pack("<IIIIIIII", UF2_MAGIC0, UF2_MAGIC1, flags, base + i * 256, 256, i, n, fam) + ch + b"\0" * 220 + struct.pack("<I", UF2_END)
        return out

    app = os.urandom(256 * 5 + 17)
    padded = app.ljust(256 * 6, b"\0")
    want = hashlib.sha256(padded).hexdigest()
    check("uf2 payload hash (app-only)", uf2_payload_hash(mk_uf2(app, 0x4000), 0x4000, 6) == want)
    full = mk_uf2(b"\xAA" * 0x4000, 0) + mk_uf2(padded + b"\xFF" * 1024, 0x4000)
    check("uf2 payload hash (CURRENT.UF2 full dump from 0)", uf2_payload_hash(full, 0x4000, 6) == want)
    check("uf2 payload hash mismatch detected", uf2_payload_hash(mk_uf2(os.urandom(256 * 6), 0x4000), 0x4000, 6) != want)
    check("uf2 truncated -> None", uf2_payload_hash(mk_uf2(app, 0x4000)[:512 * 3], 0x4000, 6) is None)

    if os.path.exists(os.path.join(DIST, "manifest.json")):
        m = load_manifest()
        check("dist manifest parse", re.fullmatch(r"[0-9a-f]{8}", m["build_id"]))
        data = open(os.path.join(DIST, m["uf2_file"]), "rb").read()
        check("dist uf2 matches manifest payload_sha256",
              uf2_payload_hash(data, m.get("payload_base", 0x4000), m["block_count"]) == m["payload_sha256"])
        check("dist uf2 matches manifest uf2_sha256", hashlib.sha256(data).hexdigest() == m.get("uf2_sha256"))
    else:
        check("dist/manifest.json exists", False)

    check("parse FW_VER", parse_fw_ver("junk\r\nFW_VER:wio-factory-p1-0.1.0|2273449b\r\n") == ("wio-factory-p1-0.1.0", "2273449b"))
    check("parse FW_VER none", parse_fw_ver("hello") is None)
    check("up-to-date compare", is_up_to_date(("wio-factory-p1-0.1.0", "2273449b"), "2273449b"))
    check("up-to-date rejects mismatch", not is_up_to_date(("wio-factory-p1-0.1.0", "deadbeef"), "2273449b"))
    check("done message", "烧录完成" in done_message() and "多 Tab" in done_message())
    check("INFO_UF2 wio", is_wio_uf2("UF2 Bootloader v3.7.0 SFHR\nModel: Wio Terminal\nBoard-ID: SAMD51J19A-Wio-v0\n"))
    check("INFO_UF2 other board rejected", not is_wio_uf2("UF2 Bootloader\nModel: Raspberry Pi RP2\nBoard-ID: RPI-RP2\n"))
    check("ioreg VID match", mac_seeed_present('| "idVendor" = 10374\n'))
    check("ioreg VID no match", not mac_seeed_present('| "idVendor" = 1027\n| "idVendor" = 103740\n'))

    ap = build_parser()
    a = ap.parse_args([])
    check("default is flash-only", not a.monitor and not a.save_log and not a.retest and not a.once)
    a = ap.parse_args(["--monitor", "--save-log"])
    check("monitor + save-log args", a.monitor and a.save_log and args_error(a) is None)
    a = ap.parse_args(["--save-log"])
    check("save-log requires monitor", args_error(a) is not None)

    with tempfile.TemporaryDirectory() as td:  # 伪造 /sys/class/tty 结构
        def fake(tty, vid, pid):
            usb = os.path.join(td, "devices", tty + "-usb")
            intf = os.path.join(usb, "1-1:1.0")
            os.makedirs(intf)
            open(os.path.join(usb, "idVendor"), "w").write(vid + "\n")
            open(os.path.join(usb, "idProduct"), "w").write(pid + "\n")
            os.makedirs(os.path.join(td, "class", tty))
            os.symlink(intf, os.path.join(td, "class", tty, "device"))
        fake("ttyACM0", "2341", "0043")
        fake("ttyACM1", "2886", "002d")  # UF2 bootloader CDC 应跳过
        fake("ttyACM2", "2886", "802d")
        check("linux sysfs VID/PID filter", linux_ports(os.path.join(td, "class")) == ["/dev/ttyACM2"])

        drv = os.path.join(td, "drv")
        os.makedirs(drv)
        open(os.path.join(drv, "CURRENT.UF2"), "wb").write(full)
        man = {"payload_base": 0x4000, "block_count": 6, "payload_sha256": want}
        check("CURRENT.UF2 match", check_drive_current(drv, man) == "match")
        check("CURRENT.UF2 old", check_drive_current(drv, dict(man, payload_sha256="0" * 64)) == "old")

    import termios
    check("termios has B115200/B1200", hasattr(termios, "B115200") and hasattr(termios, "B1200"))
    print("SELF-TEST %s" % ("OK" if ok else "FAILED"))
    return 0 if ok else 1

def main():
    ap = build_parser()
    args = ap.parse_args()
    if args.self_test:
        sys.exit(self_test())
    if os.name != "posix":
        sys.exit("仅支持 Linux / macOS")
    err = args_error(args)
    if err:
        ap.error(err)
    man = load_manifest()
    log("目标固件 %s | build %s（%s）" % (man["fw_version"], man["build_id"], os.path.join(DIST, man["uf2_file"])))
    if args.monitor:
        log("调试模式：将等待 JSON 结果%s" % ("并保存 logs/" if args.save_log else "，不保存日志"))
    else:
        log("flash-only 模式：烧录完成后立即结束，不等待测试结果")
    try:
        while True:
            ok = run_one(man, args)
            if args.once:
                sys.exit(0 if ok else 2)
            wait_unplug()
    except KeyboardInterrupt:
        print()

if __name__ == "__main__":
    main()
