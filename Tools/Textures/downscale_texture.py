"""Writes copies of textures that fit SV's decoded (RGBA8) texture budget of 5 MB.
usage: downscale_texture.py map <src> <dst> <width> <height>   (resample)
       downscale.py ring <src> <dst>                   (1D radial profile: keep width, 4 rows)"""
import sys, numpy as np
from PIL import Image
mode, src, dst = sys.argv[1:4]
img = Image.open(src)
if mode == 'map':
    out = img.resize((int(sys.argv[4]), int(sys.argv[5])), Image.LANCZOS)
else:
    rgba = np.asarray(img.convert('RGBA')).astype(np.float64)
    profile = rgba.mean(axis=0)  # Every row holds the same radial profile
    out = Image.fromarray(np.repeat(profile[None].round().astype(np.uint8), 4, axis=0))
kw = {'quality': 95} if dst.lower().endswith('.jpg') else {}
out.save(dst, **kw)
print(dst, out.size, 'decoded RGBA bytes', out.size[0] * out.size[1] * 4)
