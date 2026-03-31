# real-time-monitor
A monitor (GUI + probe) to inspect the real-time behavior of your code

A few dependencies are required for building:
- the GUI
- the units test

To do so, you can either install them by calling conan directly or by using the helper script setup_build.sh

## Dependency Management

This project uses [Conan](https://conan.io/) to manage its dependencies.

### Requirements

- [Conan](https://conan.io/downloads.html) (>= 2.10)
- CMake (>= 3.28)
- C++17 compatible compiler:
  - **Linux**: GCC >= 13 (recommended)
  - **macOS**: Apple Clang (comes with Xcode Command Line Tools)

### Installing Dependencies

To initialize the build directory, use the provided `setup_build.sh` script. This script sets up all Conan dependencies **inside the `build` directory**. It also creates a `toolchain.cmake` file in the `build` directory that shall be used with cmake.
The `build` directory is created if needed.

Usage:
```bash
./setup_build.sh build
```

**System Dependencies:**

The GUI is based on [GLFW](https://www.glfw.org/), which requires platform-specific dependencies:

- **Linux**: The following system libraries are required:
  ```bash
  sudo apt-get install libwayland-dev libxkbcommon-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
  ```
  (On Debian/Ubuntu systems)

- **MacOS**: The required frameworks (Metal, MetalKit, QuartzCore) are part of the macOS SDK and require Xcode Command Line Tools to be installed:
  ```bash
  xcode-select --install
  ```

### Build

1. Setup dependencies (if not already done):
   ```bash
   ./setup_build.sh build
   ```

2. Configure and build:
   ```bash
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

   Or using the traditional approach:
   ```bash
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   make
   ```


### Build Python Bindings

The project uses [scikit-build-core](https://scikit-build-core.readthedocs.io/) for Python bindings. For an efficient build process (to avoid rebuilding from scratch every time), run:

```bash
# First, setup the build directory
./setup_build.sh /tmp/build

# Then install with uv
uv pip install --no-build-isolation -Cbuild-dir=/tmp/build -v .
```

**Note**: This requires `uv` to be installed. The build process will automatically handle Conan dependencies through the setup script.
