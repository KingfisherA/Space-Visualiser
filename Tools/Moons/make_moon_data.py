"""Generates src/SV/Universe/MoonData.cpp: the orbits and sizes of every known moon of the planets.

usage: make_moon_data.py <cache dir> <output .cpp>

1. Downloads JPL's tables of satellite mean elements and physical parameters
   (ssd.jpl.nasa.gov/sats/elem/, /sats/phys_par/).
2. Asks JPL Horizons (ssd.jpl.nasa.gov/api/horizons.api) for each moon's position relative to its
   planet at 35 times between 2017 and 2035, with its radius and albedo. Responses are cached.
3. Fits a precessing ellipse to each moon's positions, in the reference plane of JPL's tables (the
   local Laplace plane, the planet's equator or the ecliptic), starting from the table's elements.
   The tables' periods and shapes are good, but not always their phases (several of Saturn's moons
   are 60 to 160 degrees out) or, for Neptune's moons, their periods, so the mean longitude, its
   rate, the orientation and the shape are fitted. The two precession rates come from the tables;
   as the tables don't give their directions, every combination is tried.
4. Writes the moons, with each fit's worst error over 2017 to 2035, to the .cpp.

Earth's Moon is computed in the app from a lunar theory instead (see Moon.cpp), but is fitted here
too for the record.

Needs numpy and scipy.
"""
import html
import json
import math
import os
import re
import sys
import time
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor

import numpy as np
from scipy.optimize import least_squares

DEG = math.pi / 180
OBLIQUITY = 84381.448 / 3600 * DEG
# The fitted elements' epoch: 2026-01-01 00:00 TDB
EPOCH_JD = 2461041.5
# Sample times relative to the epoch (days): dense near it, out to 9 years either side
OFFSETS = [0.0] + [s * 0.05 * 2 ** k for k in range(17) for s in (1, -1)]
# Widening spans of the samples to fit over, in days (scaled up for long periods), so a mean motion
# that is slightly out can't wrap the phase before it is corrected
STAGES = [0.8, 3.2, 12.8, 51.2, 204.8, 819.2, 3276.8]

PLANETS = {'Earth': '399', 'Mars': '499', 'Jupiter': '599', 'Saturn': '699', 'Uranus': '799', 'Neptune': '899'}
# North poles (IAU 2015) for JPL's "equatorial" frames. Uranus' satellites use its positive pole (by
# the right-hand rule), the opposite of its IAU north pole.
POLES = {'Earth': (0.0, 90.0), 'Mars': (317.68143, 52.88650), 'Jupiter': (268.056595, 64.495303),
         'Saturn': (40.589, 83.537), 'Uranus': (77.311, 15.175), 'Neptune': (299.36, 43.46)}
ECLIPTIC_POLE = (270.0, 90.0 - 84381.448 / 3600)
G = 6.6743e-20  # km^3 kg^-1 s^-2

# Moons without a radius from JPL: from their absolute magnitude V(1,0) and albedo where known,
# otherwise a typical size for the planet's small irregular moons (most are 1 to 4 km across)
DEFAULT_RADIUS_KM = {'Earth': 1.0, 'Mars': 1.0, 'Jupiter': 1.5, 'Saturn': 2.0, 'Uranus': 5.0, 'Neptune': 15.0}
# Moons that don't rotate synchronously, though their orbits are regular
NOT_SYNCHRONOUS = {'Hyperion', 'Phoebe', 'Nereid'}


def get(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return open(path).read()
    for attempt in range(5):
        try:
            data = urllib.request.urlopen(url, timeout=120).read().decode()
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, 'w').write(data)
            return data
        except Exception as error:
            print('retrying', url[:100], error, flush=True)
            time.sleep(5 * (attempt + 1))
    raise RuntimeError('could not download ' + url)


def table_rows(page):
    rows = []
    for row in re.findall(r'<tr[^>]*>(.*?)</tr>', page, flags=re.S):
        cells = re.findall(r'<t[dh][^>]*>(.*?)</t[dh]>', row, flags=re.S)
        rows.append([re.sub(r'\s+', ' ', html.unescape(re.sub(r'<[^>]+>', '', c))).strip() for c in cells])
    return rows


def number(text):
    text = text.strip().rstrip('.')
    try:
        return float(text) if text else 0.0
    except ValueError:
        return 0.0


def julian_day(date):
    y, m, d = date.split('-')
    y, m, d = int(y), int(m), float(d)
    if m <= 2:
        y, m = y - 1, m + 12
    a = y // 100
    return int(365.25 * (y + 4716)) + int(30.6001 * (m + 1)) + d + 2 - a + a // 4 - 1524.5


def equatorial_to_ecliptic(v):
    return np.array([v[0], math.cos(OBLIQUITY) * v[1] + math.sin(OBLIQUITY) * v[2],
                     -math.sin(OBLIQUITY) * v[1] + math.cos(OBLIQUITY) * v[2]])


def direction(ra, dec):
    ra, dec = ra * DEG, dec * DEG
    return np.array([math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)])


def reference_frame(pole):
    """Columns: the plane's x (its ascending node on the ICRF equator), y and pole, in ecliptic J2000
    coordinates (the frame of Horizons' vectors)"""
    p = direction(*pole)
    x = np.cross([0.0, 0.0, 1.0], p)
    x /= np.linalg.norm(x)
    y = np.cross(p, x)
    return np.stack([equatorial_to_ecliptic(v) for v in (x, y, p)], axis=1)


def orbit_positions(params, rates, frame, dts):
    """params: mean longitude, its rate (deg/day), argument of periapsis and node at the epoch (deg),
    a (km), e, inclination (deg); rates: of the argument of periapsis and the node (deg/day)"""
    L0, n, w0, node0, a, e, i = params
    e = abs(e)
    w = (w0 + rates[0] * dts) * DEG
    node = (node0 + rates[1] * dts) * DEG
    M = (L0 + n * dts) * DEG - w - node
    E = M + e * np.sin(M)
    for _ in range(60):
        E = E - (E - e * np.sin(E) - M) / (1 - e * np.cos(E))
    xp, yp = a * (np.cos(E) - e), a * math.sqrt(max(1 - e * e, 1e-12)) * np.sin(E)
    cw, sw, cn, sn = np.cos(w), np.sin(w), np.cos(node), np.sin(node)
    ci, si = math.cos(i * DEG), math.sin(i * DEG)
    v = np.stack([(cw * cn - sw * sn * ci) * xp + (-sw * cn - cw * sn * ci) * yp,
                  (cw * sn + sw * cn * ci) * xp + (-sw * sn + cw * cn * ci) * yp,
                  (sw * si) * xp + (cw * si) * yp])
    return (frame @ v).T


def horizons(code, planet, cache):
    query = {'format': 'text', 'COMMAND': f"'{code}'", 'OBJ_DATA': 'YES', 'MAKE_EPHEM': 'YES', 'EPHEM_TYPE': 'VECTORS',
             'CENTER': f"'500@{PLANETS[planet]}'", 'TLIST': ' '.join("'%.5f'" % (EPOCH_JD + o) for o in OFFSETS),
             'REF_PLANE': 'ECLIPTIC', 'REF_SYSTEM': 'J2000', 'VEC_TABLE': '1', 'OUT_UNITS': 'KM-S', 'CSV_FORMAT': 'YES'}
    text = get('https://ssd.jpl.nasa.gov/api/horizons.api?' + urllib.parse.urlencode(query),
               os.path.join(cache, 'horizons', code + '.txt'))
    info = {}
    head = text.split('$$SOE')[0]
    match = re.search(r'Radi(?:us|i)[^=:\n]*[=:][ \t]*~?([\d.]+)[ \t]*x[ \t]*([\d.]+)[ \t]*x[ \t]*([\d.]+)', head)
    if match:
        info['radius'] = float(np.prod([float(match.group(k)) for k in (1, 2, 3)]) ** (1 / 3))
    else:
        match = re.search(r'[Rr]adius[^=\n]*=[ \t]*~?([\d.]+)', head)
        if match:
            info['radius'] = float(match.group(1))
    match = re.search(r'Geometric [Aa]lbedo\s*=\s*([\d.]+)', head)
    if match:
        info['albedo'] = float(match.group(1))
    match = re.search(r'GM[^=\n]*=\s*([\d.]+(?:[Ee][+-]?\d+)?)', head)
    if match:
        info['gm'] = float(match.group(1))
    match = re.search(r'V\(1,0\)\s*=\s*([+-]?[\d.]+)', head)
    if match:
        info['h'] = float(match.group(1))
    match = re.search(r'Rot[a-z.]* [Pp]er[a-z.]*[ \t]*=[ \t]*([A-Za-z]+)', head)
    if match:
        info['synchronous'] = match.group(1).lower() == 'synchronous'
    ephemeris = re.search(r'\$\$SOE(.*?)\$\$EOE', text, re.S)
    if not ephemeris:
        return None, info
    rows = ephemeris.group(1).strip().splitlines()
    jd = np.array([float(r.split(',')[0]) for r in rows])
    positions = np.array([[float(x) for x in r.split(',')[2:5]] for r in rows])
    return (jd, positions), info


def fit(row, samples):
    _, planet, name, code, _, kind, epoch, a, e, w, M, i, node, P, Pw, Pn, ra, dec, _, _ = row
    a, e, w, M, i, node, P, Pw, Pn, ra, dec = map(number, (a, e, w, M, i, node, P, Pw, Pn, ra, dec))
    pole = ECLIPTIC_POLE if kind == 'ecliptic' else POLES[planet] if kind == 'equatorial' else (ra, dec)
    frame = reference_frame(pole)
    since = EPOCH_JD - julian_day(epoch)
    jd, positions = samples
    dts = jd - EPOCH_JD
    best = None
    for signs in [(sw, sn, with_rates) for sw in (1, -1) for sn in (1, -1) for with_rates in (False, True)]:
        rates = (signs[0] * (360 / (Pw * 365.25) if Pw > 0 else 0.0), signs[1] * (360 / (Pn * 365.25) if Pn > 0 else 0.0))
        # P is the sidereal period in some tables and the anomalistic period in others
        n = 360.0 / P + (rates[0] + rates[1] if signs[2] else 0.0)
        p = np.array([M + w + node + n * since, n, w + rates[0] * since, node + rates[1] * since, a, max(e, 1e-4), i])

        # The phase first, from several starts, over a short arc
        short = np.abs(dts) <= STAGES[0] * max(1.0, P / 3.0)
        def phase_error(x):
            return ((orbit_positions(np.r_[x, p[1:]], rates, frame, dts[short]) - positions[short]) / a).ravel()
        p[0] = min((least_squares(phase_error, [p[0] + start]) for start in range(0, 360, 30)), key=lambda r: r.cost).x[0]

        for stage, span in enumerate(STAGES):
            within = np.abs(dts) <= (span * max(1.0, P / 2.0) if P > 2 else span)
            if within.sum() < 4:
                continue
            free = [0, 2, 3, 4, 5, 6] if stage < 2 else [0, 1, 2, 3, 4, 5, 6]
            def error(x):
                q = p.copy()
                q[free] = x
                return ((orbit_positions(q, rates, frame, dts[within]) - positions[within]) / a).ravel()
            p[free] = least_squares(error, p[free], x_scale='jac').x
        # Fits that wandered off to an unphysical orbit (a sparse, perturbed arc can) are rejected
        if not (abs(p[5]) < 0.95 and 0.67 < p[4] / a < 1.5 and np.all(np.isfinite(p))):
            continue
        errors = np.linalg.norm(orbit_positions(p, rates, frame, dts) - positions, axis=1) / np.linalg.norm(positions, axis=1)
        if best is None or errors.max() < best['error']:
            best = dict(error=float(errors.max()), params=p.copy(), rates=rates, pole=pole)
    return best


def table_elements(row):
    """The table's own elements, carried to the epoch, for moons Horizons has no ephemeris for now"""
    _, planet, name, code, _, kind, epoch, a, e, w, M, i, node, P, Pw, Pn, ra, dec, _, _ = row
    a, e, w, M, i, node, P, Pw, Pn, ra, dec = map(number, (a, e, w, M, i, node, P, Pw, Pn, ra, dec))
    pole = ECLIPTIC_POLE if kind == 'ecliptic' else POLES[planet] if kind == 'equatorial' else (ra, dec)
    since = EPOCH_JD - julian_day(epoch)
    n = 360.0 / P
    rates = (360 / (Pw * 365.25) if Pw > 0 else 0.0, -360 / (Pn * 365.25) if Pn > 0 else 0.0)
    return dict(error=None, params=np.array([M + w + node + n * since, n, w + rates[0] * since, node + rates[1] * since, a, e, i]),
                rates=rates, pole=pole)


def display_name(name):
    match = re.match(r'^S(\d{4})_([A-Z])_(\d+)$', name)
    return f'S/{match.group(1)} {match.group(2)} {match.group(3)}' if match else name


def main(cache, output):
    elements = table_rows(get('https://ssd.jpl.nasa.gov/sats/elem/', os.path.join(cache, 'elem.html')))
    physical = table_rows(get('https://ssd.jpl.nasa.gov/sats/phys_par/', os.path.join(cache, 'phys.html')))
    rows = [r for r in elements if len(r) == 20 and r[0] != 'ID' and r[1] in PLANETS]
    by_code = {}
    for r in rows:
        by_code.setdefault(r[3], []).append(r)
    gm_table = {r[2]: number(r[3].split()[0]) for r in physical if len(r) == 6 and r[2].isdigit()}
    radius_table = {r[2]: number(r[4].split()[0]) for r in physical if len(r) == 6 and r[2].isdigit()}

    def process(code):
        candidates = by_code[code]
        samples, info = horizons(code, candidates[0][1], cache)
        if samples is None:
            print('no ephemeris for', candidates[0][2], '- using the table elements', flush=True)
            return candidates[0], table_elements(candidates[0]), info
        fits = [(r, fit(r, samples)) for r in candidates]
        fits = [f for f in fits if f[1] is not None]
        if not fits:
            print('no good fit for', candidates[0][2], '- using the table elements', flush=True)
            return candidates[0], table_elements(candidates[0]), info
        best = min(fits, key=lambda x: x[1]['error'])
        return best[0], best[1], info

    with ThreadPoolExecutor(4) as pool:
        results = list(pool.map(process, by_code))

    moons = []
    for row, orbit, info in results:
        planet, name, code, kind = row[1], row[2], row[3], row[5]
        # Moons with elements on the ecliptic are the irregulars, captured on distant, inclined orbits
        # (the Moon's elements are on the ecliptic too, as the Sun perturbs it most)
        regular = kind != 'ecliptic' or planet == 'Earth'
        albedo = info.get('albedo') or (0.5 if planet == 'Saturn' and regular else 0.06)
        radius = radius_table.get(code) or info.get('radius')
        estimated = radius is None
        if radius is None and 'h' in info:
            radius = 0.5 * 1329.0 / math.sqrt(albedo) * 10 ** (-info['h'] / 5)
        if radius is None:
            radius = DEFAULT_RADIUS_KM[planet]
        gm = gm_table.get(code) or info.get('gm') or G * 1500e9 * 4 / 3 * math.pi * radius ** 3  # 1.5 g/cm^3
        synchronous = info.get('synchronous', regular) and name not in NOT_SYNCHRONOUS
        L0, n, w0, node0, a, e, i = orbit['params']
        moons.append(dict(name=display_name(name), planet=planet, code=int(code), radius=radius, estimated=estimated,
                          mass=gm / G, albedo=albedo, synchronous=synchronous, regular=regular, pole=orbit['pole'],
                          a=a, e=abs(e), i=i, L0=L0 % 360, n=n, w0=w0 % 360, wRate=orbit['rates'][0],
                          node0=node0 % 360, nodeRate=orbit['rates'][1], error=orbit['error']))
        print(f"{name:14s} {planet:8s} error {'-' if orbit['error'] is None else '%.3f deg' % (orbit['error'] / DEG)}", flush=True)

    order = list(PLANETS)
    moons.sort(key=lambda m: (order.index(m['planet']), m['a']))
    write(moons, output)


def write(moons, output):
    lines = [
        '// Generated by Tools/Moons/make_moon_data.py from JPL\'s planetary satellite mean elements and',
        '// physical parameters (ssd.jpl.nasa.gov/sats/) and positions from JPL Horizons. Do not edit.',
        '#include "MoonData.hpp"',
        '',
        'namespace SV::Universe',
        '{',
        f'\tconst std::array<MoonData, {len(moons)}> MOON_DATA = {{ {{',
        '\t\t// name, planet, NAIF ID, radius (km), radius estimated, mass (kg), geometric albedo, synchronous, regular,',
        '\t\t// reference plane pole (RA, Dec), a (km), e, i, mean longitude, its rate (deg/day), argument of periapsis,',
        '\t\t// its rate, ascending node, its rate, worst fit error 2017-2035 (deg, -1 for none)',
    ]
    for m in moons:
        error = -1.0 if m['error'] is None else m['error'] / DEG
        lines.append(
            f"\t\t{{ \"{m['name']}\", \"{m['planet']}\", {m['code']}, {m['radius']:.4g}, {str(m['estimated']).lower()}, "
            f"{m['mass']:.4e}, {m['albedo']:.3g}, {str(m['synchronous']).lower()}, {str(m['regular']).lower()}, "
            f"{m['pole'][0]:.6f}, {m['pole'][1]:.6f}, {m['a']:.10g}, {m['e']:.6f}, {m['i']:.6f}, "
            f"{m['L0']:.6f}, {m['n']:.12g}, {m['w0']:.6f}, {m['wRate']:.10g}, {m['node0']:.6f}, {m['nodeRate']:.10g}, "
            f"{error:.3g} }},")
    lines += ['\t} };', '}', '']
    open(output, 'w').write('\n'.join(lines))
    print('wrote', len(moons), 'moons to', output)


if __name__ == '__main__':
    main(*sys.argv[1:3])
