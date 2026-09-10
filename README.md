# PanVK on Mali-G52 MC2 (Kbase/JM) — Native

Experimental work getting Mesa's PanVK (Vulkan driver for Arm Panfrost GPUs)
running on a Mali-G52 MC2 (Bifrost, Job Manager frontend, no CSF) via the
proprietary `mali_kbase` kernel driver — no DRM kernel module, no root,
compiled **natively on-device** via Termux.

## Target device
- Xiaomi Redmi 13C, MediaTek MT6769V/CZ (mt6768), Android 14 (API 34)
- Kernel 4.19.191, Mali-G52 MC2, `mali_kbase` driver, Kbase UAPI 11.38
- No root

## Credit

Phase 2 of this work builds directly on top of
[funnymdzz/mesa](https://github.com/funnymdzz/mesa) and
[leegao/mesa-funnymdzz](https://github.com/leegao/mesa-funnymdzz)
("panvk-over-kbase for winlator"), which solved GPU device *discovery*
without a DRM node (bypassing `drmGetDevices2`/`drmGetVersion` via a custom
`enumerate` hook and `pan_kmod_dev_create_with_driver`). This repo would not
have gotten past "0 physical devices" without their work. Community
discussion: https://github.com/wonderkast02/panvk-g720-kbase-csf/discussions/1

## Status

### Phase 1 — vanilla Mesa, Termux-only patches
- Mesa 26.3.0-devel builds end-to-end natively in Termux (`libvulkan_panfrost.so`)
- Driver exports the ICD entry point, `vkCreateInstance` succeeds via `dlopen`
- `vkEnumeratePhysicalDevices` returns **0 devices** — vanilla PanVK only
  discovers GPUs via DRM render nodes (`drmGetDevices2`), which don't exist
  on a `mali_kbase`-only system (no `/dev/dri`)
- See `patches/termux-android-detection-fixes.patch` (5 one-line fixes: Termux's
  clang targets the `aarch64-*-android` triple, so `__ANDROID__` is defined,
  but Termux lacks the actual AOSP userland — `liblog`, `libsync`,
  `libcutils`. All patches force the portable code paths that already exist
  in Mesa's own source)

### Phase 2 — funnymdzz/mesa fork (`-Dpanfrost-kmds=kbase,panthor`)
- GPU **detected**: `vkEnumeratePhysicalDevices` returns 1 device,
  `Mali unknown 0x74021000 MC2` (core count MC2 matches real hardware; GPU ID
  not in the driver's model table yet, falls back to conservative Bifrost
  defaults)
- Fixed `KBASE_IOCTL_MEM_JIT_INIT` failing with `ENOMEM`: the fork's default
  JIT VA size (`1<<25`, ~128GB) was rejected by this older 4.19 kernel;
  reduced to `1<<20` (~4GB), resolved
- `KBASE_IOCTL_MEM_EXEC_INIT` still fails with `EPERM` — not yet root-caused
- **Important limitation found in `kbase_kmod.c` itself**: "Command
  submission (CSF queue groups / JM job atoms) is not wired up yet; this
  backend currently only supports device enumeration and memory management."
  So even with the GPU detected, no real rendering/compute works yet through
  this backend as-is.
- 3 additional Termux patches needed on top of Phase 1's (same
  `__ANDROID__`/DRM-WSI issues), plus one meson.build fix: the fork's
  `foreach arch : [6,7,10,12,13,14]` loop unconditionally references
  `panthor_kmod_*` symbols even when only `kbase` kmod is enabled — fixed by
  building with `-Dpanfrost-kmds=kbase,panthor` (panthor only for link
  symbols, kbase is the only backend actually used at runtime, since only
  `/dev/mali0` exists)

### Phase 3 — raw Kbase JM job submission (own work, in progress)
Since command submission isn't implemented upstream, we're prototyping it
directly against the kernel UAPI, independent of Mesa for now:
- ✅ Standalone harness confirms the full JM handshake works:
  `KBASE_IOCTL_VERSION_CHECK` → `KBASE_IOCTL_SET_FLAGS` →
  `mmap(BASE_MEM_MAP_TRACKING_HANDLE)`
- ✅ Standalone harness confirms a full job submit/completion round-trip:
  built a `base_jd_atom_v2` (dependency-only, `BASE_JD_REQ_DEP`, doesn't
  touch the GPU) → `KBASE_IOCTL_JOB_SUBMIT` accepted it → `poll()` +
  `read()` returned a `base_jd_event_v2` with `event_code=BASE_JD_EVENT_DONE`
  matching the submitted atom
- This proves the atom struct layout, ioctl numbers, and completion-event
  readback are all correct for this device's UAPI (11.38) — the plumbing
  works. Next: a job that actually touches the GPU (real job chain), reusing
  the existing job-chain-building code already in the panfrost Vulkan driver
  (today feeding `DRM_IOCTL_PANFROST_SUBMIT`) instead of building one from
  scratch
- Kbase JM sync primitives (`BASE_JD_REQ_SOFT_FENCE_TRIGGER`/`_WAIT`, backed
  by the kernel's standard `sync_file`) look like a workable bridge to
  Vulkan's `vk_sync`, but this hasn't been implemented or tested yet

## Build environment
- Termux, native aarch64 build (no cross-compilation), clang/gcc 21.1.8,
  meson 1.12.0 (via pip; not packaged for Termux), ninja 1.13.2, libclc
  21.1.3-1 (needed once `-Dpanfrost-kmds` pulls in `with_clc`)
- Build config (Phase 2):
  `meson setup build -Dvulkan-drivers=panfrost -Dgallium-drivers=
  -Dplatforms= -Dbuildtype=release -Dbuild-tests=false
  -Dpanfrost-kmds=kbase,panthor`
- `ninja -C build -j1` (single job — target device has limited free RAM
  during compilation)

## Kernel-side evidence
- `/dev/mali0`: opens in `O_RDWR` without root; UAPI 11.38 confirmed via
  `KBASE_IOCTL_VERSION_CHECK`
- `/dev/ion` present (legacy ION allocator, no `/dev/dma_heap` on this kernel)
- `/sys/module/mali_kbase/{version,parameters/*}`: blocked by SELinux for
  non-root (both Termux and adb shell)
- Vulkan ICD resolution: `/vendor/lib64/hw/vulkan.mt6768.so` is a symlink to
  the proprietary `libGLES_mali.so` blob; `ro.hardware.vulkan` unset, falls
  back to `ro.board.platform=mt6768`

## Open questions
- Root cause of `KBASE_IOCTL_MEM_EXEC_INIT` returning `EPERM`
- How to build and submit a real (GPU-touching) job chain via
  `KBASE_IOCTL_JOB_SUBMIT` instead of a dependency-only atom
- How to bridge `vk_sync` to Kbase's atom-dependency/soft-fence sync model
- How to install this as an alternate Vulkan ICD without root, given
  `/vendor` is read-only without remount

## License
This repository contains only patches, a raw-ioctl prototype, and
documentation. Mesa itself is MIT-licensed; see
https://gitlab.freedesktop.org/mesa/mesa. The `funnymdzz/mesa` fork this
builds on is also Mesa-derived (MIT).

### Phase 3 — raw Kbase JM job submission (own work, standalone harnesses)

Since command submission isn't implemented upstream (Phase 2's fork), we
prototyped it directly against the kernel UAPI, independent of Mesa. All
harnesses live in `kbase-jm-prototype/`.

**Confirmed handshake** (`kbase_handshake.c`): `KBASE_IOCTL_VERSION_CHECK` →
`KBASE_IOCTL_SET_FLAGS` → `mmap(BASE_MEM_MAP_TRACKING_HANDLE)`. Every other
ioctl returns `EPERM` before this sequence completes.

**Dead end, then the real fix — `core_req`:** submitting a `base_jd_atom_v2`
with `jc` pointing to a real, byte-verified `MALI_JOB_TYPE_WRITE_VALUE` job
chain (built with Mesa's own genxml pack macros, header-only, no linking
needed) consistently "succeeded" — kernel returned `BASE_JD_EVENT_DONE` in
~29µs — but never actually wrote anything to memory. Extensive elimination
(cache sync via `KBASE_IOCTL_MEM_SYNC`, byte-level hexdump verification of
the packed job, stride bisection across 40–96 bytes confirming the kernel
only accepts 48/56/64) ruled out struct-layout issues entirely: **all three
accepted strides still faked completion without touching hardware.**

The real cause, found by reading the actual `mali_kbase_jd.c` kernel source
(`jd_run_atom()`): `core_req` is masked with `BASE_JD_REQ_ATOM_TYPE`, and if
the result equals `BASE_JD_REQ_DEP` (i.e. `core_req == 0`), the atom is
marked complete **immediately, regardless of `jc`**. We had left `core_req`
at `0` in every test. The correct flag for a `WRITE_VALUE` job is
`BASE_JD_REQ_V = (1 << 4) = 0x10` ("Requires value writeback").

**Result with `core_req = 0x10`:** everything changed. Submit-to-event time
jumped from 29µs to **1761µs** (a real hardware round-trip signature).
`exception_status` in the job header — untouched (`0x0`) in every prior
test — changed to `0x10258` after this submission: the **hardware wrote
back** into our job header for the first time. The event code is
`0x58` = `BASE_JD_EVENT_DATA_INVALID_FAULT` — a genuine hardware fault
code, not a fabricated `DONE`.

This confirms, for the first time, that the GPU actually receives and
processes a job submitted via raw Kbase ioctls on this device. The write
itself still doesn't land (job faults instead of completing), so the
current open question is what's wrong with the payload encoding for this
architecture — not whether jobs reach the hardware at all.

**Related work found mid-investigation:** a fork of this repo by
[mexicanbr0auth](https://github.com/mexicanbr0auth/panvk-mali-g57), porting
this same approach to a Mali-G57 (Valhall) on a newer kernel/DDK
(r54p1/UAPI 11.46). Their `base_jd_atom` layout differs from ours (64-byte
struct with a `renderpass_id` field at a different offset) — direct proof
that atom layout varies by DDK version and can't be assumed across devices;
useful as methodology reference, not as literal values to copy.

## Event code reference (`base_jd_event_code`)
`0x00`=NOT_STARTED, `0x01`=DONE, `0x03`=STOPPED, `0x04`=TERMINATED,
`0x40`=JOB_CONFIG_FAULT, `0x58`=DATA_INVALID_FAULT, `0x59`=TILE_RANGE_FAULT,
`0x60`=OUT_OF_MEMORY, `0x7F`=UNKNOWN, `0x80`=DELAYED_BUS_FAULT,
`0x88`=SHAREABILITY_FAULT, `0xC1`-`0xC4`=TRANSLATION_FAULT_LEVEL1-4,
`0xC8`=PERMISSION_FAULT

### Phase 4 — real PanVK driver patched to submit via Kbase (own work)

With the raw-ioctl recipe from Phase 3 proven, we patched the actual PanVK
driver's submission path (`src/panfrost/vulkan/jm/panvk_vX_gpu_queue.c`),
not just standalone harnesses. Patches applied programmatically (Python,
each replacement guarded by an exact-match assert) — see
`panvk-driver-patch/` for the scripts and the resulting diff.

**What changed:** both `DRM_IOCTL_PANFROST_SUBMIT` call sites (vertex/tiler/
compute chain and fragment chain) replaced with a helper that submits via
raw `KBASE_IOCTL_JOB_SUBMIT` and blocks synchronously on completion
(`poll()` + `read()` of the completion event) — the same mechanism proven
in Phase 3. `drm_syncobj` (create/destroy/wait) removed entirely, since it
depends on real DRM ioctls the kbase fd doesn't support, and synchronous
submission doesn't need it. This sacrifices async pipelining for
correctness first; optimizing back to async (via kbase's `sync_file`-backed
soft-fence atoms) is future work, not required for functional correctness.

`core_req` values: `0x16` (`T|CS|V` combined) for the vertex/tiler/compute
chain — since it can contain mixed job types — and `0x01` (`FS`) for the
fragment chain, matching the naming already used by the DRM driver
(`PANFROST_JD_REQ_FS`).

**Result: full Vulkan compute pipeline confirmed working end to end.**
`vkCreateInstance` → `vkCreateDevice` → `vkCreateBuffer` +
`vkCreateShaderModule` (a real SPIR-V compute shader, assembled with
`spirv-as` from `write_value.spvasm` — no `glslang` needed) →
`vkCreateComputePipeline` → descriptor sets → a recorded command buffer
with `vkCmdDispatch(1,1,1)` → `vkQueueSubmit` (exercising the patched
Kbase path) → `vkQueueWaitIdle` → buffer readback returns exactly the
value the shader was compiled to write. See `panvk_compute_test.c`.

This is the first confirmed real GPU compute execution through the actual
Vulkan driver (not a standalone test) on a Mali-G52 via `mali_kbase`
without a DRM kernel module, without root, and without the proprietary
Vulkan blob.

**Not yet tested:** real graphics draw calls (vertex + fragment,
`core_req=0x01` path unverified), async/pipelined submission, WSI/display
output — needed before this can render an actual game frame via Winlator.

### Phase 5 — GPU variant recognition fix

Community input (thanks wonderkast02 to Wonder for reviewing the project and
flagging this) identified that the "unknown gpu_id" warning had a fixable
root cause: `pan_model.c`'s `BIFROST_MODEL` macro hardcodes `gpu_variant=0`
for every Bifrost table entry, but this specific G52 reports
`gpu_variant=0x2` (read from `CORE_FEATURES`). Since `product_id=0x7402` is
already unique to "G52 r1" in the table, `gpu_variant` was redundant for
this match. One-line patch in `pan_get_model()`
(`panvk-driver-patch/gpu_variant_fix.patch`): ignore `gpu_variant` for
Bifrost (`arch <= 7`).

**Result:** the device is now correctly identified as `Mali-G52 r1 MC2`
instead of `Mali unknown 0x74021000 MC2` — unlocking model-specific
defaults instead of the conservative generic-Bifrost fallback.

### Phase 6 — EXEC_INIT ordering fix

Second tip from Isaac Andrade: swap the order of `KBASE_IOCTL_MEM_EXEC_INIT`
and `KBASE_IOCTL_MEM_JIT_INIT` in `kbase_kmod.c` — run EXEC_INIT first. This
contradicted what the G57 fork's notes said (that the `EPERM` on EXEC_INIT
was normal "once-only ioctl" behavior), so we treated it as an unconfirmed
hypothesis and tested it directly rather than assuming either source was
right.

**Result: confirmed correct.** The `KBASE_IOCTL_MEM_EXEC_INIT failed:
Operation not permitted` warning is gone entirely after reordering. The
full compute pipeline test (Phase 4) was re-run and still passes cleanly,
with no warnings at all. This means the previous `EPERM` was a real
ordering bug, not benign — it likely means executable GPU memory (needed
for real shaders, not just compute buffers) was silently broken before this
fix. See `panvk-driver-patch/exec_init_order_fix.patch`.

### Phase 7 — real graphics draw call confirmed (offscreen)

With the graphics (`FS`) submission path unverified, we tested it directly:
`panvk_draw_test.c` creates a 4×4 offscreen `R8G8B8A8` color image, a render
pass, a real graphics pipeline with hand-written SPIR-V vertex and fragment
shaders (`triangle_vert.spvasm`/`triangle_frag.spvasm` — no `glslang`
available in Termux, assembled with `spirv-as`, validated with `spirv-val`),
draws a full-screen triangle, copies the result to a host-visible buffer,
and reads it back.

**Result: pixel readback = RGBA(255, 0, 0, 255)** — exactly the red the
fragment shader writes. This confirms the `frag_jc`/`core_req=0x01` (`FS`)
path in our Kbase submission patch works correctly, completing validation
of both submission paths used by the real driver (vertex/tiler/compute via
`0x16`, fragment via `0x01`).

**Note:** this is an offscreen test — the image only exists in GPU memory,
never connected to a display. Nothing appears on screen. Showing something
on-screen requires WSI (window system integration) — a swapchain backed by
an `ANativeWindow`, since kbase exposes no `/dev/dri` and the standard
PanVK WSI paths (DRM/X11/Wayland) don't apply here. That's separate,
not-yet-started work.

### Phase 8 — WSI investigation (X11/XCB), in progress

Started implementing real on-screen presentation via WSI. `wsi_common_x11.c`
was confirmed present and buildable (`-Dplatforms=x11` compiles cleanly with
no conflict against the earlier direct-display WSI patches). Set up `Xvfb`
+ `xorgproto`/`libxrandr`/`xcb-*` in Termux to test locally.

**Confirmed working in isolation:**
- `VK_KHR_xcb_surface` present in instance extensions
- `vkGetPhysicalDeviceXcbPresentationSupportKHR` returns `YES`
- Creating a window + `VkSurfaceKHR` via **XCB** (not Xlib) and calling
  `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` on it works correctly in a
  minimal standalone test (`wsi-investigation/vk_surface_xcb_check.c`)

**Still broken:** the same call, inside the fuller
`panvk_swapchain_test_xcb.c` (device creation, more proc addrs resolved
first, etc.), hangs indefinitely — confirmed via `/proc/PID/wchan` showing
`futex_wait_queue_me` (blocked on a userspace mutex, not waiting on the X
server). Root cause not found yet despite systematic bisection between the
working minimal test and the hanging fuller one — they appear semantically
identical in call order and flags. Note: `vkCreateXlibSurfaceKHR` was
initially suspected and is broken too, but switching to
`vkCreateXcbSurfaceKHR` alone did not fix the fuller test, so Xlib-vs-XCB
was a red herring for the full case (even though the minimal-XCB case
does work).

This does not block anything already proven working (compute, offscreen
draw) — it only affects on-screen presentation, which is separate,
not-yet-necessary work for further GPU-side progress. To be resumed.
