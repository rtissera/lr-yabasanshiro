#include "gtest/gtest.h"
#include <core.h>
#include "yabause.h"
extern yabsys_struct yabsys;
int main(int argc, char** argv) {
  (void)argc; (void)argv;
  // Match the real libretro build: use_sh2_cache=1 skips the x86_64
  // overrideMemFunc path (which truncates 64-bit pointers). Set before any
  // DynarecSh2 fixture is constructed.
  yabsys.use_sh2_cache = 1;
  return RUN_ALL_TESTS();
}
