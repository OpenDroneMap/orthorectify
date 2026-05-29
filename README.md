# Orthorectification Tool

![image](https://user-images.githubusercontent.com/1951843/111536715-fc91c380-8740-11eb-844c-5b7960186391.png)

Orthorectifies individual images (or all images) from an existing [ODM](https://github.com/OpenDroneMap/ODM) reconstruction. It reads the camera model, DEM, and shot positions produced by ODM and reprojects each pixel onto the ground plane.

![image](https://user-images.githubusercontent.com/1951843/111529183-3ad6b500-8738-11eb-9960-b1aa676f863b.png)

## Prerequisites

| Tool | Minimum version |
|------|----------------|
| CMake | 3.21 |
| Git | any (for vcpkg submodule) |
| **Windows** — MSVC (Visual Studio 2022) | 14.x |
| **Linux** — GCC or Clang + Ninja | GCC 10 / Clang 12 |

All C++ dependencies (GDAL, Eigen3, nlohmann-json, cxxopts, plog) are managed automatically by **vcpkg** in [manifest mode](https://learn.microsoft.com/vcpkg/users/manifests). No manual library installation is needed.

## Building

Clone the repository with its submodules (vcpkg is included as a shallow submodule):

```bash
git clone --recurse-submodules https://github.com/yourorg/orthorectify.git
cd orthorectify
```

### Using the helper scripts (recommended)

**Windows (Release / Debug):**
```bat
build_release.bat
build_debug.bat
```

**Linux (Release / Debug):**
```bash
./build_release.sh
./build_debug.sh
```

The scripts bootstrap vcpkg on first run, configure via `cmake --preset`, and build. The binary is placed in `build/<preset>/Release/Orthorectify` (or `Debug/`).

### Manual CMake workflow

```bash
# Windows
cmake --preset windows-release
cmake --build --preset windows-release

# Linux
cmake --preset linux-release
cmake --build --preset linux-release
```

Available presets: `windows-release`, `windows-debug`, `linux-release`, `linux-debug`.

### Docker

```bash
docker build -t digipa/orthorectify .
```

> The Docker image uses system GDAL/Eigen3 from Debian Bookworm packages; vcpkg is used only for the header-only dependencies (cxxopts, plog).

## Usage

After running a reconstruction with ODM, point the tool at the dataset directory:

```bash
Orthorectify /dataset-path
```

With Docker:

```bash
docker run -it -v /dataset-path:/dataset digipa/orthorectify /dataset
```

### Options

```
Usage:
  Orthorectify [OPTION...] <dataset-path>

  -e, --dem arg               Absolute path to DEM (default: odm_dem/dsm.tif)
      --no-alpha              Omit the alpha channel from output
  -i, --interpolation arg     Pixel sampling method: nearest | bilinear (default: bilinear)
  -o, --outdir arg            Output directory (default: orthorectified)
  -l, --image-list arg        File listing image filenames to process
                              (default: img_list.txt)
      --images arg            Comma-separated filenames to process
                              (alternative to --image-list)
  -s, --skip-visibility-test  Skip visibility test — faster but may leave
                              relief-displacement artifacts
  -t, --threads arg           Thread count (-1 = all available) (default: -1)
  -v, --verbose               Enable debug logging
  -h, --help                  Print this help
```

## Project Structure

```
orthorectify/
├── src/                  # C++ source & headers
│   ├── main.cpp          # Entry point
│   ├── parameters.hpp    # CLI argument parsing (cxxopts)
│   ├── dataset.hpp       # ODM dataset loading (shots, camera models)
│   ├── rawimage.hpp/cpp  # Image I/O via GDAL
│   ├── processing.hpp    # Core orthorectification pipeline
│   ├── transform.hpp     # Coordinate transform helpers (Eigen)
│   ├── types.hpp         # Shared type aliases
│   └── utils.hpp/cpp     # Miscellaneous utilities
├── vcpkg/                # vcpkg submodule (shallow clone)
├── vcpkg.json            # vcpkg manifest — dependency list
├── CMakeLists.txt        # Root build definition
├── CMakePresets.json     # Build presets (windows/linux, debug/release)
├── version.h.in          # Version header template
└── Dockerfile            # Multi-stage container build
```

## CI

GitHub Actions builds both `linux-release` and `windows-release` on every push/PR to `main`. See [.github/workflows/build.yml](.github/workflows/build.yml).

---

## Roadmap

- [ ] Merging of multiple orthorectified images (blending, seam leveling)
- [ ] Faster visibility test
- [ ] Direct orthorectification method
- [ ] GPU support