#!/bin/bash
# Regenerates the icons rendered from Icons/SpaceVisualiser Icon.svg:
#   Icons/SpaceVisualiser.icns       macOS application bundle icon (macOS only: needs iconutil)
#   Icons/SpaceVisualiser Icon.png   512x512, embedded in the executable for the splash screen
# Needs Inkscape (/Applications/Inkscape.app or on PATH).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SVG="$ROOT/Icons/SpaceVisualiser Icon.svg"
ICNS="$ROOT/Icons/SpaceVisualiser.icns"
PNG="$ROOT/Icons/SpaceVisualiser Icon.png"

if command -v inkscape >/dev/null 2>&1; then
    INKSCAPE=inkscape
elif [ -x /Applications/Inkscape.app/Contents/MacOS/inkscape ]; then
    INKSCAPE=/Applications/Inkscape.app/Contents/MacOS/inkscape
else
    echo "Install Inkscape to render the icons" >&2
    exit 1
fi

render() { # <size> <output.png>
    "$INKSCAPE" "$SVG" --export-type=png --export-background-opacity=0 \
        --export-width="$1" --export-height="$1" --export-filename="$2" 2>/dev/null
}

render 512 "$PNG"
echo "Wrote $PNG"

ICONSET="$(mktemp -d)/SpaceVisualiser.iconset"
mkdir -p "$ICONSET"
trap 'rm -rf "$(dirname "$ICONSET")"' EXIT

for SIZE in 16 32 128 256 512; do
    render "$SIZE" "$ICONSET/icon_${SIZE}x${SIZE}.png"
    render "$((SIZE * 2))" "$ICONSET/icon_${SIZE}x${SIZE}@2x.png"
done

iconutil --convert icns --output "$ICNS" "$ICONSET"
echo "Wrote $ICNS"
