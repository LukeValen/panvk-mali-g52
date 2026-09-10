#include <stdio.h>
#include <dlfcn.h>
#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

int main() {
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) { printf("xcb_connect failed\n"); return 1; }
    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;

    xcb_window_t win = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root,
                       0, 0, 320, 240, 1,
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0, NULL);
    xcb_map_window(conn, win);
    xcb_flush(conn);
    printf("xcb window created\n"); fflush(stdout);

    void *lib = dlopen("/data/data/com.termux/files/home/funnymdzz-mesa/build/src/panfrost/vulkan/libvulkan_panfrost.so", RTLD_NOW);
    PFN_vkGetInstanceProcAddr icd_gpa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    PFN_vkCreateInstance CreateInstance = (PFN_vkCreateInstance)icd_gpa(NULL, "vkCreateInstance");

    const char *exts[] = {"VK_KHR_surface", "VK_KHR_xcb_surface"};
    VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .enabledExtensionCount = 2, .ppEnabledExtensionNames = exts };
    VkInstance instance;
    CreateInstance(&ici, NULL, &instance);
    printf("vkCreateInstance ok\n"); fflush(stdout);

    PFN_vkCreateXcbSurfaceKHR CreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)icd_gpa(instance, "vkCreateXcbSurfaceKHR");
    VkXcbSurfaceCreateInfoKHR surf_info = { .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, .connection = conn, .window = win };
    VkSurfaceKHR surface;
    if (CreateXcbSurfaceKHR(instance, &surf_info, NULL, &surface) != VK_SUCCESS) { printf("vkCreateXcbSurfaceKHR failed\n"); return 1; }
    printf("vkCreateXcbSurfaceKHR ok\n"); fflush(stdout);

    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)icd_gpa(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)icd_gpa(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    uint32_t pdCount = 1;
    VkPhysicalDevice pd;
    EnumeratePhysicalDevices(instance, &pdCount, &pd);
    printf("physical device enumerated\n"); fflush(stdout);

    printf("calling GetSurfaceCapabilitiesKHR (via XCB surface)...\n"); fflush(stdout);
    VkSurfaceCapabilitiesKHR caps;
    VkResult r = GetSurfaceCapabilitiesKHR(pd, surface, &caps);
    printf("GetSurfaceCapabilitiesKHR result=%d, extent=%ux%u\n", r, caps.currentExtent.width, caps.currentExtent.height);
    return 0;
}
