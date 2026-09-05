# PanVK on Mali-G52 MC2 (Kbase/JM) — Native Termux Build

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
