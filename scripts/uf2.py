# PlatformIO post-build: firmware.bin -> firmware.uf2 (SAMD51, app base 0x4000, family 0x55114460)
import hashlib, json, os, re, shutil, struct
Import("env")

UF2_MAGIC0, UF2_MAGIC1, UF2_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
BASE, FAMILY = 0x4000, 0x55114460

def bin_to_uf2(src, dst):
    data = open(src, "rb").read()
    n = (len(data) + 255) // 256
    out = bytearray()
    for i in range(n):
        chunk = data[i * 256:(i + 1) * 256].ljust(256, b"\x00")
        hdr = struct.pack("<IIIIIIII", UF2_MAGIC0, UF2_MAGIC1, 0x2000, BASE + i * 256, 256, i, n, FAMILY)
        out += hdr + chunk + b"\x00" * (512 - 32 - 256 - 4) + struct.pack("<I", UF2_END)
    open(dst, "wb").write(out)
    print("UF2 written: %s (%d blocks)" % (dst, n))

def cfg_str(text, name):
    m = re.search(r'#define\s+%s\s+"([^"]*)"' % name, text)
    return m.group(1) if m else None

def after_bin(source, target, env):
    b = target[0].get_abspath()
    uf2 = b[:-4] + ".uf2"
    bin_to_uf2(b, uf2)
    root = env.subst("$PROJECT_DIR")
    dist = os.path.join(root, "dist")
    os.makedirs(dist, exist_ok=True)
    shutil.copyfile(uf2, os.path.join(dist, "wio-factory-test.uf2"))
    data = open(b, "rb").read()
    n = (len(data) + 255) // 256
    payload = data.ljust(n * 256, b"\x00")
    cfg = open(os.path.join(root, "include", "test_config.h"), encoding="utf-8").read()
    manifest = {
        "fw_version": cfg_str(cfg, "FW_VERSION"),
        "build_id": env["FW_BUILD_ID"],
        "uf2_file": "wio-factory-test.uf2",
        "uf2_sha256": hashlib.sha256(open(uf2, "rb").read()).hexdigest(),
        "payload_sha256": hashlib.sha256(payload).hexdigest(),
        "payload_base": BASE,
        "block_count": n,
        "family_id": FAMILY,
        "wifi_ssid_2g": cfg_str(cfg, "WIFI_SSID_2G"),
        "wifi_ssid_5g": cfg_str(cfg, "WIFI_SSID_5G"),
        "wifi_connect_ssid": cfg_str(cfg, "WIFI_CONNECT_SSID"),
        "wifi_connect_enable": bool(re.search(r"#define\s+WIFI_CONNECT_ENABLE\s+1\b", cfg)),
    }
    with open(os.path.join(dist, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write("\n")
    print("dist updated: build_id=%s blocks=%d" % (manifest["build_id"], n))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_bin)
