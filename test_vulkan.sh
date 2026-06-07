#!/bin/bash
# Script to test the libretro Vulkan core with RetroArch
set -e

CORE="$HOME/yaba2026/libretro/yabasanshiro_libretro.so"
CONFIG="/tmp/test_retroarch.cfg"
BIOS_DIR="$HOME/Bureau/REGLINUX/bios"
ROM="$HOME/Bureau/REGLINUX/roms/saturn/SS-parodius-sexy.chd"

# Create config
cat > "$CONFIG" << 'EOF'
video_driver = "vulkan"
system_directory = "/home/romain/Bureau/REGLINUX/bios"
EOF

# Ensure core option for Vulkan
mkdir -p "$HOME/.config/retroarch/config/YabaSanshiro"
echo 'yabasanshiro_video_core = "vulkan"' > "$HOME/.config/retroarch/config/YabaSanshiro/YabaSanshiro.opt"

# Install latest core
cp "$CORE" "$HOME/.config/retroarch/cores/"

echo "=== Running RetroArch with Vulkan core ==="
gdb --args retroarch --config="$CONFIG" \
  -L "$HOME/.config/retroarch/cores/yabasanshiro_libretro.so" \
  "$ROM" --verbose 2>&1
