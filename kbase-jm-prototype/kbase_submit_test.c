#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#define KBASE_IOCTL_TYPE 0x80

struct kbase_ioctl_version_check { uint16_t major; uint16_t minor; };
#define KBASE_IOCTL_VERSION_CHECK _IOWR(KBASE_IOCTL_TYPE, 0, struct kbase_ioctl_version_check)

struct kbase_ioctl_set_flags { uint32_t create_flags; };
#define KBASE_IOCTL_SET_FLAGS _IOW(KBASE_IOCTL_TYPE, 1, struct kbase_ioctl_set_flags)

struct kbase_ioctl_job_submit { uint64_t addr; uint32_t nr_atoms; uint32_t stride; };
#define KBASE_IOCTL_JOB_SUBMIT _IOW(KBASE_IOCTL_TYPE, 2, struct kbase_ioctl_job_submit)

#define BASE_MEM_MAP_TRACKING_HANDLE (3ull << 12)

/* base_jd_atom_v2 - layout do header oficial ARM (mali_base_kernel.h) */
struct base_jd_udata { uint64_t blob[2]; };
struct base_dependency { uint8_t atom_id; uint8_t dependency_type; };
typedef uint32_t base_jd_core_req;
#define BASE_JD_REQ_DEP ((base_jd_core_req)0)

struct base_jd_atom_v2 {
    uint64_t jc;
    struct base_jd_udata udata;
    uint64_t extres_list;
    uint16_t nr_extres;
    uint16_t compat_core_req;
    struct base_dependency pre_dep[2];
    uint8_t atom_number;
    uint8_t prio;
    uint8_t device_nr;
    uint8_t padding1;
    base_jd_core_req core_req;
};

struct base_jd_event_v2 {
    uint32_t event_code;
    uint8_t atom_number;
    uint8_t padding[3];
    struct base_jd_udata udata;
};

int main(void) {
    int fd = open("/dev/mali0", O_RDWR);
    if (fd < 0) { printf("open failed: %s\n", strerror(errno)); return 1; }
    printf("open ok, fd=%d\n", fd);

    struct kbase_ioctl_version_check ver = {0, 0};
    if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK, &ver)) {
        printf("VERSION_CHECK failed: %s\n", strerror(errno)); return 1;
    }
    printf("VERSION_CHECK ok: %u.%u\n", ver.major, ver.minor);

    struct kbase_ioctl_set_flags set_flags = { .create_flags = 0 };
    if (ioctl(fd, KBASE_IOCTL_SET_FLAGS, &set_flags)) {
        printf("SET_FLAGS failed: %s\n", strerror(errno)); return 1;
    }
    printf("SET_FLAGS ok\n");

    void *tracking_page = mmap(NULL, 4096, PROT_NONE, MAP_SHARED, fd, BASE_MEM_MAP_TRACKING_HANDLE);
    if (tracking_page == MAP_FAILED) {
        printf("mmap(tracking_page) failed: %s\n", strerror(errno)); return 1;
    }
    printf("tracking_page ok\n");

    printf("sizeof(base_jd_atom_v2) = %zu\n", sizeof(struct base_jd_atom_v2));

    struct base_jd_atom_v2 atom;
    memset(&atom, 0, sizeof(atom));
    atom.jc = 0;
    atom.atom_number = 1;
    atom.core_req = BASE_JD_REQ_DEP;

    struct kbase_ioctl_job_submit submit = {
        .addr = (uint64_t)(uintptr_t)&atom,
        .nr_atoms = 1,
        .stride = sizeof(struct base_jd_atom_v2),
    };

    if (ioctl(fd, KBASE_IOCTL_JOB_SUBMIT, &submit)) {
        printf("JOB_SUBMIT failed: %s\n", strerror(errno));
        return 1;
    }
    printf("JOB_SUBMIT ok\n");

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int pret = poll(&pfd, 1, 2000);
    printf("poll returned %d, revents=0x%x\n", pret, pfd.revents);

    if (pret > 0 && (pfd.revents & POLLIN)) {
        struct base_jd_event_v2 event;
        ssize_t n = read(fd, &event, sizeof(event));
        printf("read %zd bytes: event_code=0x%x atom_number=%u\n",
               n, event.event_code, event.atom_number);
    } else {
        printf("no event available (timeout or no POLLIN)\n");
    }

    close(fd);
    return 0;
}
