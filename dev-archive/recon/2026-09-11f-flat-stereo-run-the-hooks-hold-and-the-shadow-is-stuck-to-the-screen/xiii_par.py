# Measure horizontal disparity between left and right halves of a SBS screenshot.
# For each probe patch in the left half, find the best-matching x-offset in the right half (same rows).
import sys
from PIL import Image
import numpy as np

path = sys.argv[1]
im = np.asarray(Image.open(path).convert('L')).astype(np.float32)
# window rect capture: client starts at x=8,y=31 ; client 1280x960
cl = im[31:31+960, 8:8+1280]
L = cl[:, :640]; R = cl[:, 640:]
probes = [(int(a), int(b), c) for a, b, c in [p.split(',') for p in sys.argv[2:]]] if len(sys.argv) > 2 else []
if not probes:
    probes = [(80, 400, 'near-left-pillar'), (320, 380, 'fountain-far'), (320, 150, 'balcony-far'), (300, 800, 'floor-near'), (600, 400, 'right-pillar')]
for cx, cy, name in probes:
    h = 40; w = 40
    patch = L[cy-h:cy+h, cx-w:cx+w]
    best = None
    for dx in range(-40, 41):
        x0 = cx - w + dx
        if x0 < 0 or x0 + 2*w > 640:
            continue
        cand = R[cy-h:cy+h, x0:x0+2*w]
        a = patch - patch.mean(); b = cand - cand.mean()
        den = np.sqrt((a*a).sum() * (b*b).sum()) + 1e-6
        ncc = (a*b).sum() / den
        if best is None or ncc > best[1]:
            best = (dx, ncc)
    print(f"{name:18s} at ({cx},{cy}): right-eye shift {best[0]:+d} px  (ncc {best[1]:.3f})")
