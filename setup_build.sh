#!/bin/bash

SOURCE_DIR=$(dirname "$(realpath $0)")

# Minimum required version
MIN_CONAN_VERSION="2.10.0"

# Check if conan is installed
if ! command -v conan >/dev/null 2>&1; then
    echo "Conan is not installed. Aborting..."
    exit 1
fi

# Get installed version
INSTALLED_CONAN_VERSION=$(conan --version | awk '{print $3}')

# Compare versions
if [[ "$(printf '%s\n' "$MIN_CONAN_VERSION" "$INSTALLED_CONAN_VERSION" | sort -V | head -n1)" != "$MIN_CONAN_VERSION" ]]; then
    echo "Conan version $MIN_CONAN_VERSION or higher is required (found $INSTALLED_CONAN_VERSION)"
    exit 1
fi

build_dir=$1
if [ -z "$1" ]; then
    echo "You need to provide the build directory."
    exit 1
else
    mkdir -p $build_dir
fi
echo $build_dir

# Detect OS
OS=$(uname -s)

source "$SOURCE_DIR/tools/setup/detect_compiler.sh"

# Detect the archecture
conan profile detect --force > /dev/null 2>&1
ARCH_NAME=$(conan profile show -cx host | grep "arch=" | cut -d'=' -f2)

echo "Architecture for Conan: $ARCH_NAME"

# Set platform-specific variables
if [[ "$OS" == "Darwin" ]]; then
    OS_NAME="Macos"
    COMPILER_NAME="apple-clang"
    LIBCXX_NAME="libc++"
    SYSTEM_NAME="Darwin"
    SYSTEM_VAR="APPLE"
    ARCH_NAME="armv8\|x86_64"
else
    OS_NAME="Linux"
    COMPILER_NAME="gcc"
    LIBCXX_NAME="libstdc++11"
    SYSTEM_NAME="Linux"
    SYSTEM_VAR="LINUX"
fi

# Generate cmake toolchain profile
TEMPLATE_CMAKE_TOOLCHAIN="$SOURCE_DIR/cmake/toolchain.cmake.template"
OUTPUT_CMAKE_TOOLCHAIN="$build_dir/toolchain.cmake"
sed \
  -e "s|SYSTEM_NAME|${SYSTEM_NAME}|g" \
  -e "s|MAJOR_VERSION|${GREATEST_VERSION}|g" \
  -e "s|BINARY_PATH_CC|$(command -v $GREATEST_CC)|g" \
  -e "s|BINARY_PATH_CXX|$(command -v $GREATEST_CXX)|g" \
  "$TEMPLATE_CMAKE_TOOLCHAIN" > "$OUTPUT_CMAKE_TOOLCHAIN"

# Generate conan profile
TEMPLATE_CONAN_PROFILE="$SOURCE_DIR/conan/profile.txt.template"
OUTPUT_CONAN_PROFILE="$build_dir/profile.txt"
sed \
  -e "s|OS_NAME|${OS_NAME}|g" \
  -e "s|ARCH_NAME|${ARCH_NAME}|g" \
  -e "s|COMPILER_NAME|${COMPILER_NAME}|g" \
  -e "s|LIBCXX_NAME|${LIBCXX_NAME}|g" \
  -e "s|MAJOR_VERSION|${GREATEST_VERSION}|g" \
  -e "s|BINARY_PATH_CC|$(command -v $GREATEST_CC)|g" \
  -e "s|BINARY_PATH_CXX|$(command -v $GREATEST_CXX)|g" \
  "$TEMPLATE_CONAN_PROFILE" > "$OUTPUT_CONAN_PROFILE"

if [[ "$OS" == "Darwin" ]]; then
    echo "OSX_ARCH_VARIANTS=x86_64;arm64" >> "$OUTPUT_CONAN_PROFILE"

    echo "" >> "$OUTPUT_CONAN_PROFILE"
    echo "[platform_tool_requires]" >> "$OUTPUT_CONAN_PROFILE"
    echo "cmake/[>=4.1.1]" >> "$OUTPUT_CONAN_PROFILE"
fi
cat $OUTPUT_CONAN_PROFILE

# Prepare debug depencies only for local call, not in CI
if [[ $CIBUILDWHEEL != "1" ]]; then
    conan install $SOURCE_DIR/conan/conanfile.txt -of=$build_dir -pr $OUTPUT_CONAN_PROFILE -pr:b $OUTPUT_CONAN_PROFILE  --build=missing -s build_type=Debug
fi
conan install $SOURCE_DIR/conan/conanfile.txt -of=$build_dir -pr $OUTPUT_CONAN_PROFILE -pr:b $OUTPUT_CONAN_PROFILE  --build=missing -s build_type=Release
