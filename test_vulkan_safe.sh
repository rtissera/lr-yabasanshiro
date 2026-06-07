#!/bin/bash
# Safe test script - won't crash terminal
CORE="$HOME/yaba2026/libretro/yabasanshiro_libretro.so"
CONFIG="/tmp/test_retroarch.cfg"
ROM="$HOME/Bureau/REGLINUX/roms/saturn/SS-parodius-sexy.chd"

cat > "$CONFIG" << 'EOF'
video_driver = "vulkan"
system_directory = "/home/romain/Bureau/REGLINUX/bios"
EOF

mkdir -p "$HOME/.config/retroarch/config/YabaSanshiro"
echo 'yabasanshiro_video_core = "vulkan"' > "$HOME/.config/retroarch/config/YabaSanshiro/YabaSanshiro.opt"

cp "$CORE" "$HOME/.config/retroarch/cores/"

echo "=== Running RetroArch (no timeout — Ctrl+C to quit) ==="
retroarch --config="$CONFIG" \
  -L "$HOME/.config/retroarch/cores/yabasanshiro_libretro.so" \
  "$ROM" --verbose 2>&1 | tee /tmp/retroarch_test.log

echo ""
echo "=== Core check ==="
if grep -q "SET_HW_RENDER.*vulkan" /tmp/retroarch_test.log 2>/dev/null; then
  echo "PASS: Vulkan context requested"
else
  echo "FAIL: No Vulkan context"
fi
if grep -q "erros: 0" /tmp/retroarch_test.log 2>/dev/null; then
  echo "PASS: Shaders compiled OK"
else
  echo "INFO: No shader compile output seen"
fi
if grep -i "\[ERROR\]" /tmp/retroarch_test.log 2>/dev/null | grep -v Wayland | grep -q .; then
  echo "WARN: Non-Wayland errors found"
else
  echo "PASS: No unexpected errors"
fi
