#!/usr/bin/env python3
import sys

path = "/data/data/com.termux/files/home/funnymdzz-mesa/src/panfrost/vulkan/jm/panvk_vX_gpu_queue.c"

with open(path, "r") as f:
    content = f.read()

replacements = []

old1 = '#include "drm-uapi/panfrost_drm.h"\n'
new1 = '''#include "drm-uapi/panfrost_drm.h"
#include "drm-uapi/mali_kbase_ioctl.h"
#include <poll.h>

struct panvk_kbase_event_v2 {
   uint32_t event_code;
   uint8_t atom_number;
   uint8_t padding[3];
   uint64_t udata[2];
};

static int
panvk_kbase_submit_and_wait(int fd, uint64_t jc, uint32_t core_req)
{
   uint8_t atom_bytes[48];
   memset(atom_bytes, 0, sizeof(atom_bytes));
   memcpy(atom_bytes + 0, &jc, 8);
   atom_bytes[40] = 1;
   memcpy(atom_bytes + 44, &core_req, 4);

   struct kbase_ioctl_job_submit submit = {
      .addr = (uint64_t)(uintptr_t)atom_bytes,
      .nr_atoms = 1,
      .stride = 48,
   };

   if (ioctl(fd, KBASE_IOCTL_JOB_SUBMIT, &submit))
      return -errno;

   struct pollfd pfd = { .fd = fd, .events = POLLIN };
   int pret = poll(&pfd, 1, 5000);
   if (pret <= 0 || !(pfd.revents & POLLIN))
      return -110;

   struct panvk_kbase_event_v2 event;
   ssize_t n = read(fd, &event, sizeof(event));
   if (n != sizeof(event))
      return -5;

   if (event.event_code != 0x1)
      return -(int)event.event_code;

   return 0;
}
'''
assert old1 in content, "ANCHOR 1 NOT FOUND"
content = content.replace(old1, new1, 1)

old2 = '''   if (batch->vtc_jc.first_job) {
      struct drm_panfrost_submit submit = {
         .bo_handles = (uintptr_t)bos,
         .bo_handle_count = nr_bos,
         .in_syncs = (uintptr_t)in_fences,
         .in_sync_count = nr_in_fences,
         .out_sync = queue->sync,
         .jc = batch->vtc_jc.first_job,
      };

      ret = pan_kmod_ioctl(dev->drm_fd, DRM_IOCTL_PANFROST_SUBMIT, &submit);
      assert(!ret);'''
new2 = '''   if (batch->vtc_jc.first_job) {
      ret = panvk_kbase_submit_and_wait(dev->drm_fd, batch->vtc_jc.first_job, 0x04);
      assert(!ret);'''
assert old2 in content, "ANCHOR 2 NOT FOUND"
content = content.replace(old2, new2, 1)

old3 = '''   if (batch->frag_jc.first_job) {
      struct drm_panfrost_submit submit = {
         .bo_handles = (uintptr_t)bos,
         .bo_handle_count = nr_bos,
         .out_sync = queue->sync,
         .jc = batch->frag_jc.first_job,
         .requirements = PANFROST_JD_REQ_FS,
      };

      if (batch->vtc_jc.first_job) {
         submit.in_syncs = (uintptr_t)(&queue->sync);
         submit.in_sync_count = 1;
      } else {
         submit.in_syncs = (uintptr_t)in_fences;
         submit.in_sync_count = nr_in_fences;
      }

      ret = pan_kmod_ioctl(dev->drm_fd, DRM_IOCTL_PANFROST_SUBMIT, &submit);
      assert(!ret);'''
new3 = '''   if (batch->frag_jc.first_job) {
      ret = panvk_kbase_submit_and_wait(dev->drm_fd, batch->frag_jc.first_job, 0x01);
      assert(!ret);'''
assert old3 in content, "ANCHOR 3 NOT FOUND"
content = content.replace(old3, new3, 1)

old4 = '''   int ret = drmSyncobjCreate(device->drm_fd, DRM_SYNCOBJ_CREATE_SIGNALED,
                              &queue->sync);
   if (ret) {
      result = panvk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);
      goto err_finish_queue;
   }
'''
new4 = '''   queue->sync = 0;
'''
assert old4 in content, "ANCHOR 4 NOT FOUND"
content = content.replace(old4, new4, 1)

old5 = "   drmSyncobjDestroy(dev->drm_fd, queue->sync);\n"
new5 = "   /* PATCH: no drm_syncobj to destroy */\n"
assert old5 in content, "ANCHOR 5 NOT FOUND"
content = content.replace(old5, new5, 1)

old6 = '''   ASSERTED int ret = drmSyncobjWait(dev->drm_fd, &queue->sync, 1,
                                     INT64_MAX, DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL,
                                     NULL);
   assert(!ret);

   return VK_SUCCESS;'''
new6 = '''   return VK_SUCCESS;'''
assert old6 in content, "ANCHOR 6 NOT FOUND"
content = content.replace(old6, new6, 1)

with open(path, "w") as f:
    f.write(content)

print("PATCH APPLIED SUCCESSFULLY - 6 changes")
