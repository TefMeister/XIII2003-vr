# Add the 2026-09-12 fix-verification evidence to the same recon folder.
import os, shutil
from PIL import Image

S = r"C:\Users\Tefa\AppData\Local\Temp\claude\D--Program-Files--x86--Steam-steamapps-common\83896c25-936b-49aa-9e03-2eb792c66e2b\scratchpad"
D = r"D:\claude video game stuff\github-backups\XIII2003-vr\dev-archive\recon\2026-09-11f-flat-stereo-run-the-hooks-hold-and-the-shadow-is-stuck-to-the-screen"

shots = {
    "q2.png": "12-plage01-hud-fixed-both-eyes.jpg",
    "q3-fix-on.png": "13-plage01-shadow-fix-ON-ipd13.40.jpg",
    "q4-fix-off.png": "14-plage01-shadow-fix-OFF-ipd13.40.jpg",
    "q5-fix-on-again.png": "15-plage01-shadow-fix-ON-again.jpg",
    "r2.png": "16-banque01-objective-card-both-eyes.jpg",
    "r3-hud-fixed.png": "17-banque01-hud-fix-ON.jpg",
    "r4-hud-old.png": "18-banque01-hud-fix-OFF.jpg",
}
for src, dst in shots.items():
    Image.open(os.path.join(S, src)).convert("RGB").crop((8, 31, 1288, 991)).save(os.path.join(D, dst), quality=85)
for src, dst in [("q-shadow-ab.png", "13b-shadow-AB-crop-top-fixed-bottom-old.jpg"),
                 ("r-hud-ab.png", "17b-hud-AB-strip-top-fixed-bottom-old.jpg")]:
    Image.open(os.path.join(S, src)).convert("RGB").save(os.path.join(D, dst), quality=90)
shutil.copyfile(r"C:\Users\Tefa\AppData\Local\Temp\xiii_capture\xiii_stereo.log",
                os.path.join(D, "xiii_stereo-all-runs-through-2026-09-12.log"))
os.remove(os.path.join(D, "xiii_stereo-all-three-runs.log"))
shutil.copyfile(os.path.join(S, "xiii_ini_final.py"), os.path.join(D, "xiii_ini_final.py"))
shutil.copyfile(os.path.join(S, "pack_recon2.py"), os.path.join(D, "pack_recon2.py"))
print("\n".join(sorted(os.listdir(D))))
