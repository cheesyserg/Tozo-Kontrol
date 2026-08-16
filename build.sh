#!/usr/bin/env bash
set -e

# Detect source file
SRC=""
for file in "${1}" main.cpp tozo_kontrol.cpp; do
    if [[ -f "$file" ]]; then
        SRC="$file"
        break
    fi
done

if [[ -z "$SRC" ]]; then
    echo "Error: No source file found (.cpp)."
    exit 1
fi

TARGET="tozo_kontrol"
MOC_BIN="/usr/lib/qt6/moc"

# Check dependencies
if [[ ! -x "$MOC_BIN" ]]; then
    MOC_BIN="$(command -v moc 2>/dev/null || true)"
fi

if [[ -z "$MOC_BIN" ]]; then
    echo "Error: Qt6 moc not found. Install dependencies with:"
    echo "  sudo pacman -S --needed base-devel gcc pkgconf qt6-base bluez-libs"
    exit 1
fi

echo "==> Source: $SRC"
echo "==> Running MOC..."
"$MOC_BIN" "$SRC" -o main.moc

echo "==> Compiling $TARGET..."
g++ -fPIC -O2 "$SRC" -o "$TARGET" $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread

echo "==> Build complete: ./$TARGET"
