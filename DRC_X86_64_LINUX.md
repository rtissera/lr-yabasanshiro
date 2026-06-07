# YabaSanshiro SH2 dynarec (devmiyax) — x86_64 / Linux port

Status: **boots, runs, audio plays, no crash — but stays black** (VDP2 display-enable
`TVMD` never set). The **interpreter is fully working** (video + sound, verified
67766/76800 non-black px) and is the safe default. The dynarec is selectable
(`yabasanshiro_sh2coretype=dynarec`) for continued work.

The devmiyax JIT (`src/sh2_dynarec_devmiyax/`) was never functional on Linux x86_64
(only Win64 / ARM / AArch64 were wired). This documents the port: bugs fixed, the
differential test method, what's proven correct, and the one open issue.

---

## 1. Build & run (headless — never opens a window)

```
make HAVE_VULKAN=1 -j$(nproc)            # -> yabasanshiro_libretro.so
# headless run via software Vulkan (lavapipe) + virtual X:
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a retroarch -L ./yabasanshiro_libretro.so <game>.chd
```
Core options file: `~/.config/retroarch/config/YabaSanshiro/YabaSanshiro.opt`
(`yabasanshiro_sh2coretype = dynarec|interpreter`, `_video_core = vulkan|opengl|software`).

Gated debug env vars (safe, compiled in):
- `YABA_READBACK=1` — one-shot GPU framebuffer non-black probe (vidvulkan_bridge).
- `YABA_VDP2REG=1` — log VDP2 TVMD/BGON/PRINA per frame (vdp2.cpp).
- `YABA_BLOCK1=1` — force 1-instruction blocks in a PC window (per-instruction trace;
  DynarecSh2.cpp ~1111). Very slow; for tracing only.

aarch64 reference oracle: `gx10-a3a1.local` (NVIDIA GB10, 20-core). The aarch64
dynarec **runs the game correctly** headless (TVMD=8000, scenes cycle), confirming the
x86 issue is x86-specific. See §6.

---

## 2. Bugs fixed (all verified bit-exact vs the aarch64 oracle / interpreter)

1. **x86_64 JIT fragment-size macros** (DynarecSh2.cpp ~L346): was falling through to
   ARMv7 sizes → instant crash. Added an `X86_64` size block.
2. **Microsoft x64 ABI**: `dynalib_x86_64.asm` uses the MS ABI (args rcx/rdx,
   rbx/rbp/rdi/r12–15 nonvolatile, shadow space). Added
   `DYNAREC_MSABI = __attribute__((ms_abi))` (DynarecSh2.h) on the `dynaFunc`
   block-entry typedef + all 16 JIT-called C helpers
   (memGet/Set{Byte,Word,Long}[NoCache], *EachClock) so GCC emits Win64-convention
   functions matching the asm.
3. **opdesc template sizes**: recomputed for x86_64. opdesc data (7 bytes: dw size +
   5 db offsets) is emitted IN `.code` right before each `x86_<op>` label, so
   `size = (next x86_ label) − 7`. The `.continue`-tail opcodes (branches) were
   truncated; fixed.
4. **opcodePass off3 / BT-BF imm offsets**: (a) the off3 branch-displacement patch must
   cover x86_64 too (`#if _WINDOWS || X86_64 || __x86_64__`) as a single u16 write; the
   ARM split-byte path corrupted x86 branch templates. (b) BT/BF `imm` offset 18→15:
   NASM -Ox shrinks their leading `and r,dword 0` (7B→4B), shifting the disp operand.
5. **`overrideMemFunc` 64-bit truncation** (DynarecSh2.cpp ~L955): when SH2 cache
   emulation is off, the JIT's hardcoded cache accessors (`mov rax, memGetByte`) are
   swapped for the NoCache variants. The scan/patch used only the **low 32 bits** of
   the 64-bit `movabs` immediate → with ASLR the core `.so` loads >4 GB, the scan never
   matched → override silently dead → x86 always used the (wrong-in-harness) cache
   accessors. Rewrote to match/patch the **full 8-byte** immediate (table-driven).
   Also fixed a copy-paste bug (a `memSetLong` branch that should be `memGetLong`).
   → `libretro.c` now sets `yinit.use_sh2_cache = 0` so x86 uses the direct (NoCache,
   T2 word-swap-correct) WRAM accessors, matching aarch64. (Interpreter verified still
   correct with cache=0: TVMD=8000, 67766 non-black.)
6. **DIV1 opdesc** (`dynalib_x86_64.asm`): was `389,6,6` — src and dest both = offset 6.
   The template's Rm (divisor) patch point is at offset **62** (`add rbp,byte $00` →
   `mov r13d,[rbp]`), which was therefore **never patched → DIV1 always divided by R0**.
   The unit test only used `div1 r0,r3` (m=0) so it passed by luck. The game's 32-step
   software divide (rotcl/div1 sled) was wrong. Fixed to `389,62,6`; full divide now
   matches the oracle bit-exact.

---

## 3. Differential test harness (the method)

`src/sh2_dynarec_devmiyax/optest/` — builds the devmiyax gtest opcode suite standalone
via a minimal gtest shim (`build.sh`, `gtest/gtest.h`), plus targeted `probe_*.cpp`
single-scenario tests. Build the same harness on x86 (locally) and aarch64
(gx10-a3a1.local) and **diff outputs** — the aarch64 dynarec is the oracle.

Must use the real-build config: `-DCACHE_ENABLE=1` and set `use_sh2_cache` to match.
GOTCHAS:
- `use_sh2_cache=0` makes x86 run the override path; without the 8-byte fix it
  produces false "x86-only bugs" (crashes from the truncated 64-bit patch).
- Per-opcode cycle VALUES live in shared C++ (`opdesc(...)` in DynarecSh2.cpp) →
  identical x86/ARM; cycle differences can't explain x86-vs-ARM.
- Do NOT rewrite memory_for_test `MappedMemory*` to T2 — test expectations are
  calibrated to the original (raw byte / BSWAP16L word / BSWAP32 long) harness.

---

## 4. Proven correct (x86 == aarch64 == interpreter)

- **20480-opcode single-op brute force** (`probe_fuzz`, distinct nonzero regs,
  use_sh2_cache=0, override active): x86 == ARM for ALL opcodes **except** `LDC.L SR`
  (0x4n07) — and there **x86 is the correct one** (interpreter mask 0x3F3 =
  sh2int.c:1110; ARM drops the I-mask). So ARM is an *imperfect* opcode oracle; compare
  against the interpreter when they disagree.
- div1 32-step sequence, rotcl chaining, braf, delay-slot branches (bf/s,bt/s,bra,bsr,
  bsrf), multi-memory-op blocks — all match.
- **Cycle counting correct + identical** (`probe_cyc`: GET_COUNT 5/5/14 on both).
  Wiring verified: prologue r12=&SysReg[3]=PC, [r12+4]=SysReg[4]=COUNT, x86
  `NORMAL_CLOCK_OFFSET=10` patches the Clock-add immediate with `asm_list[i].cycle`.

**Conclusion: x86 codegen + cycle counting are correct in isolation.**

---

## 5. The open bug (full-system timing)

Symptom: dynarec boots + audio + partial VDP2 setup (BGON enabled) but `TVMD` never
gets set → black. Spins around PC 0x06012ca0 (a software sqrt: dmuls.l/sts mach/shll8).

Tracing the first interpreter-vs-dynarec divergence (headless, wide-window register
trace + unique-PC alignment): **PC 0x060108b6**, a counter at WRAM **0x060348a4** =
`0xf` (dyn) vs `0x18` (int). That counter is `counter++` at PC 0x0601038c; the main
flow snapshots it. So an earlier value/timing diverged and propagates.

Ruled out as the cause (all headless):
- **Interrupts**: instrumented delivery in both cores — **0 master interrupts during
  boot** in BOTH. Not interrupt-driven.
- **Slave SH2**: executes **0 WRAM code during boot** (idle). No slave async source.
- **DMA**: no DMA writes to the counter address.
- **Opcode codegen + cycle counting**: identical to ARM in isolation (§4).

So master runs game code alone, yet the counter diverges → the only remaining influence
is **CPU-cycle-vs-peripheral timing in the full system** (master polls a scheduler-
driven hardware status — VDP TVSTAT/HBLANK, SMPC, CD, or a DMA flag — and the dynarec
advances SH2 cycles at a slightly different rate vs the scheduler than the interpreter).

Race/timing-sensitivity confirmed:
- The black outcome **varies by renderer** (RADV → BGON=000f; lavapipe → BGON=0000).
- `YABA_NOIDLE=1` (disable the idle-skip) crashes when run fast but **does not crash
  under gdb** (slowed) → a **race condition**. The x86-only idle-skip (see §7) masks it,
  so idle-skip is kept ON.

`internal_jmp` is **RULED OUT** as a fix: it's perf-only (cycle counts identical with or
without it) AND counterproductive — without it each loop iteration re-dispatches through
`Execute()` which checks interrupts every iteration (DynarecSh2.cpp:1772); a native
internal_jmp loop runs many iterations with no interrupt check until the budget exhausts,
reducing interrupt-delivery granularity — the opposite of what this sync bug needs.

---

## 6. aarch64 oracle box (gx10-a3a1.local)

NVIDIA GB10, aarch64, Ubuntu 24.04. Full headless game runs work:
- apt: retroarch xvfb liblzma/zstd/zlib/gl/flac/vulkan/shaderc-dev (sudo OK).
- After a `HAVE_VULKAN=0` build, `rm src/libretro/libretro.c.o
  src/libretro/vidvulkan_libretro.c.o` before a `HAVE_VULKAN=1` build (make doesn't
  rebuild on flag change → stale GLES/no-HW context).
- Run: `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json xvfb-run -a retroarch -L
  ./yabasanshiro_libretro.so ~/roms/<game>.chd` with `_video_core=vulkan`.
- Result: aarch64 dynarec → TVMD=8000, BGON cycling scenes = runs to display.

Caveat: the box can be loaded/slow under lavapipe; give long timeouts and a quiet box.

---

## 7. x86-only code paths (where a remaining x86 bug must live)

Everything in shared C++ matches ARM (which works), so a remaining x86 bug is in:
- `dynalib_x86_64.asm` — opcode templates (verified via §4) + prologue/epilogue +
  the normal/delay separators.
- `overrideMemFunc` `#else` branch (verified for single + multi-mem ops).
- The **idle-skip** (DynarecSh2.cpp: CompileBlock ~858 detects `DT Rn; BF self` and
  Execute ~1733 fast-forwards it). x86-only; AArch64 uses internal_jmp/native loops.
  Cycle-equivalent to re-dispatch in probes, but it's the only x86-only logic that runs
  in-game and interacts with scheduler timing. Kept ON because NOIDLE exposes a race.

---

## 8. Best next steps (in order)

1. **Reliable ARM-dyn counter@0x060108b6** on a quiet box: is the correct dynarec value
   `0x18` (interp) → real x86 timing divergence to chase; or `0xf` → that value is fine
   and the true hang is later. Cheapest decisive test.
2. **Full-system cycle-lockstep**: run interpreter and dynarec to the same emulated
   cycle and diff all SH2 + peripheral state. Heavy instrumentation; the only thing that
   bottoms out a full-system timing bug.
3. **Chase the thread race**: emulation thread vs render/audio/peripheral threads and
   the `AddInterrupt`/`CheckInterupt` mutex (`mtx_`). The NOIDLE crash is the easiest
   handle on it.

Do NOT implement internal_jmp (§5).
