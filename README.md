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
- The Sun's animated surface, chromosphere and corona, with a lens flare when it is far away
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
```

If a texture is missing, that surface is drawn black. `Tools/Textures/` has the Python scripts
used to prepare the textures: `downscale_texture.py` fits a texture to the 5 MB decoded texture
budget, and `make_saturn_map.py` builds the Saturn map from Cassini data.

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
| W / A / S / D                           | Move                                     |
| `-` / `=`                               | Decrease / increase exposure             |
| 1 / 2 / 3                               | Solid / wireframe / points               |
| G / L                                   | Toggle the grid / labels                 |

Use **Go to** in the Bodies panel to fly to a body and follow it. Panning or moving stops
following it.

## Repository layout

```
CMakeLists.txt          Top level: build type, options, Dahlia subproject
cmake/                  CMake helpers (file embedding, app bundle Info.plist)
src/SV/                 Space Visualiser sources and CMakeLists.txt
Shaders/                Shaders (Vulkan GLSL, compiled to SPIR-V and embedded at build time)
Icons/                  Space Visualiser icon (SVG source, generated .icns and .png)
Scripts/                Helper scripts (icon generation)
Tools/Textures/         Texture preparation scripts
```
