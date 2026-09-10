#include <stdio.h>
#include <dlfcn.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

int main() {
    void *lib = dlopen("/data/data/com.termux/files/home/funnymdzz-mesa/build/src/panfrost/vulkan/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed\n"); return 1; }
    PFN_vkGetInstanceProcAddr icd_gpa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    PFN_vkEnumerateInstanceExtensionProperties EnumExt =
        (PFN_vkEnumerateInstanceExtensionProperties)icd_gpa(NULL, "vkEnumerateInstanceExtensionProperties");

    uint32_t count = 0;
    EnumExt(NULL, &count, NULL);
    VkExtensionProperties exts[128];
    if (count > 128) count = 128;
    EnumExt(NULL, &count, exts);
    for (uint32_t i = 0; i < count; i++)
        printf("%s\n", exts[i].extensionName);
    return 0;
}
