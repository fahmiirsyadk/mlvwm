#!/bin/bash
# MLVWM Installer for Void Linux
set -e

echo "=== MLVWM Installer ==="
echo ""

# Check dependencies
echo "Checking dependencies..."
MISSING=""
for pkg in gcc make imake libX11-devel libXext-devel libXpm-devel; do
    if ! xbps-query -l "$pkg" &>/dev/null; then
        MISSING="$MISSING $pkg"
    fi
done

if [ -n "$MISSING" ]; then
    echo "Missing packages:$MISSING"
    echo "Install them with: sudo xbps-install -S$MISSING"
    echo ""
fi

# Check for X11 apps
echo "Checking for recommended X11 apps..."
for app in xterm xclock xeyes xcalc; do
    if ! which "$app" &>/dev/null; then
        echo "  WARNING: $app not found (recommended)"
    fi
done
echo ""

# Build
echo "Building mlvwm..."
cd mlvwm
xmkmf
make
echo "Build successful!"
echo ""

# Install
echo "Installing..."
sudo make install
echo "Installed mlvwm to /usr/local/bin/"
echo ""

# Install desktop entry
echo "Installing lightdm session entry..."
sudo cp install/mlvwm.desktop /usr/share/xsessions/
echo "Installed mlvwm.desktop to /usr/share/xsessions/"
echo ""

# Setup mlvwmrc config (optional)
echo "=== Optional: mlvwmrc configuration ==="
echo "A better configuration is available in mlvwmrc/"
echo "To install it, run:"
echo "  cd mlvwmrc && make && make install"
echo ""

echo "=== Done ==="
echo "Select 'MLVWM' from your display manager (lightdm) to start."
echo "Or run: mlvwm"
