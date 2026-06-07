#!/bin/bash
# Build the devmiyax dynarec opcode unit-test battery standalone (no gtest dep).
# Produces ./optest. Same compiler defines as the real libretro build so codegen matches.
set -e
cd "$(dirname "$0")"
HERE=$(pwd)
DRC=$(cd .. && pwd)            # sh2_dynarec_devmiyax
SRC=$(cd ../.. && pwd)        # src
ROOT=$(cd ../../.. && pwd)    # repo root

ARCH=$(uname -m)
case "$ARCH" in
  x86_64)  ARCHDEF=-DX86_64;  ASM_OBJ=$DRC/dynalib_x86_64.asm.o ;;
  aarch64) ARCHDEF=-DAARCH64; ASM_OBJ=$DRC/dynalib_arm64.s.o ;;
  *) echo "unsupported arch $ARCH"; exit 1 ;;
esac

DEFS="-D__LIBRETRO__ -DDYNAREC_DEVMIYAX=1 -DARCH_IS_LINUX=1 $ARCHDEF -DGTEST -DCACHE_ENABLE=1 \
  -DHAVE_BUILTIN_BSWAP16=1 -DHAVE_BUILTIN_BSWAP32=1 -DHAVE_C99_VARIADIC_MACROS=1 \
  -DHAVE_STDINT_H=1 -DNO_CLI"
INC="-I$HERE -I$DRC -I$SRC -I$ROOT/stub_include -I$SRC/libretro \
  -I$SRC/libretro/libretro-common/include"
CXXFLAGS="-O0 -g -std=gnu++11 -fPIC $DEFS $INC -w"
CFLAGS="-O0 -g -std=gnu99 -fPIC $DEFS $INC -w"

# Assemble the native template library (fresh, matching current asm source).
if [ "$ARCH" = x86_64 ]; then
  nasm -f elf64 -o $DRC/dynalib_x86_64.asm.o $DRC/dynalib_x86_64.asm
else
  gcc $CFLAGS -c $DRC/dynalib_arm64.s -o $DRC/dynalib_arm64.s.o
fi

rm -rf obj && mkdir -p obj && cd obj

g++ $CXXFLAGS -c $HERE/optest_main.cpp -o optest_main.o
for f in $DRC/test/*.cpp; do g++ $CXXFLAGS -c "$f" -o "t_$(basename $f .cpp).o"; done
g++ $CXXFLAGS -c $DRC/memory_for_test.cpp      -o memory_for_test.o
g++ $CXXFLAGS -c $DRC/DynarecSh2.cpp           -o DynarecSh2.o
g++ $CXXFLAGS -c $DRC/DynarecSh2CInterface.cpp -o DynarecSh2CInterface.o
gcc $CFLAGS   -c $DRC/dmy.c                     -o dmy.o
gcc $CFLAGS   -c $SRC/sh2d.c                    -o sh2d.o

g++ -o $HERE/optest *.o "$ASM_OBJ" -lpthread
cd "$HERE"
echo "built ./optest for $ARCH"
