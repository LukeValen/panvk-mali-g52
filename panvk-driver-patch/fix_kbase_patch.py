#!/usr/bin/env python3
path = "/data/data/com.termux/files/home/funnymdzz-mesa/src/panfrost/vulkan/jm/panvk_vX_gpu_queue.c"

with open(path, "r") as f:
    content = f.read()

old_a = '''      if (PANVK_DEBUG(TRACE) || PANVK_DEBUG(SYNC)) {
         ret = drmSyncobjWait(dev->drm_fd, &submit.out_sync, 1, INT64_MAX, 0,
                              NULL);
         assert(!ret);

         /* If we want to read the descriptors back, we need to invalidate the
          * whole desc pool, otherwise we might end up with stale data. */
         panvk_pool_invalidate_maps(&cmdbuf->desc_pool);
         pan_kmod_flush_bo_map_syncs(dev->kmod.dev);
      }

      if (PANVK_DEBUG(TRACE)) {
         pandecode_jc(dev->debug.decode_ctx, batch->vtc_jc.first_job,
                      phys_dev->kmod.dev->props.gpu_id);
      }

      if (PANVK_DEBUG(DUMP))
         pandecode_dump_mappings(dev->debug.decode_ctx);

      if (PANVK_DEBUG(SYNC))
         pandecode_abort_on_fault(dev->debug.decode_ctx, submit.jc,
                                  phys_dev->kmod.dev->props.gpu_id);
   }'''
new_a = '''      if (PANVK_DEBUG(TRACE)) {
         panvk_pool_invalidate_maps(&cmdbuf->desc_pool);
         pandecode_jc(dev->debug.decode_ctx, batch->vtc_jc.first_job,
                      phys_dev->kmod.dev->props.gpu_id);
      }

      if (PANVK_DEBUG(DUMP))
         pandecode_dump_mappings(dev->debug.decode_ctx);
   }'''

count_a = content.count(old_a)
assert count_a == 1, f"ANCHOR A found {count_a} times, expected 1"
content = content.replace(old_a, new_a, 1)

old_b = '''      if (PANVK_DEBUG(TRACE) || PANVK_DEBUG(SYNC)) {
         ret = drmSyncobjWait(dev->drm_fd, &submit.out_sync, 1, INT64_MAX, 0,
                              NULL);
         assert(!ret);

         /* If we want to read the descriptors back, we need to invalidate the
          * whole desc pool, otherwise we might end up with stale data. */
         panvk_pool_invalidate_maps(&cmdbuf->desc_pool);
         pan_kmod_flush_bo_map_syncs(dev->kmod.dev);
      }

      if (PANVK_DEBUG(TRACE))
         pandecode_jc(dev->debug.decode_ctx, batch->frag_jc.first_job,
                      phys_dev->kmod.dev->props.gpu_id);

      if (PANVK_DEBUG(DUMP))
         pandecode_dump_mappings(dev->debug.decode_ctx);

      if (PANVK_DEBUG(SYNC))
         pandecode_abort_on_fault(dev->debug.decode_ctx, submit.jc,
                                  phys_dev->kmod.dev->props.gpu_id);
   }'''
new_b = '''      if (PANVK_DEBUG(TRACE)) {
         panvk_pool_invalidate_maps(&cmdbuf->desc_pool);
         pandecode_jc(dev->debug.decode_ctx, batch->frag_jc.first_job,
                      phys_dev->kmod.dev->props.gpu_id);
      }

      if (PANVK_DEBUG(DUMP))
         pandecode_dump_mappings(dev->debug.decode_ctx);
   }'''

count_b = content.count(old_b)
assert count_b == 1, f"ANCHOR B found {count_b} times, expected 1"
content = content.replace(old_b, new_b, 1)

with open(path, "w") as f:
    f.write(content)

print("FIX APPLIED SUCCESSFULLY")
