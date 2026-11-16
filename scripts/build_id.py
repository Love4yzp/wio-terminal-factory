# PlatformIO pre-script: deterministic FW_BUILD_ID = sha256(src/ include/ platformio.ini)[:8]
import hashlib, os
Import("env")

def compute(root):
    h = hashlib.sha256()
    files = [os.path.join(root, "platformio.ini")]
    for d in ("src", "include"):
        for dp, _, fns in os.walk(os.path.join(root, d)):
            files += [os.path.join(dp, f) for f in fns]
    for p in sorted(files, key=lambda p: os.path.relpath(p, root).replace(os.sep, "/")):
        h.update(os.path.relpath(p, root).replace(os.sep, "/").encode() + b"\0")
        with open(p, "rb") as f:
            h.update(f.read().replace(b"\r\n", b"\n") + b"\0")
    return h.hexdigest()[:8]

bid = compute(env.subst("$PROJECT_DIR"))
env["FW_BUILD_ID"] = bid
env.Append(CPPDEFINES=[("FW_BUILD_ID", env.StringifyMacro(bid))])
print("FW_BUILD_ID =", bid)
