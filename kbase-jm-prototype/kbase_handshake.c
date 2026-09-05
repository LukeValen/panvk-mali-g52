#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#define KBASE_IOCTL_TYPE 0x80

struct kbase_ioctl_version_check {
    uint16_t major;
    uint16_t minor;
};
#define KBASE_IOCTL_VERSION_CHECK _IOWR(KBASE_IOCTL_TYPE, 0, struct kbase_ioctl_version_check)

struct kbase_ioctl_set_flags {
    uint32_t create_flags;
};
#define KBASE_IOCTL_SET_FLAGS _IOW(KBASE_IOCTL_TYPE, 1, struct kbase_ioctl_set_flags)

#define BASE_MEM_MAP_TRACKING_HANDLE (3ull << 12)

int main(void) {
    int fd = open("/dev/mali0", O_RDWR);
    if (fd < 0) {
        printf("open failed: %s\n", strerror(errno));
        return 1;
    }
    printf("open ok, fd=%d\n", fd);

    struct kbase_ioctl_version_check ver = {0, 0};
    if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK, &ver)) {
        printf("VERSION_CHECK failed: %s\n", strerror(errno));
        return 1;
    }
    printf("VERSION_CHECK ok: major=%u minor=%u\n", ver.major, ver.minor);

    struct kbase_ioctl_set_flags set_flags = { .create_flags = 0 };
    if (ioctl(fd, KBASE_IOCTL_SET_FLAGS, &set_flags)) {
        printf("SET_FLAGS failed: %s\n", strerror(errno));
        return 1;
    }
    printf("SET_FLAGS ok\n");

    void *tracking_page = mmap(NULL, 4096, PROT_NONE, MAP_SHARED, fd,
                                BASE_MEM_MAP_TRACKING_HANDLE);
    if (tracking_page == MAP_FAILED) {
        printf("mmap(tracking_page) failed: %s\n", strerror(errno));
        return 1;
    }
    printf("tracking_page mmap ok: %p\n", tracking_page);

    printf("HANDSHAKE COMPLETE\n");
    close(fd);
    return 0;
}
