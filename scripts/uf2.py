# PlatformIO post-build: firmware.bin -> firmware.uf2 (SAMD51, app base 0x4000, family 0x55114460)
import struct
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

def after_bin(source, target, env):
    b = target[0].get_abspath()
    bin_to_uf2(b, b[:-4] + ".uf2")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_bin)
