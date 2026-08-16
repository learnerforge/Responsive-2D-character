# Memory Budget — the 5 MB constraint

## Goal and definition

**Target: working-set memory ≤ 5 MB** as shown by Task Manager during normal
idle + tracking operation on Windows 10/11 x64.

- *Working set* = the set of memory pages physically resident and attributed to
  the process (RAM footprint), **not** virtual address space and **not** disk
  size.
- Task Manager's "Memory" column approximates the working set.

This is the single hardest requirement and every implementation rule below
exists to protect it. Budget is measured after the app has settled (2 s after
startup, after the first few frames) — transient spikes during startup are
allowed but kept minimal.

## The baseline reality

An empty, freshly-started 32-bit Win32 GUI process on Win10/11 typically shows a
working set of **~1.5–3 MB** (shared DLL pages attributed: `user32`, `gdi32`,
`ntdll`, `kernel32`, `shell32`, `uxtheme`, DWM shims…). We add:

- our EXE code/data,
- `gdiplus.dll` shared pages,
- two 128×160 ARGB buffers — the GDI+ render bitmap and the presentation DIB
  (~82 KB each),
- a handful of GDI objects.

Each addition is small; the shared-DLL tail dominates and cannot be removed. So
the realistic floor is ~2–4 MB and our target of **<5 MB** is achievable only if
we *avoid every avoidable allocation and dependency*.

## Per-component budget

| Component | Estimate | Rule |
|---|---|---|
| EXE code + data (stripped, `/O1`, LTCG) | 0.2–0.5 MB | Build flags in [CMakeLists.txt.md](CMakeLists.txt.md) |
| Static CRT + startup (`/MT`) | 0.1–0.3 MB | No `/MD` (avoids VCRUNTIME page pulls) |
| Stack (1 MB reserve, ~16–64 KB committed) | 0.02–0.06 MB | No deep recursion |
| Heap: two fixed 128×160×4 buffers (render + present) | ~0.16 MB | The only large allocations |
| Heap: GDI+ objects, small buffers | 0.1–0.3 MB | Fixed-size arrays, reuse objects |
| `gdiplus.dll` shared pages (attributable) | 0.8–1.5 MB | Needed for anti-aliased art |
| `user32/gdi32/shell32/uxtheme/dwm` shared pages | 0.8–1.5 MB | Unavoidable OS tail |
| Working-set trim (`SetProcessWorkingSetSize(-1,-1)`) | −1.0–2.0 MB | Once at idle, see below |
| **Total (with trim)** | **2.5–4.5 MB** | ✅ under 5 MB |

## Hard rules to stay under budget

1. **Build:** `Release`, `/O1` (favor size) or `/O2`, `/GL` + `/LTCG`, `/MT`
   (static CRT), no debug info, no `/EHsc`/RTTI bloat where avoidable
   (GDI+ needs exceptions disabled-safe; keep RTTI off). Full settings in
   [CMakeLists.txt.md](CMakeLists.txt.md).
2. **No STL containers in the hot path.** Use fixed-size arrays and plain
   structs. STL `std::string`/`vector` are fine in cold paths (menu, autostart)
   but not in the 30 Hz tick.
3. **No iostream**, no `<iostream>`/`<sstream>`, no `printf` family in the app.
   Tests may use them. Each pulls in tens of KB.
4. **Lazy / minimal DLLs:** import only `user32`, `gdi32`, `gdiplus`, `shell32`,
   `advapi32`, `ole32`(not needed) — keep the import table short. Use
   `SetDefaultDllDirectories` not needed; just link few libs.
5. **Fixed image buffers only:** the GDI+ render bitmap and the presentation DIB
   (128×160 each) are created once and reused. Never allocate a bitmap per
   frame. (See [Renderer.h.md](character/Renderer.h.md) and
   [OverlayWindow.cpp.md](desktop/OverlayWindow.cpp.md).)
6. **GDI object hygiene:** `Gdiplus::Graphics`/`Bitmap` created once and reused;
   HBITMAPs/HDCs released immediately after use. Leaks grow working set.
7. **Repaint only when dirty** (see Controller/App docs) — no per-frame pixel
   churn when nothing moves. This also protects CPU, but note: *allocations
   churn pages; avoid it.*
8. **Trim working set after settling:** call
   `SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1)` once, ~2 s after
   startup, and once more after the context menu closes. This tells the OS to
   evict clean, cold pages (our code, shared DLL pages) — they reload on demand
   at negligible cost. Do **not** call it every frame (thrashing).
   Implemented in [util/WinUtil.cpp.md](util/WinUtil.cpp.md).
9. **No worker threads.** One thread = one stack. Every thread costs a 1 MB
   stack reserve (8 KB–1 MB committed depending on usage). Single-threaded only.
10. **Small stack frames / no recursion** in tick path; keep stack committed low.
11. **Windows subsystem GUI, `wWinMain`** (no console subsystem, no console
    allocation).

## What we measured vs. must measure

Acceptance procedure (also in [scripts/package.ps1.md](scripts/package.ps1.md)):

1. Build Release.
2. Launch, wait 3 s.
3. Read Task Manager "Memory (working set)" — must be **≤ 5.0 MB**.
4. Wiggle the mouse 10 s, read again — must stay **≤ 5.0 MB**.
5. Click the character, open/close the menu — transient spike allowed, must
   settle back below 5 MB within 2 s.
6. Verify with a tool: `Get-Process r2dc | Select WorkingSet64`.

## Known costs we deliberately pay

| Cost | Value | Why kept |
|---|---|---|
| `gdiplus.dll` (~1 MB attributable) | smooth anti-aliased eyes/head | Quality choice; could drop to plain GDI to reclaim ~1 MB if ever needed |

## Failure checklist (if over budget)

- [ ] Debug build? → rebuild Release.
- [ ] `/MD` instead of `/MT`? → switch to `/MT`.
- [ ] STL / iostream pulled into the 30 Hz path? → remove.
- [ ] New bitmap per frame? → reuse.
- [ ] Missing trim call? → add `TrimWorkingSet()`.
- [ ] GDI+ object leak → check `Graphics`/`Bitmap` reuse and `LockBits` release.
- [ ] Threads spawned? → single-threaded only.

## Virtual address space (informational)

VA size will be tens of MB (DLLs, heap reserves) — that is normal and **not** a
budget violation. The 5 MB constraint is about *resident* memory only.
