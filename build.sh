#!/bin/bash
# Build all MDA plugins locally.
# Usage: ./build.sh [arch]
#   arch: arm64, x86_64, or universal (default: current machine)
#
# Examples:
#   ./build.sh              # build for current architecture
#   ./build.sh arm64        # build for Apple Silicon
#   ./build.sh universal    # build Universal Binary (arm64 + x86_64)

set -e

ARCH="${1:-}"
CMAKE_EXTRA=""
PARALLEL=$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [ "$(uname)" = "Darwin" ]; then
    case "$ARCH" in
        arm64)     CMAKE_EXTRA="-DCMAKE_OSX_ARCHITECTURES=arm64" ;;
        x86_64)    CMAKE_EXTRA="-DCMAKE_OSX_ARCHITECTURES=x86_64" ;;
        universal) CMAKE_EXTRA="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" ;;
    esac
fi

echo "=== Configure ==="
cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel $CMAKE_EXTRA

echo "=== Build (parallel $PARALLEL) ==="
cmake --build build --config MinSizeRel --parallel "$PARALLEL"

echo "=== Strip ==="
STRIPPED=0
if [ "$(uname)" = "Darwin" ]; then
    find build -path "*/MinSizeRel/*" -type f \( -perm +111 -o -name "*.so" -o -name "*.dylib" \) \
        ! -name "*.ttl" ! -name "*.json" ! -name "*.plist" | while read f; do
        strip -x "$f" 2>/dev/null || true
    done
    # Re-sign after strip
    find build -path "*/MinSizeRel/*" \( -name "*.vst3" -o -name "*.component" -o -name "*.clap" -o -name "*.lv2" \) -maxdepth 5 -mindepth 2 | while read bundle; do
        codesign --force --sign - "$bundle" 2>/dev/null || true
    done
    echo "Stripped and re-signed macOS bundles"
elif [ "$(uname)" = "Linux" ]; then
    find build -path "*/MinSizeRel/*" -type f \( -executable -o -name "*.so" \) | while read f; do
        strip --strip-unneeded "$f" 2>/dev/null || true
    done
    echo "Stripped Linux binaries"
fi

echo ""
echo "=== Results ==="
for fmt in VST3 AU CLAP LV2; do
    count=$(find build -path "*/MinSizeRel/${fmt}/*" -maxdepth 5 \( -name "*.vst3" -o -name "*.component" -o -name "*.clap" -o -name "*.lv2" \) 2>/dev/null | wc -l | tr -d ' ')
    if [ "$count" -gt 0 ]; then
        sample=$(find build -path "*/MinSizeRel/${fmt}/*" -maxdepth 5 \( -name "*.vst3" -o -name "*.component" -o -name "*.clap" -o -name "*.lv2" \) | head -1)
        binary=$(find "$sample" -type f \( -perm +111 -o -name "*.so" -o -name "*.dylib" \) ! -name "*.ttl" ! -name "*.json" ! -name "*.plist" | head -1)
        if [ -n "$binary" ]; then
            size=$(ls -lh "$binary" | awk '{print $5}')
        else
            size=$(du -sh "$sample" | awk '{print $1}')
        fi
        echo "  $fmt: $count plugins (~$size each)"
    fi
done
echo ""
echo "Plugins are in build/MDA*_artefacts/MinSizeRel/"
