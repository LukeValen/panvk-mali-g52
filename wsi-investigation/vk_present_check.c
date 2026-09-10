#include <stdio.h>
#include <dlfcn.h>
#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

int main() {
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) { printf("xcb_connect failed\n"); return 1; }
    printf("xcb_connect ok\n"); fflush(stdout);
    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;

    void *lib = dlopen("/data/data/com.termux/files/home/funnymdzz-mesa/build/src/panfrost/vulkan/libvulkan_panfrost.so", RTLD_NOW);
    PFN_vkGetInstanceProcAddr icd_gpa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    PFN_vkCreateInstance CreateInstance = (PFN_vkCreateInstance)icd_gpa(NULL, "vkCreateInstance");

    const char *exts[] = {"VK_KHR_surface", "VK_KHR_xcb_surface"};
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = exts,
    };
    VkInstance instance;
    if (CreateInstance(&ici, NULL, &instance) != VK_SUCCESS) { printf("vkCreateInstance failed\n"); return 1; }
    printf("vkCreateInstance ok\n"); fflush(stdout);

    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)icd_gpa(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR XcbPresentationSupportKHR =
        (PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)icd_gpa(instance, "vkGetPhysicalDeviceXcbPresentationSupportKHR");

    uint32_t pdCount = 1;
    VkPhysicalDevice pd;
    EnumeratePhysicalDevices(instance, &pdCount, &pd);
    printf("physical device enumerated\n"); fflush(stdout);

    printf("calling vkGetPhysicalDeviceXcbPresentationSupportKHR...\n"); fflush(stdout);
    VkBool32 supported = XcbPresentationSupportKHR(pd, 0, conn, screen->root_visual);
    printf("XCB presentation support (queueFamily=0): %s\n", supported ? "YES" : "NO");
    return 0;
}
