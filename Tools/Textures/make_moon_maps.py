"""Builds the moon maps from NASA (and NASA-funded USGS Astrogeology) global mosaics.

usage: make_moon_maps.py <cache dir> <texture dir>

Sources are downloaded into the cache directory (about 1.5 GB; files already there are reused).
Each map is written to <texture dir>/Solar System/<Planet>/Moons/<Moon> <W>x<H>.jpg as an
equirectangular, east-positive map centred on longitude 0 (the longitude facing the planet), the
layout Shaders/Planet.frag expects. Areas the spacecraft never imaged (the poles of the Galilean
moons, the northern hemispheres of Triton and the Uranian moons) are filled smoothly from the
surrounding surface. Greyscale mosaics are tinted with the moon's colour.

Prints each map's albedo scale (the reflectance of its brightest point, for Moon::surfaceAlbedo),
from the mean albedos in MAPS.

Sources:
  NASA 3D Resources (github.com/nasa/NASA-3D-Resources): Io, Phobos, Deimos, Miranda, Ariel,
    Umbriel, Titania and Oberon (Galileo, Viking and Voyager 2)
  USGS Astrogeology mosaics (asc-pds-services.s3.us-west-2.amazonaws.com/mosaic/), made for
    NASA's Planetary Data System: the Moon (LRO WAC), Europa, Ganymede and Callisto (Voyager and
    Galileo SSI), Mimas, Enceladus, Tethys, Dione, Rhea, Iapetus and Titan (Cassini ISS) and
    Triton (Voyager 2)
"""
import os
import struct
import sys
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

NASA_3D = 'https://raw.githubusercontent.com/nasa/NASA-3D-Resources/master/Images%20and%20Textures/'
USGS = 'https://asc-pds-services.s3.us-west-2.amazonaws.com/mosaic/'

# name: (planet, source, centre longitude (east), size, mean albedo or None, tint (linear RGB) or None)
#   source: 'nasa3d:<folder>', 'usgs:<file>' or 'wac' (the LRO WAC mosaic, sampled by rows)
#   mean albedo: the map is scaled so its mean reflectance is this. None keeps the brightest point
#   at the albedo given by BRIGHTEST.
MAPS = {
    'Moon':      ('Earth',   'wac', 0, (1536, 768), 0.12, (1.0, 0.93, 0.85)),
    'Phobos':    ('Mars',    'nasa3d:Mars - Phobos', 0, (1024, 512), 0.07, (1.0, 0.92, 0.85)),
    'Deimos':    ('Mars',    'nasa3d:Mars - Deimos', 0, (1024, 512), 0.07, (1.0, 0.92, 0.85)),
    'Io':        ('Jupiter', 'nasa3d:Jupiter - Io (A)', 0, (1440, 720), 0.62, None),
    'Europa':    ('Jupiter', 'usgs:Europa_Voyager_GalileoSSI_global_mosaic_500m.tif', 180, (1536, 768), 0.68, (1.0, 0.93, 0.82)),
    'Ganymede':  ('Jupiter', 'usgs:Ganymede_Voyager_GalileoSSI_Global_ClrMosaic_1435m.tif', 180, (1536, 768), 0.44, None),
    'Callisto':  ('Jupiter', 'usgs:Callisto_Voyager_GalileoSSI_global_mosaic_1km.tif', 180, (1536, 768), 0.19, (1.0, 0.9, 0.78)),
    'Mimas':     ('Saturn',  'usgs:Mimas/Cassini_DLR_Mimas.zip', 0, (1024, 512), 0.6, (1.0, 0.98, 0.96)),
    'Enceladus': ('Saturn',  'usgs:Enceladus_Cassini_mosaic_global_110m.tif', 180, (1024, 512), 0.9, (0.97, 0.99, 1.0)),
    'Tethys':    ('Saturn',  'usgs:Tethys_Cassini_mosaic_global_293m.tif', 0, (1024, 512), 0.8, (1.0, 0.99, 0.97)),
    'Dione':     ('Saturn',  'usgs:Dione_Cassini_Voyager_mosaic_global_154m.tif', 0, (1024, 512), 0.65, (1.0, 0.98, 0.95)),
    'Rhea':      ('Saturn',  'usgs:Rhea_Cassini_Voyager_mosaic_global_417m.tif', 180, (1024, 512), 0.65, (1.0, 0.97, 0.93)),
    'Titan':     ('Saturn',  'usgs:Titan_ISS_P19658_Mosaic_Global_4km.tif', 180, (1536, 768), 0.2, (1.0, 0.85, 0.65)),
    'Iapetus':   ('Saturn',  'usgs:Iapetus_Cassini_Voyager_mosaic_global_783m.tif', 0, (1024, 512), None, 'iapetus'),
    'Miranda':   ('Uranus',  'nasa3d:Uranus - Miranda', 0, (1024, 512), 0.32, None),
    'Ariel':     ('Uranus',  'nasa3d:Uranus - Ariel', 0, (1024, 512), 0.45, None),
    'Umbriel':   ('Uranus',  'nasa3d:Uranus - Umbriel', 0, (1024, 512), 0.2, None),
    'Titania':   ('Uranus',  'nasa3d:Uranus - Titania', 0, (1024, 512), 0.35, None),
    'Oberon':    ('Uranus',  'nasa3d:Uranus - Oberon', 0, (1024, 512), 0.3, None),
    'Triton':    ('Neptune', 'usgs:Triton_Voyager2_ClrMosaic_GlobalFill_600m.tif', 0, (1536, 768), 0.75, (1.0, 0.93, 0.88)),
}
# Pixels to trim (at twice the map's size) from the edges of the imaged area, where Voyager's views of
# the Uranian moons and Triton darken towards the terminator
ERODE = {'Miranda': 13, 'Ariel': 13, 'Umbriel': 13, 'Titania': 13, 'Oberon': 13, 'Triton': 11}
# Iapetus' bright trailing hemisphere is ice (albedo about 0.6); its leading side is as dark as coal
BRIGHTEST = {'Iapetus': 0.6}


def download(url, path):
    if os.path.exists(path):
        return path
    print('downloading', url, flush=True)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    urllib.request.urlretrieve(url, path + '.part')
    os.replace(path + '.part', path)
    return path


def read_tiff(path):
    """Uncompressed, stripped 8-bit TIFFs (most USGS mosaics): read directly, as PIL misreads some.
    Falls back to PIL for anything else."""
    im = Image.open(path)
    tags = im.tag_v2
    if tags.get(259, 1) != 1 or 324 in tags or tags.get(258, (8,))[0] != 8:
        return np.asarray(im)
    w, h, bands = tags[256], tags[257], tags.get(277, 1)
    rows_per_strip = tags.get(278, h)
    raw = open(path, 'rb').read()
    out = np.zeros((h, w, bands), np.uint8)
    planar = tags.get(284, 1) == 2
    offsets = tags[273]
    if planar:
        per_band = len(offsets) // bands
        for b in range(bands):
            for i in range(per_band):
                y0 = i * rows_per_strip
                rows = min(rows_per_strip, h - y0)
                out[y0:y0 + rows, :, b] = np.frombuffer(raw, np.uint8, rows * w, offsets[b * per_band + i]).reshape(rows, w)
    else:
        for i, offset in enumerate(offsets):
            y0 = i * rows_per_strip
            rows = min(rows_per_strip, h - y0)
            out[y0:y0 + rows] = np.frombuffer(raw, np.uint8, rows * w * bands, offset).reshape(rows, w, bands)
    return out[..., 0] if bands == 1 else out


def lro_wac(cache):
    """The LRO WAC global mosaic (303 pixels per degree, 6 GB): 2048 of its rows, fetched by range
    request, box filtered across to 4096 columns"""
    path = os.path.join(cache, 'moon_wac_4096x2048.npy')
    if os.path.exists(path):
        return np.load(path)
    url = USGS + 'LRO_WAC_Mosaic_Global_303ppd.tif'
    head = urllib.request.urlopen(urllib.request.Request(url, headers={'Range': 'bytes=0-1048575'})).read()
    assert struct.unpack('<H', head[2:4])[0] == 43, 'expected a BigTIFF'
    width, height, strip_offsets = 109164, 54582, 437008
    offsets = struct.unpack('<%dQ' % height, head[strip_offsets:strip_offsets + 8 * height])
    rows = [int((i + 0.5) * height / 2048) for i in range(2048)]

    def fetch(row):
        start = offsets[row]
        request = urllib.request.Request(url, headers={'Range': f'bytes={start}-{start + width - 1}'})
        return np.frombuffer(urllib.request.urlopen(request, timeout=120).read(), np.uint8)

    print('downloading 2048 rows of', url, flush=True)
    with ThreadPoolExecutor(16) as pool:
        image = np.stack(list(pool.map(fetch, rows)))
    edges = np.linspace(0, width, 4097).astype(int)
    sums = np.concatenate([np.zeros((2048, 1)), np.cumsum(image, axis=1, dtype=np.float64)], axis=1)
    columns = ((sums[:, edges[1:]] - sums[:, edges[:-1]]) / np.diff(edges)).astype(np.float32)
    np.save(path, columns)
    return columns


def load(source, cache):
    """The source map (8-bit grey or RGB, or float grey for the WAC mosaic) and a mask of where it
    has data"""
    if source == 'wac':
        image = lro_wac(cache) / 255.0
        return image, image > 0.5 / 255.0
    if source.startswith('nasa3d:'):
        folder = source[7:]
        quoted = urllib.parse.quote(folder)
        path = download(f'{NASA_3D}{quoted}/{quoted}.jpg', os.path.join(cache, 'nasa3d', folder + '.jpg'))
        image = np.asarray(Image.open(path).convert('RGB'))
    else:
        name = source[5:]
        path = download(USGS + name, os.path.join(cache, 'usgs', os.path.basename(name)))
        if path.endswith('.zip'):
            import zipfile
            with zipfile.ZipFile(path) as archive:
                member = next(n for n in archive.namelist() if n.endswith('_DLR_basemap.tif'))
                target = os.path.join(cache, 'usgs', os.path.basename(member))
                if not os.path.exists(target):
                    with open(target, 'wb') as f:
                        f.write(archive.read(member))
                path = target
        image = read_tiff(path)
    # No data is black (the NASA 3D Uranian maps' unseen north is near black, with JPEG noise)
    grey = image if image.ndim == 2 else image.max(axis=2)
    return image, grey > (12 if source.startswith('nasa3d:Uranus') else 1)


def reduce(image, mask, width):
    """Averages the mapped pixels in blocks, down to at least `width` columns, in float 0..1 (H, W, C)"""
    factor = max(1, image.shape[1] // width)
    h, w = image.shape[0] // factor, image.shape[1] // factor
    channels = 1 if image.ndim == 2 else image.shape[2]
    total = np.zeros((h, w, channels), np.float64)
    count = np.zeros((h, w), np.float64)
    for y in range(h):  # Row by row, to bound memory
        rows = slice(y * factor, (y + 1) * factor)
        block = image[rows, :w * factor].reshape(factor, w, factor, channels).astype(np.float64)
        weight = mask[rows, :w * factor].reshape(factor, w, factor).astype(np.float64)
        total[y] = (block * weight[..., None]).sum(axis=(0, 2))
        count[y] = weight.sum(axis=(0, 2))
    scale = 255.0 if image.dtype == np.uint8 else 1.0
    return total / np.maximum(count, 1e-9)[..., None] / scale, count / (factor * factor)


def resize(image, size, method=Image.LANCZOS):
    return np.stack([np.asarray(Image.fromarray(image[..., c].astype(np.float32)).resize(size, method))
                     for c in range(image.shape[2])], axis=2)


def fill(image, coverage):
    """Fills the unmapped areas (coverage 0) by pulling the surrounding surface in from ever
    coarser scales"""
    levels = []
    total, weight = image * coverage[..., None], coverage
    while min(weight.shape) > 2:
        levels.append((total, weight))
        h, w = weight.shape[0] // 2, weight.shape[1] // 2
        total = total[:2 * h, :2 * w].reshape(h, 2, w, 2, -1).sum(axis=(1, 3))
        weight = weight[:2 * h, :2 * w].reshape(h, 2, w, 2).sum(axis=(1, 3))
    # Whatever is still unmapped at the coarsest scale (a whole unseen hemisphere) takes the mean
    mean = levels[0][0].sum(axis=(0, 1)) / levels[0][1].sum()
    result = np.where(weight[..., None] > 0, total / np.maximum(weight, 1e-9)[..., None], mean)
    for total, weight in reversed(levels):
        coarse = resize(result, (weight.shape[1], weight.shape[0]), Image.BILINEAR)
        known = np.clip(weight, 0.0, 1.0)[..., None]
        result = known * total / np.maximum(weight, 1e-9)[..., None] + (1.0 - known) * coarse
    return result


def make(name, cache, out_dir):
    planet, source, centre, size, mean_albedo, tint = MAPS[name]
    image, mask = load(source, cache)
    image, coverage = reduce(image, mask, 2 * size[0])
    if name in ERODE:
        from PIL import ImageFilter
        coverage = np.asarray(Image.fromarray(coverage.astype(np.float32)).filter(ImageFilter.MinFilter(ERODE[name])))
    if coverage.min() < 0.999:
        image = fill(image, coverage)
    # East-positive, centred on longitude 0
    image = np.roll(image, int(round(-centre / 360.0 * image.shape[1])), axis=1)
    image = np.clip(resize(image, size), 0.0, 1.0)
    if image.shape[2] == 1:
        image = image[..., 0]

    # Decode to linear reflectance. The greyscale mosaics are close to linear in reflectance already
    # (calibrated I/F); the colour mosaics and the NASA 3D maps are display images.
    linear = image if (image.ndim == 2 and not source.startswith('nasa3d:')) else image ** 2.2
    if tint == 'iapetus':
        # Grey to the dark material's reddish brown, in proportion to darkness
        brown = np.array([0.55, 0.38, 0.27])
        t = np.clip(linear / np.percentile(linear, 99.5), 0, 1)[..., None]
        linear = linear[..., None] * (brown + (1 - brown) * t)
    elif tint is not None:
        if linear.ndim == 3:
            linear = linear @ np.array([0.2126, 0.7152, 0.0722])
        linear = linear[..., None] * np.array(tint)
    elif linear.ndim == 2:
        linear = linear[..., None].repeat(3, axis=2)
    linear = linear / np.percentile(linear.max(axis=2), 99.8)
    linear = np.clip(linear, 0.0, 1.0)

    luminance = linear @ np.array([0.2126, 0.7152, 0.0722])
    brightest = BRIGHTEST[name] if mean_albedo is None else mean_albedo / luminance.mean()
    out = Image.fromarray((linear ** (1 / 2.2) * 255 + 0.5).astype(np.uint8))
    path = os.path.join(out_dir, 'Solar System', planet, 'Moons', f'{name} {size[0]}x{size[1]}.jpg')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    out.save(path, quality=92)
    print(f'{name:10s} {path}  albedo scale {brightest:.3f}', flush=True)
    return brightest


if __name__ == '__main__':
    cache, out_dir = sys.argv[1:3]
    names = sys.argv[3:] or list(MAPS)
    for moon in names:
        make(moon, cache, out_dir)
