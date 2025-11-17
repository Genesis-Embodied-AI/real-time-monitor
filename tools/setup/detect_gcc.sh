#!/bin/bash

echo "Scanning for GCC installations..."

GREATEST_VERSION=""
GREATEST_CC=""
GREATEST_CXX=""

# Find all gcc executables in PATH that match gcc or gcc-[version]
GCC_LIST=$(compgen -c | grep -E '^gcc-[0-9]+$' | sort -ru)

if [ -z "$GCC_LIST" ]; then
    # Try to fallback to default GCC
    GCC_LIST=$(compgen -c | grep -E '^gcc$')
fi

if [ -z "$GCC_LIST" ]; then
    echo "!!! No GCC installations found !!!"
    exit 1
fi

for gcc_bin in $GCC_LIST; do
    # Check if it's an actual executable
    if command -v "$gcc_bin" &> /dev/null; then
        version=$("$gcc_bin" -dumpfullversion -dumpversion 2>/dev/null)
        if [ -z "$version" ]; then
            version=$("$gcc_bin" --version | head -n1 | awk '{print $3}')
        fi
        echo " * Found: $gcc_bin ($version)"

        # Capture the first (highest) one and stop
        if [ -z "$GREATEST_VERSION" ]; then
            GREATEST_VERSION=$version
            GREATEST_CC=$gcc_bin
        fi
    fi
done

# remove minor and patch (x.y.z -> x)
GREATEST_VERSION=${GREATEST_VERSION%%.*}

# create g++ binary from gcc binary (we expect it to be installed)
GREATEST_CXX="${GREATEST_CC//gcc/g++}"

echo
echo "--> Greatest GCC version detected: $GREATEST_VERSION ($GREATEST_CC/$GREATEST_CXX)"
echo ""
