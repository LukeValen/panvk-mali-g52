# PanVK on Mali-G52 MC2 (Kbase/JM) — Native Termux Build

Experimental build of Mesa's PanVK (Vulkan driver for Arm Panfrost GPUs)
targeting a Mali-G52 MC2 (Bifrost, Job Manager frontend, no CSF), compiled
**natively on-device** via Termux — no PC, no NDK cross-compilation.

## Target device
- Xiaomi Redmi 13C, MediaTek MT6769V/CZ (mt6768), Android 14 (API 34)
- Kernel 4.19.191, Mali-G52 MC2, `mali_kbase` driver, Kbase UAPI 11.38 (confirmed via ioctl)
- No root

## Status (as of this build)
- ✅ Mesa 26.3.0-devel builds successfully end-to-end (`libvulkan_panfrost.so`, ~19.6MB)
- ✅ Driver exports the standard ICD entry point (`vk_icdGetInstanceProcAddr`)
- ✅ `vkCreateInstance` succeeds when loading the driver directly via `dlopen` (bypassing any system Vulkan loader)
- ⚠️ `vkEnumeratePhysicalDevices` returns **0 devices**, even with `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`
  set. Root cause not yet identified — open investigation.

This is **not yet a working driver**. It builds and initializes at the
instance level, but does not currently detect the GPU. Documented here for
reproducibility and so the patches aren't lost.

## Why patches were needed
Termux's `clang` targets the `aarch64-*-android` triple, so `__ANDROID__` is
defined by the compiler — but Termux is not a full AOSP/NDK environment: it
lacks `liblog`, `libsync`, and `libcutils` headers/implementations. Mesa code
that branches on `DETECT_OS_ANDROID` (or `__ANDROID__` directly) assumes
those are available. All patches force the portable/non-Android code paths
that already exist in Mesa's own source, rather than inventing new behavior.

See `patches/termux-android-detection-fixes.patch` for the exact diff (5
one-line changes across 4 files).

## Build environment
- Termux, native aarch64 build (no cross-compilation)
- Toolchain: clang/gcc 21.1.8, meson 1.12.0 (via pip), ninja 1.13.2
- Mesa cloned shallow (`--depth 1`) from gitlab.freedesktop.org/mesa/mesa

## Build config
(`-j1` used due to limited free RAM on the target device during compilation)

## Kernel-side evidence (pre-build validation)
- `/dev/mali0`: opens successfully in `O_RDWR` without root
- `KBASE_IOCTL_VERSION_CHECK` ioctl: succeeds, UAPI major=11 minor=38
- `/dev/ion`: present (legacy ION allocator; no `/dev/dma_heap` on this kernel)
- `/sys/module/mali_kbase/{version,parameters/*}`: blocked by SELinux for
  non-root, both via Termux and adb shell

## Open questions
- Why does the physical device enumeration return 0 despite a successful
  instance creation and the "broken driver" override flag?
- How to install this as an alternate Vulkan ICD without root, given
  `/vendor/lib64/hw/vulkan.mt6768.so` (the resolved ICD name for
  `ro.board.platform=mt6768`) is a symlink to the proprietary blob and
  `/vendor` is read-only without remount

## License
This repository contains only patches and documentation. Mesa itself is
licensed under the MIT License; see https://gitlab.freedesktop.org/mesa/mesa
