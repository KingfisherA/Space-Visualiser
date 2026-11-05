"""Builds an equirectangular Saturn texture from the NASA PDS Cassini ISS global colour map
(co_iss_global-maps, Cassini_ISS_RGB_Saturn_global_color_map_original.fits, 2011-08-11).
Row 0 of the FITS cube is the north pole (the ring shadow gap is southern, the 2010-11 storm northern).
Data gaps (polar caps, ring shadow band, equator line) are filled (see the comments below)."""
import sys, numpy as np
from PIL import Image

src, dst, out_w, out_h = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
H, W = 1801, 3601
cube = np.fromfile(src, dtype='>f4', offset=2880, count=3 * H * W).reshape(3, H, W).astype(np.float64)
cube = cube[:, :, :W - 1]                          # Columns 0 and 3600 are the same meridian
bad_rows = (~np.isfinite(cube) | (cube <= 0)).any(axis=(0, 2))
# The rows just north of the ring shadow gap (1.9-5.3 S) are dimmed by the shadow's edge: refill them too
bad_rows[int(round((90 + 1.9) * 10)):int(round((90 + 5.4) * 10))] = True

def lat_of(row): return 90.0 - 0.1 * row
def row_of(lat): return int(round((90.0 - lat) * 10))

# Contiguous runs of missing rows
runs, start = [], None
for r, b in enumerate(bad_rows):
    if b and start is None: start = r
    if not b and start is not None: runs.append((start, r - 1)); start = None
if start is not None: runs.append((start, H - 1))
print('gaps (rows -> latitude):', [(a, b, lat_of(a), lat_of(b)) for a, b in runs])

out = cube.copy()
for a, b in runs:
    n = b - a + 1
    if a == 0 or b == H - 1:
        # Polar cap: reflect the valid latitudes across the boundary, fading to the boundary's
        # zonal mean colour towards the pole (no data exists there)
        edge = b + 1 if a == 0 else a - 1
        step = 1 if a == 0 else -1
        zonal = cube[:, edge:edge + 4 * step:step, :].mean(axis=(1, 2))
        for i in range(n):
            r = (b - i) if a == 0 else (a + i)       # i = distance from the boundary
            refl = cube[:, edge + (i + 1) * step, :]
            fade = min(1.0, (i + 1) / n)
            out[:, r, :] = refl * (1 - fade) + zonal[:, None] * fade
    elif n <= 20:
        # Thin gap (equator): linear interpolation per column
        top, bot = cube[:, a - 1, :], cube[:, b + 1, :]
        for i in range(n):
            t = (i + 1) / (n + 1)
            out[:, a + i, :] = top * (1 - t) + bot * t
    else:
        # Ring shadow band: mirror the other hemisphere, corrected (per zonal mean) so it joins the
        # valid data either side
        mirror = np.stack([cube[:, row_of(-lat_of(r)), :] for r in range(a - 1, b + 2)], axis=1)
        c_top = cube[:, a - 1, :].mean(axis=1) - mirror[:, 0, :].mean(axis=1)
        c_bot = cube[:, b + 1, :].mean(axis=1) - mirror[:, -1, :].mean(axis=1)
        for i in range(n):
            t = (i + 1) / (n + 1)
            out[:, a + i, :] = mirror[:, i + 1, :] + (c_top * (1 - t) + c_bot * t)[:, None]

rgb = np.clip(out, 0, 255).transpose(1, 2, 0).astype(np.uint8)
img = Image.fromarray(rgb).resize((out_w, out_h), Image.LANCZOS)
img.save(dst)
print('wrote', dst, img.size, 'decoded RGBA bytes', out_w * out_h * 4)
