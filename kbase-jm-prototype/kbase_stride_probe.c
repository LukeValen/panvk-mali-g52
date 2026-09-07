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

struct base_jd_event_v2 { uint32_t event_code; uint8_t atom_number; uint8_t padding[3]; uint64_t udata[2]; };

int test_stride(int fd, void *cpu_ptr, uint64_t gpu_va, uint32_t stride, int atom_number_off, int core_req_off) {
    uint8_t *buf = (uint8_t *)cpu_ptr;
    uint64_t target_addr = gpu_va + 512;
    uint64_t *target_ptr = (uint64_t *)(buf + 512);
    *target_ptr = 0xCAFECAFECAFECAFEull;

    struct MALI_JOB_HEADER header = {0};
    header.type = MALI_JOB_TYPE_WRITE_VALUE;
    header.index = 1;
    header.invalidate_cache = true;
    struct mali_job_header_packed packed_header;
    MALI_JOB_HEADER_pack(&packed_header, &header);
    memcpy(buf + 0, &packed_header, sizeof(packed_header));

    struct MALI_WRITE_VALUE_JOB_PAYLOAD payload = {0};
    payload.address = target_addr;
    payload.type = MALI_WRITE_VALUE_TYPE_ZERO;
    struct mali_write_value_job_payload_packed packed_payload;
    MALI_WRITE_VALUE_JOB_PAYLOAD_pack(&packed_payload, &payload);
    memcpy(buf + 32, &packed_payload, sizeof(packed_payload));

    uint8_t atom_bytes[128];
    memset(atom_bytes, 0, sizeof(atom_bytes));
    uint64_t jc_value = gpu_va;
    memcpy(atom_bytes + 0, &jc_value, 8);
    if (atom_number_off >= 0) atom_bytes[atom_number_off] = 1;
    uint32_t core_req_value = 0;
    if (core_req_off >= 0) memcpy(atom_bytes + core_req_off, &core_req_value, 4);

    struct kbase_ioctl_job_submit submit = {
        .addr = (uint64_t)(uintptr_t)atom_bytes, .nr_atoms = 1, .stride = stride,
    };

    errno = 0;
    int ret = ioctl(fd, KBASE_IOCTL_JOB_SUBMIT, &submit);
    int err = errno;
    if (ret) {
        printf("stride=%u atom#off=%d core_req_off=%d -> JOB_SUBMIT FAILED: %s\n",
               stride, atom_number_off, core_req_off, strerror(err));
        return 0;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int pret = poll(&pfd, 1, 1000);
    uint32_t event_code = 0xffffffff;
    if (pret > 0 && (pfd.revents & POLLIN)) {
        struct base_jd_event_v2 event;
        read(fd, &event, sizeof(event));
        event_code = event.event_code;
    }

    int wrote = (*target_ptr == 0);
    printf("stride=%u atom#off=%d core_req_off=%d -> OK event=0x%x WROTE=%s\n",
           stride, atom_number_off, core_req_off, event_code, wrote ? "YES!!!" : "no");
    return wrote;
}

int main(void) {
    int fd = open("/dev/mali0", O_RDWR);
    if (fd < 0) { printf("open failed\n"); return 1; }

    struct kbase_ioctl_version_check ver = {0, 0};
    ioctl(fd, KBASE_IOCTL_VERSION_CHECK, &ver);
    struct kbase_ioctl_set_flags set_flags = { .create_flags = 0 };
    ioctl(fd, KBASE_IOCTL_SET_FLAGS, &set_flags);
    void *tracking_page = mmap(NULL, 4096, PROT_NONE, MAP_SHARED, fd, BASE_MEM_MAP_TRACKING_HANDLE);
    if (tracking_page == MAP_FAILED) { printf("tracking mmap failed\n"); return 1; }

    union kbase_ioctl_mem_alloc alloc_req = {
        .in = { .va_pages = 1, .commit_pages = 1, .extension = 0,
            .flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR | BASE_MEM_PROT_GPU_RD |
                     BASE_MEM_PROT_GPU_WR | BASE_MEM_COHERENT_LOCAL | BASE_MEM_SAME_VA }
    };
    ioctl(fd, KBASE_IOCTL_MEM_ALLOC, &alloc_req);
    void *cpu_ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, alloc_req.out.gpu_va);
    if (cpu_ptr == MAP_FAILED) { printf("bo mmap failed\n"); return 1; }
    uint64_t gpu_va = (uint64_t)(uintptr_t)cpu_ptr;
    printf("setup ok, gpu_va=0x%llx\n\n", (unsigned long long)gpu_va);

    int strides[] = {40, 44, 48, 52, 56, 60, 64, 72, 80, 96};
    for (int i = 0; i < 10; i++) {
        if (test_stride(fd, cpu_ptr, gpu_va, strides[i], 40, 44)) {
            printf(">>> SUCESSO em stride=%d! <<<\n", strides[i]);
        }
        usleep(50000);
    }

    close(fd);
    return 0;
}
