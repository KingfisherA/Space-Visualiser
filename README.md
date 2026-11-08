<p align="center">
  <img src="Icons/SpaceVisualiser%20Icon.svg" alt="Space Visualiser" width="200">
</p>

<h1 align="center">Space Visualiser</h1>

<p align="center">A real-time, to-scale model of the Solar System, rendered with the Dahlia graphics engine.</p>

---

Space Visualiser shows the Sun and the eight planets at their real sizes and distances. Each planet
starts at its current position, computed from JPL's Keplerian orbital elements, and then moves
under simulated N-body gravity. It renders:

- Planetary atmospheres with precomputed atmospheric scattering
  ([Bruneton](https://github.com/ebruneton/precomputed_atmospheric_scattering)), tuned for each planet
- Ring systems with shadows and forward scattering: Saturn's main rings from Cassini imagery, plus
  the faint dusty rings of Jupiter, Uranus and Neptune
- The Sun's photosphere in visible light (limb darkening and reddening from its temperature,
  granulation, sunspots and faculae), or its chromosphere in extreme ultraviolet, and its corona,
  with a lens flare when it is far away
- Earth's specular oceans, clouds and night lights

The interface lets you fly to any body, change the time scale, exposure and render style, and
toggle the ecliptic grid and labels. A splash screen shows loading progress while the atmospheres
are precomputed and the textures load.

Rendering is done by [Dahlia](https://github.com/KingfisherA/GraphicsEngine), which provides Metal,
Vulkan and OpenGL 4.1 backends. Metal is the default on macOS, and OpenGL 4.1 elsewhere.

## Requirements

- CMake 3.24 or newer, and Ninja
- A C++20 compiler (Clang/AppleClang, GCC or MSVC)
- The Dahlia graphics engine source, checked out next to this repository (`../Dahlia`) with its
  submodules
- Dahlia's dependencies: `glslc` (Vulkan SDK or shaderc), Boost (filesystem), GLFW 3, GLM, GLEW
  and FreeType. Optional: the Vulkan SDK and metal-cpp. See Dahlia's README for details.

On macOS with Homebrew:

```sh
brew install cmake ninja shaderc boost glfw glm glew freetype vulkan-loader
```

## Getting the source

```sh
git clone https://github.com/KingfisherA/Space-Visualiser.git
git clone --recursive https://github.com/KingfisherA/GraphicsEngine.git Dahlia
```

To use a Dahlia checkout somewhere else, configure with `-DSV_DAHLIA_DIR=/path/to/Dahlia`.

### Planet textures

The textures are not in the repository. Space Visualiser loads them from `Data Files/Textures`
by default. You can choose another folder with `-DSV_DATA_DIR=...` when configuring, or with
`--data <dir>` when running. It expects the following files:

```
Solar System/Mercury/Mercury 1536x768.jpg
Solar System/Venus/Venus Clouds 1536x768.jpg
Solar System/Earth/NASA/1536x768/Earth.jpg
Solar System/Earth/NASA/1536x768/Earth Specular Map.jpg
Solar System/Earth/NASA/1536x768/Earth Clouds.jpg
Solar System/Earth/NASA/1536x768/Earth Night.jpg
Solar System/Mars/Mars 1536x768.jpg
Solar System/Jupiter/Jupiter Cassini 1536x768.jpg
Solar System/Saturn/Saturn Cassini ISS 1536x768.png
Solar System/Saturn/Saturn Rings 8192x4.png
Solar System/Uranus/Uranus 1536x768.jpg
Solar System/Neptune/Neptune 1024x512.jpg
Solar System/Earth/Moons/Moon 1536x768.jpg
Solar System/Mars/Moons/{Phobos,Deimos} 1024x512.jpg
Solar System/Jupiter/Moons/Io 1440x720.jpg
Solar System/Jupiter/Moons/{Europa,Ganymede,Callisto} 1536x768.jpg
Solar System/Saturn/Moons/{Mimas,Enceladus,Tethys,Dione,Rhea,Iapetus} 1024x512.jpg
Solar System/Saturn/Moons/Titan 1536x768.jpg
Solar System/Uranus/Moons/{Miranda,Ariel,Umbriel,Titania,Oberon} 1024x512.jpg
Solar System/Neptune/Moons/Triton 1536x768.jpg
```

If a texture is missing, that surface is drawn black. `Tools/Textures/` has the Python scripts
used to prepare the textures: `downscale_texture.py` fits a texture to the 5 MB decoded texture
budget, `make_saturn_map.py` builds the Saturn map from Cassini data, and `make_moon_maps.py`
downloads and builds the moon maps from NASA's 3D Resources and the USGS Astrogeology global
mosaics made for NASA's Planetary Data System (about 1.5 GB of downloads):

```sh
python3 Tools/Textures/make_moon_maps.py <download cache> "Data Files/Textures"
```

The other 435 moons have only been seen as points of light, and are drawn in plain colours.

### Star, galaxy, nebula and pulsar catalogues

Zooming out of the solar system shows the universe around it, built from published catalogues:

- **Stars:** about 110,000 stars from the HYG database. They come into view as you leave the solar
  system.
- **The Milky Way:** the galaxy model, placed and turned to match the real galaxy, with
  Sagittarius A\* at its centre.
- **Pulsars, magnetars and neutron stars:** the ATNF Pulsar Catalogue.
- **X-ray binaries:** neutron stars and stellar black holes feeding on a companion star.
- **Nebulae:** planetary nebulae, H II regions and supernova remnants, each at its measured
  distance.
- **Other galaxies:** about 44,000 galaxies, from the Local Group (Andromeda, Triangulum and the
  Magellanic Clouds) out to about 300 Mpc.

Close up, the galaxy, nebula, black hole and pulsar renderers of the test scenes take over:

- Andromeda, Triangulum, the Whirlpool, the Pinwheel, the Sombrero, NGC 1300, the Rose Galaxy, the
  Fireworks, Sunflower, Spindle, Black Eye, Bode's, Needle, Sculptor, Cigar (with its H-alpha
  wind), Cartwheel, Fried Egg and Barnard's galaxies, Fornax A and the Eye of Sauron (NGC 4151,
  with its active nucleus);
- the Crab, Orion, Eagle, Bubble, Blue Flash, Box, Veil, North America, Carina, Rim, Rosette,
  Flaming Star and Lagoon nebulae;
- any black hole or neutron star you go to.

The catalogues aren't in the repository. Space Visualiser loads them from `Data Files/Catalogues`
by default (choose another folder with `-DSV_CATALOGUE_DIR=...` or `--catalogues <dir>`).
Download them with:

```sh
python3 Tools/Catalogues/download_catalogues.py
```

This needs only Python 3 and downloads about 60 MB. It saves each file as published (VizieR
tables with only the columns used), and the app parses them when it starts. The sources are:

| File              | Catalogue                                                                                   |
|-------------------|---------------------------------------------------------------------------------------------|
| `hygdata_v41.csv` | HYG database v4.1 ([astronexus](https://github.com/astronexus/HYG-Database))                |
| `psrcat.db`       | ATNF Pulsar Catalogue (Manchester et al. 2005)                                              |
| `hmxb.tsv`        | Galactic high-mass X-ray binaries (Fortin et al. 2023)                                      |
| `lmxb.tsv`        | Galactic low-mass X-ray binaries (Avakyan et al. 2023)                                      |
| `ungc.tsv`        | Updated Nearby Galaxy Catalog (Karachentsev et al. 2013)                                    |
| `2mrs.tsv`        | 2MASS Redshift Survey (Huchra et al. 2012), at distances from the redshifts                 |
| `NGC.csv`         | OpenNGC ([mattiaverga](https://github.com/mattiaverga/OpenNGC)): common names, position angles |
| `pne.tsv`         | Planetary nebulae with Gaia distances (Chornay & Walton 2021)                               |
| `hii.tsv`         | WISE catalogue of Galactic H II regions (Anderson et al. 2014)                              |
| `snr.tsv`         | Galactic supernova remnants with distances (Ranasinghe & Leahy 2023)                        |
| `clusters.tsv`    | Open clusters with Gaia distances (Hunt & Reffert 2023): distances for nebulae they light   |

Without the catalogues, the solar system is drawn on a black sky as before.

### Moon orbits

`src/SV/Universe/MoonData.cpp` holds the orbits and sizes of all 455 known moons of the planets.
It is generated by `Tools/Moons/make_moon_data.py` (needs numpy and scipy) from JPL's satellite
mean elements, fitted to JPL Horizons positions from 2017 to 2035. Earth's Moon uses a lunar
theory (Meeus' abridged ELP-2000/82) instead.

## Building

The build is always configured in `./build`:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # or Release
cmake --build build
```

In VS Code, the CMake Tools extension uses `./build` automatically (see `.vscode/settings.json`).
`cmake.sh` is a wrapper around `cmake` that turns on ccache when it is installed.

Outputs:

| Path                                | Contents                                |
|-------------------------------------|-----------------------------------------|
| `build/SV/SV`                       | Space Visualiser executable             |
| `build/SV/Space Visualiser.app`     | macOS app bundle (see below)            |
| `build/Dahlia/`                     | `libDahlia` (shared), `libDahliaTools`  |

### macOS app bundle

```sh
cmake --build build --target SVApp
open "build/SV/Space Visualiser.app"
```

This builds `Space Visualiser.app` with the Space Visualiser icon, a copy of `libDahlia.dylib` in
`Contents/Frameworks`, and an ad-hoc signature. Third-party libraries (GLFW, Boost, etc.) are still
loaded from their installed locations. The textures are loaded from `SV_DATA_DIR`, as for the
plain executable.

### Icons

Two files in `Icons/` are generated from `Icons/SpaceVisualiser Icon.svg`:
`SpaceVisualiser.icns` (the app bundle icon) and `SpaceVisualiser Icon.png` (the splash screen
image, embedded in the executable). If you edit the SVG, run `Scripts/MakeIcons.sh` (needs
Inkscape) to regenerate them.

## Running

```sh
./build/SV/SV                 # Metal on macOS, OpenGL 4.1 elsewhere
./build/SV/SV --opengl        # OpenGL 4.1
./build/SV/SV --vulkan        # Vulkan 1.3
./build/SV/SV --data <dir>    # Load the textures from <dir>
./build/SV/SV --catalogues <dir>  # Load the catalogues from <dir>
```

The window layout is saved to `imgui.ini` in a per-user settings folder:
`~/Library/Application Support/Space Visualiser` on macOS, `%APPDATA%\Space Visualiser` on Windows,
or `~/.config/space-visualiser` elsewhere. Delete it to restore the default layout.

### Controls

| Input                                   | Action                                   |
|-----------------------------------------|------------------------------------------|
| Drag / two-finger scroll                | Orbit                                    |
| Right drag, Option + drag / Shift + scroll | Pan                                   |
| Mouse wheel, Shift + drag / pinch, Cmd + scroll | Zoom                             |
| W / A / S / D                           | Move (slowing as you near a surface)     |
| `-` / `=`                               | Decrease / increase exposure             |
| 1 / 2 / 3                               | Solid / wireframe / points               |
| G / L                                   | Toggle the grid / labels                 |

Use **Go to** in the Bodies panel to fly to a body and follow it. Panning or moving stops
following it.

To go somewhere in the universe:
- search by name in the Universe panel (stars, nebulae, galaxies, pulsars and black holes);
- or use its shortcuts;
- or click a label in the viewport, or the sky near an object. Hovering over the sky rings and names
  the object a click would select, including pulsars, black holes and small nebulae too faint or
  small to see from the solar system.

Close up, a black hole or neutron star is shown by the black hole or pulsar renderer.

## Repository layout

```
CMakeLists.txt          Top level: build type, options, Dahlia subproject
cmake/                  CMake helpers (file embedding, app bundle Info.plist)
src/SV/                 Space Visualiser sources and CMakeLists.txt
Shaders/                Shaders (Vulkan GLSL, compiled to SPIR-V and embedded at build time)
Icons/                  Space Visualiser icon (SVG source, generated .icns and .png)
Scripts/                Helper scripts (icon generation)
Tools/Textures/         Texture preparation scripts
Tools/Catalogues/       Catalogue download script
```
