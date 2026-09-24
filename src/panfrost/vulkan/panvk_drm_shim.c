/* Minimal libdrm shim: this driver never exercises real DRM (no /dev/dri
 * node on this device — confirmed drmGetDevices2 always returns -ENOENT
 * via the kbase-only path). These symbols exist only to satisfy the
 * dynamic linker at load time in restricted Android vendor namespaces
 * where the real libdrm.so collides with an incompatible system/vendor
 * copy. All calls fail cleanly with -ENOSYS/-1, matching what a genuine
 * "no DRM available" environment would already report. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int drmCloseBufferHandle(int fd, uint32_t handle) { (void)fd; (void)handle; return -ENOSYS; }
void drmFreeDevice(drmDevicePtr *device) { (void)device; }
void drmFreeDevices(drmDevicePtr devices[], int count) { (void)devices; (void)count; }
void drmFreeVersion(drmVersionPtr v) { (void)v; }
int drmGetCap(int fd, uint64_t capability, uint64_t *value) { (void)fd; (void)capability; if (value) *value = 0; return -ENOSYS; }
int drmGetDevice2(int fd, uint32_t flags, drmDevicePtr *device) { (void)fd; (void)flags; (void)device; return -ENOSYS; }
int drmGetDevices2(uint32_t flags, drmDevicePtr devices[], int max_devices) { (void)flags; (void)devices; (void)max_devices; return -ENOENT; }
drmVersionPtr drmGetVersion(int fd) { (void)fd; return NULL; }
int drmIoctl(int fd, unsigned long request, void *arg) { (void)fd; (void)request; (void)arg; return -ENOSYS; }
int drmPrimeFDToHandle(int fd, int prime_fd, uint32_t *handle) { (void)fd; (void)prime_fd; (void)handle; return -ENOSYS; }
int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd) { (void)fd; (void)handle; (void)flags; (void)prime_fd; return -ENOSYS; }
int drmSyncobjCreate(int fd, uint32_t flags, uint32_t *handle) { (void)fd; (void)flags; (void)handle; return -ENOSYS; }
int drmSyncobjDestroy(int fd, uint32_t handle) { (void)fd; (void)handle; return -ENOSYS; }
int drmSyncobjExportSyncFile(int fd, uint32_t handle, int *sync_file_fd) { (void)fd; (void)handle; (void)sync_file_fd; return -ENOSYS; }
int drmSyncobjFDToHandle(int fd, int obj_fd, uint32_t *handle) { (void)fd; (void)obj_fd; (void)handle; return -ENOSYS; }
int drmSyncobjHandleToFD(int fd, uint32_t handle, int *obj_fd) { (void)fd; (void)handle; (void)obj_fd; return -ENOSYS; }
int drmSyncobjImportSyncFile(int fd, uint32_t handle, int sync_file_fd) { (void)fd; (void)handle; (void)sync_file_fd; return -ENOSYS; }
int drmSyncobjQuery2(int fd, uint32_t *handles, uint64_t *points, uint32_t count, uint32_t flags) { (void)fd; (void)handles; (void)points; (void)count; (void)flags; return -ENOSYS; }
int drmSyncobjReset(int fd, const uint32_t *handles, uint32_t handle_count) { (void)fd; (void)handles; (void)handle_count; return -ENOSYS; }
int drmSyncobjSignal(int fd, const uint32_t *handles, uint32_t handle_count) { (void)fd; (void)handles; (void)handle_count; return -ENOSYS; }
int drmSyncobjTimelineSignal(int fd, const uint32_t *handles, uint64_t *points, uint32_t handle_count) { (void)fd; (void)handles; (void)points; (void)handle_count; return -ENOSYS; }
int drmSyncobjTimelineWait(int fd, uint32_t *handles, uint64_t *points, unsigned num_handles, int64_t timeout_nsec, unsigned flags, uint32_t *first_signaled) { (void)fd; (void)handles; (void)points; (void)num_handles; (void)timeout_nsec; (void)flags; (void)first_signaled; return -ENOSYS; }
int drmSyncobjTransfer(int fd, uint32_t dst_handle, uint64_t dst_point, uint32_t src_handle, uint64_t src_point, uint32_t flags) { (void)fd; (void)dst_handle; (void)dst_point; (void)src_handle; (void)src_point; (void)flags; return -ENOSYS; }
int drmSyncobjWait(int fd, uint32_t *handles, unsigned num_handles, int64_t timeout_nsec, unsigned flags, uint32_t *first_signaled) { (void)fd; (void)handles; (void)num_handles; (void)timeout_nsec; (void)flags; (void)first_signaled; return -ENOSYS; }
