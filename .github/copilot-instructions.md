# GitHub Copilot Instructions — Orthorectify

## Project Purpose

**Orthorectify** is a C++17 command-line tool that reprojects drone images onto a ground plane using an ODM reconstruction (camera model + DEM + shot positions). Output is a set of GeoTIFF images aligned to real-world coordinates.

## Build System

- **CMake 3.21+** with **vcpkg manifest mode** for dependency management.
- vcpkg is a **git submodule** at `vcpkg/`. Always initialise it: `git submodule update --init --recursive`.
- Presets are defined in `CMakePresets.json`. Use these to configure and build:

  ```bash
  cmake --preset windows-release   # or linux-release / *-debug
  cmake --build --preset windows-release
  ```

- Binary output: `build/<preset>/Release/Orthorectify[.exe]`
- Adding a new vcpkg dependency: add it to `vcpkg.json` and `find_package` + `target_link_libraries` in `CMakeLists.txt`.

## Dependency Map

| Library | vcpkg name | CMake target | Role |
|---------|-----------|--------------|------|
| GDAL 3.12 | `gdal` | `GDAL::GDAL` | Raster I/O, GeoTIFF read/write, coordinate systems |
| Eigen 3.4 | `eigen3` | `Eigen3::Eigen` | Linear algebra (rotations, projections) |
| nlohmann/json | `nlohmann-json` | `nlohmann_json::nlohmann_json` | Parse ODM `shots.geojson` and camera models |
| cxxopts | `cxxopts` | `cxxopts::cxxopts` | CLI argument parsing |
| plog | `plog` | `plog::plog` | Structured console logging |
| OpenMP | system | `OpenMP::OpenMP_CXX` | Multi-threaded pixel processing |

## Source Layout

```
src/
├── main.cpp          — entry point; wires CLI → dataset → processing pipeline
├── parameters.hpp    — cxxopts-based CLI parser; produces a Parameters struct
├── dataset.hpp       — loads ODM shots (camera poses) and camera intrinsics from JSON
├── rawimage.hpp/cpp  — GDAL wrapper: open, read, write raster bands; handles nodata
├── processing.hpp    — outer loop over images; calls transform + sampling per pixel
├── transform.hpp     — Eigen math: world↔pixel coordinate transforms, DEM lookup
├── types.hpp         — type aliases (Vec3, Mat3, …) used project-wide
├── utils.hpp/cpp     — small helpers (path manipulation, string ops)
└── customformatter.h — plog log formatter
```

`version.h` is auto-generated from `version.h.in` by CMake into the build directory; include it as `#include "version.h"`.

## Coding Conventions

- **Standard**: C++17. Use `std::filesystem` (`fs::path`) for all path operations.
- **Namespace**: `namespace orthorectify` wraps all non-`main` code.
- **Headers only** for templated/inline code; `.cpp` files for non-trivial implementations.
- **Logging macros**: `INF`, `DBG`, `WRN`, `ERR` (from plog, defined in the project). Do not use `std::cout` for output.
- **GDAL objects**: always call `GDALAllRegister()` once at startup (done in `main.cpp`). Use RAII wrappers from `rawimage.hpp` — do not manage raw `GDALDataset*` pointers directly in new code.
- **Eigen types**: prefer `Eigen::Vector3d`, `Eigen::Matrix3d`, `Eigen::Affine3d`. Use `auto` sparingly — prefer named types for function signatures.
- **Error handling**: throw `std::runtime_error` (or a descriptive subclass) on unrecoverable conditions. plog + stack-unwinding will surface the message.
- **OpenMP**: parallelise at the image-row level inside `processing.hpp`. Guard with `#ifdef _OPENMP`.

## Common Tasks

### Add a new CLI option
1. Add field to `Parameters` struct in `src/parameters.hpp`.
2. Register the option with `options.add_options()(...)` in the `Parameters` constructor.
3. Parse and assign inside the same constructor after `options.parse(argc, argv)`.

### Add support for a new raster format
Handled automatically by GDAL — no code changes needed as long as the vcpkg `gdal` feature set includes the required driver. Check `vcpkg.json` `features` if a driver is missing.

### Extend the camera model
Camera intrinsics are parsed in `dataset.hpp` from ODM JSON. Add new fields to the relevant struct and update the JSON deserialization (`from_json`) function there.

### Run a quick smoke test
```bash
# Windows
.\build\windows-release\Release\Orthorectify.exe --help

# Linux
./build/linux-release/Orthorectify --help
```

## What NOT to do

- Do **not** hardcode library paths — all dependencies come through vcpkg and CMake targets.
- Do **not** include headers with relative `../vendor/` paths — the `vendor/` directory has been removed. Use the installed package headers (`<gdal_priv.h>`, `<nlohmann/json.hpp>`, `<Eigen/Core>`, etc.).
- Do **not** use `printf` or `std::cout` for user-visible messages — use plog macros.
- Do **not** modify files inside `vcpkg/` — it is a pinned submodule; update the submodule ref instead.
