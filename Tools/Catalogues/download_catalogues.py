"""Downloads the astronomical catalogues the universe view is built from.

usage: download_catalogues.py [output dir]    (default: Data Files/Catalogues)

The files are saved as published (VizieR tables as tab-separated text, with only the columns used),
and Space Visualiser parses them when it starts (src/SV/Universe/Catalogue.cpp):

  hygdata_v41.csv  HYG database v4.1 (Nash): ~120,000 stars from Hipparcos, Yale and Gliese, with
                   positions in parsecs, magnitudes, colours and names
                   (github.com/astronexus/HYG-Database)
  psrcat.db        ATNF Pulsar Catalogue (Manchester et al. 2005): every known pulsar, magnetar and
                   isolated neutron star, with DM or independent distances
                   (www.atnf.csiro.au/research/pulsar/psrcat)
  hmxb.tsv         Galactic high-mass X-ray binaries (Fortin et al. 2023), Gaia distances
  lmxb.tsv         Galactic low-mass X-ray binaries (Avakyan et al. 2023)
  ungc.tsv         Updated Nearby Galaxy Catalog (Karachentsev et al. 2013): the ~870 galaxies
                   within ~11 Mpc, with measured distances, sizes, inclinations and types
  2mrs.tsv         2MASS Redshift Survey (Huchra et al. 2012): ~44,000 galaxies to ~300 Mpc, with
                   redshifts, sizes, axis ratios and types
  NGC.csv          OpenNGC (Verga et al.): NGC and IC objects, for galaxies' position angles and
                   the common names of galaxies and nebulae (github.com/mattiaverga/OpenNGC)
  pne.tsv          Planetary nebulae with Gaia EDR3 distances to their central stars (Chornay &
                   Walton 2021), and their sizes from HASH
  hii.tsv          WISE catalogue of Galactic H II regions (Anderson et al. 2014), with distances
  snr.tsv          Galactic supernova remnants with distances (Ranasinghe & Leahy 2023)
  clusters.tsv     Open clusters with Gaia DR3 distances (Hunt & Reffert 2023), for the distances
                   of the nebulae lit by them that the other catalogues don't measure (such as the
                   Lagoon and Eagle nebulae)

VizieR tables come from the first of several mirrors that answers. Needs only Python 3.
"""
import io
import os
import sys
import tarfile
import urllib.parse
import urllib.request

VIZIER_MIRRORS = [
    'https://vizier.cfa.harvard.edu/viz-bin/asu-tsv',
    'https://vizier.cds.unistra.fr/viz-bin/asu-tsv',
    'http://vizier.nao.ac.jp/viz-bin/asu-tsv',
]

HYG_URL = 'https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/CURRENT/hygdata_v41.csv'
PSRCAT_URL = 'https://www.atnf.csiro.au/research/pulsar/psrcat/downloads/psrcat_pkg.tar.gz'
OPENNGC_URL = 'https://raw.githubusercontent.com/mattiaverga/OpenNGC/master/database_files/NGC.csv'

# Output file: (VizieR table, columns). Each table also gets J2000 positions in decimal degrees
# computed by VizieR (_RAJ2000, _DEJ2000), whatever frame it was published in.
VIZIER_TABLES = {
    'ungc.tsv': ('J/AJ/145/101/catalog', 'Name,a26,b/a,Bmag,TT,Mcl,Dist,A26,i,BMag'),
    '2mrs.tsv': ('J/ApJS/199/26/table3', 'ID,Ktmag,Riso,b/a,type,cz,CAT'),
    'pne.tsv': ('J/A+A/656/A110/tablea1', 'PNG,Name,pnRAdeg,pnDEdeg,pnRad,pnStat,rcomb,Rel'),
    'hii.tsv': ('J/ApJS/212/1/wisecat', 'WISE,Cl,Rad,HIIName,Dist'),
    'snr.tsv': ('J/ApJS/265/53/table1', 'SNR,Size1,Size2,Type,X,Age'),
    'clusters.tsv': ('J/A+A/673/A114/clusters', 'Name,Type,rt,dist50'),
    'hmxb.tsv': ('J/A+A/671/A149/tablea', 'ID,Class,Mx,PerSpin,Dist'),
    'lmxb.tsv': ('J/A+A/675/A199/lmxbcat', 'Name,AName,XrayType,Ppulse,MeanDist'),
}

TIMEOUT = 120  # Seconds


def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'SpaceVisualiser-catalogues/1.0'})
    with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
        return response.read()


def fetch_vizier(table, columns):
    query = urllib.parse.urlencode({
        '-source': table,
        '-out': columns,
        '-out.add': '_RAJ,_DEJ',
        '-oc.form': 'd',
        '-out.max': 'unlimited',
    })
    errors = []
    for mirror in VIZIER_MIRRORS:
        try:
            data = fetch(f'{mirror}?{query}')
            text = data.decode('utf-8', 'replace')
            if '#INFO\tError' in text or '_RAJ2000' not in text:
                raise RuntimeError('no table in the response')
            return data
        except Exception as error:  # Try the next mirror
            errors.append(f'{mirror}: {error}')
    raise RuntimeError(f'{table} could not be downloaded:\n  ' + '\n  '.join(errors))


def save(directory, name, data):
    path = os.path.join(directory, name)
    with open(path, 'wb') as file:
        file.write(data)
    rows = data.count(b'\n')
    print(f'  {name}: {len(data) / 1e6:.1f} MB, {rows} lines')


def main():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    directory = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'Data Files', 'Catalogues')
    os.makedirs(directory, exist_ok=True)
    print(f'Downloading catalogues to "{directory}"')

    print('HYG stars')
    save(directory, 'hygdata_v41.csv', fetch(HYG_URL))

    print('ATNF pulsars')
    with tarfile.open(fileobj=io.BytesIO(fetch(PSRCAT_URL)), mode='r:gz') as archive:
        member = next(m for m in archive.getmembers() if m.name.endswith('/psrcat.db'))
        save(directory, 'psrcat.db', archive.extractfile(member).read())

    print('OpenNGC')
    save(directory, 'NGC.csv', fetch(OPENNGC_URL))

    for name, (table, columns) in VIZIER_TABLES.items():
        print(f'VizieR {table}')
        save(directory, name, fetch_vizier(table, columns))
    print('Done')


if __name__ == '__main__':
    main()
