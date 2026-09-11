# Pack the 2026-09-11f XIII /lm evidence into the recon folder: client-area JPGs, game-only logs, scripts.
import os, re, shutil
from PIL import Image

S = r"C:\Users\Tefa\AppData\Local\Temp\claude\D--Program-Files--x86--Steam-steamapps-common\83896c25-936b-49aa-9e03-2eb792c66e2b\scratchpad"
D = r"D:\claude video game stuff\github-backups\XIII2003-vr\dev-archive\recon\2026-09-11f-flat-stereo-run-the-hooks-hold-and-the-shadow-is-stuck-to-the-screen"
os.makedirs(D, exist_ok=True)

shots = {
    "x11-mono.png": "01-banque01-mono-before-numpad7.jpg",
    "x12-stereo.png": "02-banque01-stereo-on-ipd3.40.jpg",
    "x13-ipdup.png": "03-banque01-ipd4.15.jpg",
    "x14-swap.png": "04-banque01-eyes-swapped.jpg",
    "x15-back.png": "05-banque01-back-to-ipd3.40.jpg",
    "x17.png": "06-banque01-lobby-stereo.jpg",
    "x23.png": "07-banque01-clerk-no-visible-shadow.jpg",
    "s6.png": "08-spin-test-t6s.jpg",
    "s12.png": "09-spin-test-t12s.jpg",
    "p1.png": "10-plage01-cutscene-character-shadow.jpg",
    "p2.png": "11-plage01-first-person-ipd13.40-arm-shadow.jpg",
}
for src, dst in shots.items():
    im = Image.open(os.path.join(S, src)).convert("RGB").crop((8, 31, 8 + 1280, 31 + 960))
    im.save(os.path.join(D, dst), quality=85)
for src, dst in [("p1-shadow.png", "10b-plage01-cutscene-shadow-crop.jpg"),
                 ("p2-shadow.png", "11b-plage01-arm-shadow-crop-left-right.jpg"),
                 ("x23-feet.png", "07b-banque01-clerk-feet-crop.jpg")]:
    Image.open(os.path.join(S, src)).convert("RGB").save(os.path.join(D, dst), quality=90)

for src, dst in [("stereo-run1.log", "xiii_stereo-run1-banque01-stereo2.log"),
                 ("stereo-run2.log", "xiii_stereo-run2-banque01-stereo1-livehmd.log"),
                 ("stereo-run3.log", "xiii_stereo-run3-plage01-disklog-build.log")]:
    shutil.copyfile(os.path.join(S, src), os.path.join(D, dst))

# debug-output captures: keep only the game's own process lines (drop Steam and other processes)
for src, pid, dst in [("ods-run1.txt", "4500", "ods-run1-game-lines.txt"),
                      ("ods-run2.txt", "10380", "ods-run2-game-lines.txt")]:
    out = []
    with open(os.path.join(S, src), encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            if f"[{pid}]" in line and line.split(f"[{pid}]", 1)[1].strip():
                out.append(line.rstrip("\n"))
    with open(os.path.join(D, dst), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")

for src in ["xdrive.ps1", "xiii_par.py", "xiii_par2.py", "xiii_ini_run2.py", "pack_recon.py"]:
    shutil.copyfile(os.path.join(S, src), os.path.join(D, src))
print("\n".join(sorted(os.listdir(D))))
