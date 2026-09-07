#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#define PAN_ARCH 7
#include "genxml/v7_pack.h"

#define KBASE_IOCTL_TYPE 0x80

struct kbase_ioctl_version_check { uint16_t major; uint16_t minor; };
#define KBASE_IOCTL_VERSION_CHECK _IOWR(KBASE_IOCTL_TYPE, 0, struct kbase_ioctl_version_check)

struct kbase_ioctl_set_flags { uint32_t create_flags; };
#define KBASE_IOCTL_SET_FLAGS _IOW(KBASE_IOCTL_TYPE, 1, struct kbase_ioctl_set_flags)

struct kbase_ioctl_job_submit { uint64_t addr; uint32_t nr_atoms; uint32_t stride; };
#define KBASE_IOCTL_JOB_SUBMIT _IOW(KBASE_IOCTL_TYPE, 2, struct kbase_ioctl_job_submit)

union kbase_ioctl_mem_alloc {
    struct { uint64_t va_pages, commit_pages, extension, flags; } in;
    struct { uint64_t flags, gpu_va; } out;
};
#define KBASE_IOCTL_MEM_ALLOC _IOWR(KBASE_IOCTL_TYPE, 5, union kbase_ioctl_mem_alloc)

#define BASE_MEM_MAP_TRACKING_HANDLE (3ull << 12)
#define BASE_MEM_PROT_CPU_RD ((uint64_t)1 << 0)
#define BASE_MEM_PROT_CPU_WR ((uint64_t)1 << 1)
#define BASE_MEM_PROT_GPU_RD ((uint64_t)1 << 2)
#define BASE_MEM_PROT_GPU_WR ((uint64_t)1 << 3)
#define BASE_MEM_COHERENT_LOCAL ((uint64_t)1 << 11)
#define BASE_MEM_SAME_VA ((uint64_t)1 << 13)

struct base_jd_udata { uint64_t blob[2]; };
struct base_dependency { uint8_t atom_id; uint8_t dependency_type; };
typedef uint32_t base_jd_core_req;

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

struct kbase_ioctl_mem_sync {
    uint64_t handle;
    uint64_t user_addr;
    uint64_t size;
    uint8_t type;
    uint8_t padding[7];
};
#define KBASE_IOCTL_MEM_SYNC _IOW(KBASE_IOCTL_TYPE, 15, struct kbase_ioctl_mem_sync)
#define BASE_SYNCSET_OP_CSYNC 2

struct base_jd_event_v2 {
    uint32_t event_code;
    uint8_t atom_number;
    uint8_t padding[3];
    struct base_jd_udata udata;
};

int main(void) {
    int fd = open("/dev/mali0", O_RDWR);
    if (fd < 0) { printf("open failed: %s\n", strerror(errno)); return 1; }

    struct kbase_ioctl_version_check ver = {0, 0};
    if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK, &ver)) {
        printf("VERSION_CHECK failed: %s\n", strerror(errno)); return 1;
    }
    printf("VERSION_CHECK ok: %u.%u\n", ver.major, ver.minor);

    struct kbase_ioctl_set_flags set_flags = { .create_flags = 0 };
    if (ioctl(fd, KBASE_IOCTL_SET_FLAGS, &set_flags)) {
        printf("SET_FLAGS failed: %s\n", strerror(errno)); return 1;
    }

    void *tracking_page = mmap(NULL, 4096, PROT_NONE, MAP_SHARED, fd, BASE_MEM_MAP_TRACKING_HANDLE);
    if (tracking_page == MAP_FAILED) {
        printf("mmap(tracking_page) failed: %s\n", strerror(errno)); return 1;
    }
    printf("handshake ok\n");

    union kbase_ioctl_mem_alloc alloc_req = {
        .in = {
            .va_pages = 1,
            .commit_pages = 1,
            .extension = 0,
            .flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                     BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_WR |
                     BASE_MEM_COHERENT_LOCAL | BASE_MEM_SAME_VA,
        }
    };
    if (ioctl(fd, KBASE_IOCTL_MEM_ALLOC, &alloc_req)) {
        printf("MEM_ALLOC failed: %s\n", strerror(errno)); return 1;
    }
    printf("MEM_ALLOC ok, gpu_va(cookie)=0x%llx flags=0x%llx\n",
           (unsigned long long)alloc_req.out.gpu_va,
           (unsigned long long)alloc_req.out.flags);

    void *cpu_ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, alloc_req.out.gpu_va);
    if (cpu_ptr == MAP_FAILED) {
        printf("mmap(bo) failed: %s\n", strerror(errno)); return 1;
    }
    uint64_t gpu_va = (uint64_t)(uintptr_t)cpu_ptr;
    printf("bo mapped at %p (gpu_va=0x%llx)\n", cpu_ptr, (unsigned long long)gpu_va);

    uint8_t *buf = (uint8_t *)cpu_ptr;
    uint64_t target_addr = gpu_va + 256;
    uint64_t *target_ptr = (uint64_t *)(buf + 256);

    *target_ptr = 0xCAFECAFECAFECAFEull;
    printf("target before job: 0x%llx\n", (unsigned long long)*target_ptr);

    struct MALI_JOB_HEADER header = {0};
    header.type = MALI_JOB_TYPE_WRITE_VALUE;
    header.index = 1;
    header.next = 0;
    header.invalidate_cache = true;

    struct mali_job_header_packed packed_header;
    MALI_JOB_HEADER_pack(&packed_header, &header);
    memcpy(buf + 0, &packed_header, sizeof(packed_header));

    struct MALI_WRITE_VALUE_JOB_PAYLOAD payload = {0};
    payload.address = target_addr;
    payload.type = MALI_WRITE_VALUE_TYPE_ZERO;
    payload.immediate_value = 0x1234567890ABCDEFull;

    struct mali_write_value_job_payload_packed packed_payload;
    MALI_WRITE_VALUE_JOB_PAYLOAD_pack(&packed_payload, &payload);
    memcpy(buf + 32, &packed_payload, sizeof(packed_payload));

    printf("job header+payload written at gpu_va=0x%llx\n", (unsigned long long)gpu_va);

    struct base_jd_atom_v2 atom;
    memset(&atom, 0, sizeof(atom));
    atom.jc = gpu_va;
    atom.atom_number = 1;
    atom.core_req = 0x10; /* BASE_JD_REQ_V - Requires value writeback */

    struct kbase_ioctl_job_submit submit = {
        .addr = (uint64_t)(uintptr_t)&atom,
        .nr_atoms = 1,
        .stride = sizeof(struct base_jd_atom_v2),
    };

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    if (ioctl(fd, KBASE_IOCTL_JOB_SUBMIT, &submit)) {
        printf("JOB_SUBMIT failed: %s\n", strerror(errno)); return 1;
    }
    printf("JOB_SUBMIT ok\n");

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int pret = poll(&pfd, 1, 3000);
    printf("poll returned %d, revents=0x%x\n", pret, pfd.revents);

    if (pret > 0 && (pfd.revents & POLLIN)) {
        struct base_jd_event_v2 event;
        ssize_t n = read(fd, &event, sizeof(event));
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        long usec = (t_end.tv_sec - t_start.tv_sec) * 1000000L + (t_end.tv_nsec - t_start.tv_nsec) / 1000L;
        printf("tempo submit->evento: %ld us\n", usec);

        printf("read %zd bytes: event_code=0x%x atom_number=%u\n",
               n, event.event_code, event.atom_number);
    } else {
        printf("no event (timeout)\n");
    }

    struct mali_job_header_packed *hdr_after = (struct mali_job_header_packed *)buf;
    printf("job header after job: opaque[0]=0x%x opaque[1]=0x%x opaque[4]=0x%x opaque[5]=0x%x\n", hdr_after->opaque[0], hdr_after->opaque[1], hdr_after->opaque[4], hdr_after->opaque[5]);
    struct kbase_ioctl_mem_sync sync_req = { .handle = gpu_va, .user_addr = (uint64_t)(uintptr_t)target_ptr, .size = 8, .type = BASE_SYNCSET_OP_CSYNC };
    if (ioctl(fd, KBASE_IOCTL_MEM_SYNC, &sync_req)) { printf("MEM_SYNC failed: %s\n", strerror(errno)); }

    printf("dump primeiros 64 bytes do buffer apos job:\n");
    for (int i = 0; i < 64; i++) { printf("%02x ", buf[i]); if ((i+1)%16==0) printf("\n"); }

    printf("target after job: 0x%llx (expected 0x0)\n",
           (unsigned long long)*target_ptr);

    close(fd);
    return 0;
}
