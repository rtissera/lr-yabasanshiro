# Vulkan HW Renderer — Progress Summary

## Goal
Implement real VDP1/VDP2 GPU rendering in the libretro Vulkan core for YabaSanshiro.

## RUNTIME DEBUG SESSION (2026-06-05) — bridge to standalone VIDVulkan.cpp
The libretro Vulkan core now bridges the **standalone** `src/vulkan/VIDVulkan.cpp`
renderer (via `vidvulkan_bridge.cpp` + `vidvulkan_libretro.c` thunks), not the old
CPU-atlas `vidvulkan_libretro.c`. Tested on AMD 780M (RADV), RetroArch + CHD games
(`~/Bureau/REGLINUX/roms/saturn/`), bios `~/Bureau/REGLINUX/bios/saturn/saturn_bios.bin`
→ `~/.config/retroarch/system/saturn_bios.bin`.

### Bugs found & FIXED this session
1. **No display / no sound (root):** `VIDVulkan.cpp` had `YuiSwapBuffers()` wrapped in
   `#ifndef __LIBRETRO__` (lines ~634 and ~1287). libretro defines `__LIBRETRO__`, so the
   present path (`YuiSwapBuffers → VIDVulkanSwapBuffers → set_image`) NEVER ran. Removed the
   guard at the end of `Vdp2DrawEnd` (kept it on `renderExternal`, which is standalone-only).
2. **GPU crash "CS rejected" / GPF in libvulkan_radeon:** the renderer submitted to the
   frontend's VkQueue with no external sync (RetroArch presents on its own WSI thread).
   Added `LibretroQueueLock.h` (`VK_Q_LOCK/UNLOCK` → `lock_queue/unlock_queue` from the HW
   interface) and bracketed EVERY `vkQueueSubmit` / `vkQueue|DeviceWaitIdle` in
   VIDVulkan.cpp, Vdp1Renderer.cpp, TextureManager.cpp, VulkanScene.cpp, WindowRenderer.cpp,
   Window.cpp, and the bridge's deviceWaitIdle.
3. **Dupe-frame present crash:** `retro_run` did `video_cb(NULL,...)` on no-render frames;
   the HW-Vulkan NULL dupe crashed the frontend present. Now re-presents the last image
   (`VIDVulkanSwapBuffers()` + `video_cb(VALID)`) when `VIDVulkanIsActive()`.
4. **Only frame 0 rendered → black after boot:** core option `yabasanshiro_frameskip`
   default **enabled** = auto-frameskip skips ALL rendering after the first frame
   (`one_frame_rendered` stays 0). WORKAROUND: set option to `disabled`. The auto-frameskip
   logic itself is buggy under libretro and still needs a real fix.
5. **Crash creating a VDP2 pipeline at frame ~981 (`VdpPipeline::createGraphicsPipeline`):**
   `getRenderPass()` returns the window render pass; the pipeline factory caches it ONCE
   (`setRenderPath` at VIDVulkan.cpp:283), but `LibretroWindow::resize()` destroyed+recreated
   `_rp` → factory held a freed render pass → GPF on next pipeline create. Fixed by making
   `LibretroWindow::resize()` KEEP `_rp`/`_rp_keep` (render-pass config is size-independent),
   recreating only image/view/framebuffer.

### Current state
- Runs without crashing; all frames render; audio path active (`SNDLIBRETROUpdateAudio`).
- Verified via `vkCmdCopyImageToBuffer` readback (`YABA_PROBE=1` env, one-shot at frame 600
  in `libretro_vulkan_swap_buffers`).

### Black-output bug — FIXED (resolution mode)
Root cause: the libretro `yabasanshiro_resolution_mode` option mapped "original"→`RES_ORIGINAL(3)`
(and 2x/4x→RES_2x/RES_4x), all of which take the **upscale path** ending in
`VIDVulkan::blitSubRenderTarget()` (VIDVulkan.cpp:7084). That blit uses swapchain-era layout
transitions (`VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` for the non-swapchain `subRenderTarget`, leaves
`_out_img` in `GENERAL` while `set_image` declares `SHADER_READ_ONLY_OPTIMAL`) → produces an
all-black window image even though VDP geometry IS submitted. **`RES_NATIVE(0)` renders layers
directly into the window framebuffer (no blit) and works.** Fix: map "original"→`g_resolution_mode=0`
in `retro_set_resolution()` (the switch was clobbering check_variables). Verified by readback:
Taroumaru f=1200 → `nonblack=67766/76800 maxchan=255`.

Diagnosis method: `vkCmdCopyImageToBuffer` readback of `_out_img`, env `YABA_READBACK=1`, samples
every 300 frames from 600 in `libretro_vulkan_swap_buffers` (logs `PROBE f=N nonblack=.. maxchan=..`).
X11 `import` grab can't read the Vulkan flip surface (always black) — use the readback.

### STILL OPEN
- **Upscale modes broken:** 2x/4x (and the standalone "original"/RES_ORIGINAL) still go through the
  broken `blitSubRenderTarget` path → black. Real fix = rewrite that blit for libretro layouts
  (no PRESENT_SRC_KHR; leave `_out_img` in SHADER_READ_ONLY_OPTIMAL or match what set_image declares),
  OR composite via a render pass into the window FB like the native path.
- **SH2 dynarec — partially fixed, still crashes; use `sh2coretype=interpreter`.**
  The devmiyax JIT was never wired for Linux x86_64. Fixes applied:
  1. `DynarecSh2.cpp` had no x86_64 branch for the JIT fragment-size macros — it fell through
     to `#else // ARMv7` and used ARM sizes, so `memcpy` copied truncated/misaligned asm
     fragments → garbage block → bogus PC → crash in `CompileBlock`. Added an
     `#elif defined(X86_64)||defined(__x86_64__)` block with sizes measured from
     `nm -n dynalib_x86_64.asm.o` (PROLOGSIZE 42, NORMAL 11, DELAY_SLOT 43, DELAY_AFTER 24,
     EPILOG 13, PageFlip/DELAYJUMP 29, DEBUG 43; CLOCK offsets 10/10/5/10 — verified by
     disassembling the `.code` section: seperator = `add [r12],2` + `add [r12+4],1`, cycle
     immediate at byte 10).
  2. `libretro.c`: set `yinit.use_sh2_cache = 1` (the `!use_sh2_cache` overrideMemFunc path
     truncates 64-bit host function pointers on x86_64).
  3. **ABI (the real blocker): `dynalib_x86_64.asm` is written for the Microsoft x64 ABI**
     (asm comment line 37: args in rcx/rdx/r8/r9; rdi/rsi nonvolatile; 32-byte shadow space
     via `sub rsp,0x28`). On Linux SysV the block was entered with the context in RDI, but the
     prologue reads it from RCX (`mov rdi,rcx`) → garbage context → `mov (%r12),%eax` with
     r12=0x5f → crash. Also every opcode keeps GenReg/SysReg in rdi/rsi across `call` to the
     mem helpers, relying on Win64 nonvolatility; and passes helper args in ecx/edx (Win64).
     FIX (small, elegant — no asm rewrite): added `DYNAREC_MSABI` = `__attribute__((ms_abi))`
     (DynarecSh2.h) and applied it to the `dynaFunc` block-entry typedef and ALL 16 C helpers
     the JIT calls (memGet/Set{Byte,Word,Long}[NoCache], EachClock/DelayEachClock/Debug*),
     decls in DynarecSh2.h + defs in DynarecSh2CInterface.cpp. GCC then emits Win64-convention
     versions matching the asm exactly. NOTE: old Makefile has no header-dep tracking — `rm`
     the dynarec .o's after editing the .h or it won't recompile.
  4. **opcode SIZES (opdesc):** sizes for opcodes with an internal `.continue` label (BSR/BRA and
     other sign-extend/branch ops) were too small — they stopped at `.continue`, so the opcode tail
     (e.g. BSR's `mov r14d,eax` that sets the branch target) was never copied → branch fell through.
     Recomputed ALL opdesc sizes from the assembled `.code`: size = (next opcode's `_size` data label)
     − (this `x86_` label). KEY GOTCHA: each opcode's 7-byte opdesc data (dw size + 5 db offsets) is
     emitted IN `.code` immediately before its `x86_` label, so size = next_x86_label − 7 (NOT the raw
     label diff, which over-counts by 7 and made the opdesc bytes execute as garbage → SIGILL).
  5. **opcodePass off3 + BT/BF imm offsets:** (a) the `#if _WINDOWS` off3 (12-bit branch disp) patch
     must also cover x86_64 — Linux fell into the ARM `#else` split-byte path, corrupting branch
     templates; changed to `#if defined(_WINDOWS)||defined(X86_64)||defined(__x86_64__)`. (b) BT/BF
     `imm` offset 18→15: NASM `-Ox` shortens their first instr `and r14d,dword 0` (7B→4B imm8),
     shifting the disp operand −3; the offsets were authored for the un-optimized form. Only BT/BF
     start with `and r14d,dword 0`. (Do NOT switch nasm to `-O0` — it changes other encodings and
     crashes; keep default `-Ox` + this targeted offset fix.)
  RESULT: control flow now matches the interpreter for ~150k blocks — BSR/BT/BF branches resolve to
  the correct targets and the dynarec reaches the BIOS milestone 0x7d384 (was 12 blocks → 283 → 150k).
  STILL OPEN: a crash in `cache_f` (memory helper) at ~0x7d388, after a ~3.6M-iteration `DT R3;BF`
  loop → at least one more opcode bug (likely a memory-addressing op). Method that found bugs 4-5:
  env `YABA_PCLOG` → log master-SH2 PC+regs in `SH2InterpreterExec` (sh2int.c) and `DynarecSh2::Execute`,
  then phase-robust set-membership diff (a dyn (PC,regs) state must appear somewhere in the interp set).
  It false-positives on huge loops (trace budget); to finish, build an in-`Execute` per-block interpreter
  lockstep verifier (run interpreter on scratch regs over the block, compare). Intra-block jump opt
  (`internal_jmp`) is `#if AARCH64`-guarded, inactive on x86_64.
- **Auto-frameskip skips ALL rendering** after frame 0 under libretro (`one_frame_rendered` stays 0).
  Use `frameskip=disabled`.
- **Intermittent SIGSEGV in libvulkan_radeon during the frontend present** (every few hundred frames),
  and **netcmd SCREENSHOT / HW readback** can crash — likely set_image lacks a completion semaphore
  (we sync with vkDeviceWaitIdle only). Consider providing a semaphore to set_image.
- **Some games don't boot** (e.g. `SS-parodius-sexy.chd`: CPU runs but VDP2 display never enabled,
  `TVMD=0`) — game/CD-specific, not the renderer. Taroumaru boots and displays fine.

### Working config (YabaSanshiro.opt)
`video_core=vulkan`, `sh2coretype=interpreter`, `frameskip=disabled`, `resolution_mode=original`.

### Notes
- Screenshot verification: X11 grab (`import`) returns black for the Vulkan flip surface
  (not real); RetroArch netcmd SCREENSHOT triggers a HW readback that currently crashes.
  Use the `YABA_PROBE` readback instead.
- Headless test cmd: `YABA_PROBE=1 retroarch -L ./yabasanshiro_libretro.so <game.chd>`.
  Build: `make HAVE_VULKAN=1 -j$(nproc)`.

---
## (earlier) original plan below

## Constraints & Preferences
- Must support both x86_64 and aarch64 cross-compilation
- Vulkan renderer must use the libretro HW context interface (`RETRO_HW_CONTEXT_VULKAN` + `libretro_vulkan.h`)
- aarch64 FLAC linking uses `-l:libFLAC.so.12` (no unversioned symlink)
- VDP1 VRAM decoded on CPU → RGBA atlas → GPU; VDP2 compositing kept in software (Titan) for correctness

## Progress
### Done
- Studied standalone Vulkan renderer (`src/vulkan/VIDVulkan.cpp`, `Vdp1Renderer.cpp`, shaders) for VDP1/VDP2 GPU approach
- Studied Titan software compositor flow and `Vdp1DrawCommands` command processing
- Rewrote `src/libretro/vidvulkan_libretro.c` with full GPU VDP1 rendering path:
  - CPU-side VDP1 VRAM character decoder (`decode_char_to_atlas`) → CRAM lookup → RGBA atlas (1024×1024)
  - VDP1 offscreen framebuffer (1024×512) with render pass + dual-buffer support
  - Vulkan graphics pipeline (sprite quad with alpha blending, indexed TRIANGLE_LIST drawing)
  - SPIR-V shaders compiled via `glslc` → `.inc` files (`vidvulkan_sprite_vert.inc`, `vidvulkan_sprite_frag.inc`, `vidvulkan_fs_vert.inc`, `vidvulkan_fs_frag.inc`)
  - Texture atlas upload + pipeline barriers
  - Vertex buffer management (coords → NDC, UV mapping)
  - VDP1 framebuffer readback (`vdp1_fb_img` → staging → `vdp1backframebuffer`) for VDP2 software composite
  - Output image creation for `set_image()` presentation (dispbuffer → B8G8R8A8_UNORM → frontend)
  - All VDP1 callbacks wired (normal/scaled/distorted/polygon/polyline/line, clipping, coordinate, framebuffer ops)
  - VDP2 callbacks still delegate to `VIDSoftVdp2DrawStart/End/Screens` (software Titan compositing)
  - `vidsoft_vulkan_mode = 1` prevents double-swap inside `VIDSoftVdp2DrawEnd()`
- aarch64 FLAC linking: `Makefile` conditional `-l:libFLAC.so.12` (`ARCH=aarch64`)
- Fixed field names (`CMDPMOD`/`CMDCOLR`/`CMDSRCA`) to match `vdp1cmd_struct`
- Fixed VDP1 character address calculation (`CMDSRCA * 8`, not nonexistent `CMDQAC`)
- Removed CRAM texture from Vulkan pipeline (CRAM decoded entirely on CPU)
- Added missing `extern u8 *vdp1backframebuffer` declaration
- **x86_64 Vulkan build**: compiles + links (verified)
- **aarch64 Vulkan build**: compiles + links with `-l:libFLAC.so.12` (verified)

### In Progress / Blocked
- **No runtime testing yet** — needs a real emulator launch with a Saturn game to test GPU rendering path
- CRAM decode → RGBA conversion in `decode_cram()` uses `cram_decoded[512]` but CRAM is color RAM (VDP2 palette), not the VDP1 color lookup table. Need to confirm CRAM usage is correct for VDP1 sprite palettes.

## Key Decisions
- **VDP1 GPU**: Decode VRAM to RGBA on CPU (using existing `Vdp1ReadCommand` + CRAM decode), upload as one atlas texture, draw sprites as indexed textured quads → offscreen framebuffer. Readback VDP1 FB to CPU for VDP2 software composite. Avoids complex shader-based VRAM format decoding while still offloading per-pixel fill to GPU.
- **VDP2**: Keep software Titan compositing — it composites once per frame (not a bottleneck) and handles all edge cases (rotation, window, priority, color offset).
- **Pipeline**: Sprite quad drawn with indexed TRIANGLE_LIST, NDC coordinates, integer texel coordinates for nearest-neighbor sampling.
- **Blend mode**: Alpha blending (SRC_ALPHA / ONE_MINUS_SRC_ALPHA) — TODO: support VDP1-specific blend modes (half-transparent, mesh, window).
- **CRAM on CPU**: CRAM is decoded to `cram_decoded[512]` array, and `decode_char_to_atlas` uses it for palette lookup. No separate CRAM GPU texture needed.

## Next Steps
1. **Runtime test** with a Saturn game in a Vulkan-capable RetroArch build
2. Fix potential issues:
   - Confirm CRAM (VDP2 palette RAM) is the correct color source for VDP1 sprites (vs VDP1-specific palette RAM)
   - Verify pixel format in `readback_vdp1_fb` matches what VDP2 software compositor expects
3. Add per-sprite blend mode pipeline (half-transparent, mesh shadow, gouraud shading)
4. Add user/system clipping support to GPU path
5. Optimize: skip atlas re-upload if VRAM unchanged; use push constants for per-sprite params

## Relevant Files
- `src/libretro/vidvulkan_libretro.c`: Full GPU VDP1 renderer + Vulkan presentation (1330 lines, rewritten)
- `src/libretro/vidvulkan_{sprite_vert,sprite_frag,fs_vert,fs_frag}.inc`: SPIR-V shader byte arrays
- `src/vidsoft.c` / `src/vidsoft.h`: Software renderer — `VIDSoftVdp2DrawStart/End/Screens`, `vdp1framebuffer`, `dispbuffer`, `Vdp1ReadCommand`
- `src/vdp1.cpp` / `src/vdp1.h`: `Vdp1DrawCommands()` command loop, `vdp1cmd_struct` definition
- `src/libretro/Makefile`: Conditional FLAC linking for aarch64, `-DHAVE_VULKAN` per-file
- `src/vulkan/`: Standalone C++ Vulkan renderer (reference, not used directly for libretro)
