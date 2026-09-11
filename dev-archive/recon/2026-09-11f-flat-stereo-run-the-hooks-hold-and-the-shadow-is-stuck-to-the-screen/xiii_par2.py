# Disparity of small patches between the two eye views, with an explicit eye offset (screen px).
# usage: python xiii_par2.py img.png OFFSET x,y,name[,half] ...   (x,y in window-capture coords, left eye)
import sys
from PIL import Image
import numpy as np

im = np.asarray(Image.open(sys.argv[1]).convert('L')).astype(np.float32)
off = int(sys.argv[2])
for spec in sys.argv[3:]:
    parts = spec.split(',')
    cx, cy, name = int(parts[0]), int(parts[1]), parts[2]
    h = int(parts[3]) if len(parts) > 3 else 12
    patch = im[cy-h:cy+h, cx-h:cx+h]
    best = None
    for dx10 in range(-450, 451):
        dx = dx10 / 10.0
        x = cx + off + dx
        xi = int(np.floor(x)); fr = x - xi
        a = im[cy-h:cy+h, xi-h:xi+h]; b = im[cy-h:cy+h, xi-h+1:xi+h+1]
        cand = a * (1 - fr) + b * fr
        p = patch - patch.mean(); q = cand - cand.mean()
        ncc = (p*q).sum() / (np.sqrt((p*p).sum() * (q*q).sum()) + 1e-6)
        if best is None or ncc > best[1]:
            best = (dx, ncc)
    print(f"{name:14s} ({cx},{cy}) half={h}: right-eye shift {best[0]:+.1f} px  ncc {best[1]:.3f}")
