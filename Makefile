DEBUG = 0
DEBUG_ASAN = 0
DEBUG_UBSAN = 0
DEBUG_TSAN = 0
HAVE_SSE = 1
__USE_OPENGL_DEBUG__ = 0
HAVE_MUSASHI = 1
FORCE_GLES = 0
FASTMATH = 1
ALLOW_POLYGON_MODE = 1
LOW_END = 0
USE_RTHREADS = 1
HAVE_VULKAN = 0
FORCE_GLEW = 0

# Architecture detection
ARCH ?= $(shell uname -m)

CORE_DIR := .

ifneq ($(CROSS_COMPILE),)
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
CC_AS := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
endif

TARGET_NAME = yabasanshiro
CC_AS ?= $(CC)
GIT_VERSION := " $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
	CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

# Linux only - x86_64 / aarch64
TARGET := $(TARGET_NAME)_libretro.so
fpic := -fPIC
SHARED := -shared -Wl,--no-undefined -Wl,--version-script=link.T
FLAC_LIB := -lFLAC
ifeq ($(ARCH),aarch64)
FLAC_LIB := -l:libFLAC.so.12
endif
LDFLAGS += -lpthread -lz $(FLAC_LIB) -lstdc++fs -llzma -lzstd
ARCH_IS_LINUX = 1

ifeq ($(ARCH),x86_64)
	HAVE_SSE = 1
	USE_X86_DRC = 1
	DYNAREC_DEVMIYAX ?= 1
	FLAGS += -DX86_64
else ifeq ($(ARCH),aarch64)
	HAVE_SSE = 0
	USE_AARCH64_DRC = 1
	DYNAREC_DEVMIYAX ?= 1
	FLAGS += -DAARCH64
	ALLOW_POLYGON_MODE = 0
else
$(error Unsupported architecture: $(ARCH). Only x86_64 and aarch64 are supported.)
endif

include Makefile.common

ifeq ($(__USE_OPENGL_DEBUG__),1)
	FLAGS += -D__USE_OPENGL_DEBUG__
endif

ifeq ($(HAVE_SSE),1)
	FLAGS += -mfpmath=sse
endif

ifeq ($(HAVE_VULKAN),1)
	LDFLAGS += -lvulkan -l:libshaderc.so.1
endif

ifeq ($(DEBUG_ASAN), 1)
	DEBUG = 1
	DEBUG_UBSAN = 0
	FLAGS += -lasan -fsanitize=address
	LDFLAGS += -lasan -fsanitize=address
endif

ifeq ($(DEBUG_UBSAN), 1)
	DEBUG = 1
	FLAGS += -lubsan -fsanitize=undefined
	LDFLAGS += -lubsan -fsanitize=undefined
endif

ifeq ($(DEBUG_TSAN), 1)
	DEBUG = 1
	FLAGS += -ltsan -fsanitize=thread
	LDFLAGS += -ltsan -fsanitize=thread
endif

ifeq ($(DEBUG), 1)
	FLAGS += -O0 -g -ggdb
else
	FLAGS += -O3 -DNDEBUG
endif

ifeq ($(HAVE_MUSASHI), 1)
	FLAGS += -DHAVE_MUSASHI=1
else
	OBJECTS += $(C68KEXEC_OBJECT)
endif

ifeq ($(FORCE_GLES), 1)
	FLAGS += -D_OGLES3_ -DHAVE_OPENGLES -DHAVE_OPENGLES3
	LDFLAGS += -lGLESv2
	ifeq ($(FORCE_GLEW), 1)
		FLAGS += -DGLEW_STATIC -D_USEGLEW_ -DGLEW_NO_GLU
	else
		# glsym_private.h declares the GLES 3.1/3.2 enums + tess/compute entrypoints
		# (glMemoryBarrier/glPatchParameteri/glDispatchCompute/glBindImageTexture)
		# that the renderer references; without it the GLES build fails to compile.
		# Mirrors the desktop-GL branch below.
		FLAGS += -DHAVE_GLSYM_PRIVATE
	endif
else
	FLAGS += -D_OGL3_
	LDFLAGS += -lGL
	ifeq ($(FORCE_GLEW), 1)
		FLAGS += -DGLEW_STATIC -D_USEGLEW_ -DGLEW_NO_GLU
	else
		FLAGS += -DHAVE_GLSYM_PRIVATE
	endif
endif

ifeq ($(LOW_END), 1)
	FLAGS += -DLOW_END
endif

ifeq ($(ALLOW_POLYGON_MODE), 1)
	FLAGS += -DALLOW_POLYGON_MODE
endif

ifeq ($(ARCH_IS_LINUX), 1)
	FLAGS += -DARCH_IS_LINUX=1
endif

ifeq ($(DYNAREC_DEVMIYAX), 1)
	FLAGS += -DDYNAREC_DEVMIYAX=1
endif

ifeq ($(FASTMATH), 1)
	FLAGS += -ffast-math
endif

LDFLAGS += $(fpic) $(SHARED)
FLAGS += $(fpic)

INCFLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(dir))

FLAGS += -Istub_include $(INCFLAGS) -D__LIBRETRO__ $(ENDIANNESS_DEFINES) \
	-DNO_CLI -DHAVE_BUILTIN_BSWAP16=1 -DHAVE_BUILTIN_BSWAP32=1 -DHAVE_C99_VARIADIC_MACROS=1 \
	-DHAVE_FLOORF=1 -DHAVE_GETTIMEOFDAY=1 -DHAVE_STDINT_H=1 -DHAVE_SYS_TIME_H=1 -DIMPROVED_SAVESTATES \
	-DHAVE_LIBGL -DHAVE_STRCASECMP=1 -DHAVE_ZLIB -DHAVE_FLAC -DHAVE_7ZIP -DHAVE_DR_FLAC -DHAVE_ZSTD -DVERSION=\"2.5.1\"

CXXFLAGS += $(FLAGS) -std=gnu++11
CFLAGS += $(FLAGS) -std=gnu99

# Only define HAVE_VULKAN for files that need it (the stub interface + libretro.c)
ifeq ($(HAVE_VULKAN), 1)
src/libretro/libretro.c.o: CFLAGS += -DHAVE_VULKAN
src/libretro/vidvulkan_libretro.c.o: CFLAGS += -DHAVE_VULKAN
endif

all: $(TARGET)

generate-files:
ifeq ($(HAVE_MUSASHI), 1)
	$(CC) -o $(SOURCE_DIR)/musashi/$(M68KMAKE_EXE) $(SOURCE_DIR)/musashi/m68kmake.c;\
	$(SOURCE_DIR)/musashi/$(M68KMAKE_EXE) $(SOURCE_DIR)/musashi/ $(SOURCE_DIR)/musashi/m68k_in.c
else
	$(CC) -DC68K_GEN -o $(SOURCE_DIR)/c68k/$(GEN68K_EXE) $(SOURCE_DIR)/c68k/c68kexec.c $(SOURCE_DIR)/c68k/c68k.c $(SOURCE_DIR)/c68k/gen68k.c;\
	cd $(SOURCE_DIR)/c68k/; ./$(GEN68K_EXE)
endif

generate-files-clean:
ifeq ($(HAVE_MUSASHI), 1)
	rm -f $(M68KMAKE_INC_SOURCES) $(SOURCE_DIR)/musashi/$(M68KMAKE_EXE)
else
	rm -f $(GEN68K_INC_SOURCES) $(SOURCE_DIR)/c68k/$(GEN68K_EXE)
endif

$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	$(CXX) -o $@ $^ $(LDFLAGS)
endif

%.S.o: %.S
	$(CC_AS) $(CFLAGS) -c $^ -o $@

%.s.o: %.s
	$(CC_AS) $(CFLAGS) -c $^ -o $@

%.cpp.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS)

%.c.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.asm.o: %.asm
	nasm -f elf64 -o $@ $<

$(C68KEXEC_OBJECT): $(C68KEXEC_SOURCE)
	$(CC) -c -o $@ $< $(CFLAGS) -O0

clean:
	rm -f $(TARGET) $(OBJECTS) $(SOURCE_DIR)/musashi/$(M68KMAKE_EXE) $(SOURCE_DIR)/c68k/$(GEN68K_EXE)

.PHONY: clean
