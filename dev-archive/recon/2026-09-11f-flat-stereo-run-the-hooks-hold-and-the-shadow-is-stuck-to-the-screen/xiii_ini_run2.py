p = r"D:\Program Files (x86)\Steam\steamapps\common\XIII - Classic\system\XIII.ini"
d = open(p, 'rb').read()
assert b"[VR]\r\nStereo=2\r\n" in d
d = d.replace(b"[VR]\r\nStereo=2\r\n", b"[VR]\r\nStereo=1\r\nCameraLiveHmd=1\r\n", 1)
open(p, 'wb').write(d)
print("ok", len(d))
