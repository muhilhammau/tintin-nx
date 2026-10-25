#!/usr/bin/env python3
import os, shutil, sys, zipfile

LIBS = ["libStormGLOFT.so", "libtintin.so"]
if len(sys.argv) != 2:
    raise SystemExit("usage: tools/prepare_apk.py /path/to/Tintin.apk")
apk = os.path.abspath(sys.argv[1])
out = os.path.abspath('.apk_libs')
os.makedirs(out, exist_ok=True)
with zipfile.ZipFile(apk) as z:
    for lib in LIBS:
        member = 'lib/armeabi-v7a/' + lib
        if member not in z.namelist():
            raise SystemExit(f"{member} not found in {apk}")
        with z.open(member) as src, open(os.path.join(out, lib), 'wb') as dst:
            shutil.copyfileobj(src, dst)
        print(os.path.join(out, lib))
