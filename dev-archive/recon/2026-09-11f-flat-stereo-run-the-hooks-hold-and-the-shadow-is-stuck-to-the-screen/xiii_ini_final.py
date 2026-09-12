# Put XIII.ini back to last night's backup, then set the two keys we want left behind:
#   Stereo=2            -> stereo OFF until numpad 7, so a casual launch looks normal
#   AutomationEngineExec=0 -> hygiene: the 0.2.9 harness crash guard (queue file is emptied too)
import shutil
sysdir = r"D:\Program Files (x86)\Steam\steamapps\common\XIII - Classic\system"
p = sysdir + r"\XIII.ini"
bak = sysdir + r"\XIII.ini.bak-2026-09-11-lm"
shutil.copyfile(p, sysdir + r"\XIII.ini.bak-2026-09-12-end-of-lm")
d = open(bak, "rb").read()
assert b"[VR]\r\n" in d and b"AutomationEngineExec=1\r\n" in d
d = d.replace(b"[VR]\r\n", b"[VR]\r\nStereo=2\r\n", 1)
d = d.replace(b"AutomationEngineExec=1\r\n", b"AutomationEngineExec=0\r\n", 1)
open(p, "wb").write(d)
# empty the stale automation queue (kept as a dated copy)
q = sysdir + r"\xiii_automation_cmds.txt"
shutil.copyfile(q, sysdir + r"\xiii_automation_cmds.txt.bak-2026-09-12")
open(q, "wb").write(b"")
i = d.index(b"[VR]")
print(d[i:i + 200].decode("ascii", "replace"))
