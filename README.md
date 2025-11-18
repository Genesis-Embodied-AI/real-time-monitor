# real-time-monitor
A monitor (GUI + probe) to inspect the real-time behavior of your code

A few dependencies are required for building:
- the monitor
- the units test

To do so, you can either install them by calling conan directly or by using the helper script setup_build.sh

## Dependency Management

This project uses [Conan](https://conan.io/) to manage its dependencies.

### Requirements

- [Conan](https://conan.io/downloads.html) (>= 2.10)
- CMake (>= 3.28)
- GCC C/C++ compiler >= 13 (clang is not officially supported but it should works)

### Installing Dependencies

To initialize the build directory, use the provided `setup_build.sh` script. This script sets up all Conan dependencies **inside the `build` directory**. It also creates a `toolchain.cmake` file in the `build` that shall be used with cmake.
`build`directory is created if needed.

For the monitor, a few system dependencies are also required:
- glfw
You have to install them with your distribution package manager.
For debian-like:
```
sudo apt install libglfw3-dev
```

### Build

1. Create and enter the build directory:

   ```bash
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   make
   ```


### Build python bindings

For efficient build process (to avoid rebuilding from scratch every time), run

```
uv pip install --no-build-isolation -Cbuild-dir=/tmp/build -v .
```
