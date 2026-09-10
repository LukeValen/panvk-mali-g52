#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define CHECK(x, msg) do { VkResult r = (x); if (r != VK_SUCCESS) { printf("FAILED %s: VkResult=%d\n", msg, r); return 1; } printf("%s ok\n", msg); } while (0)

static PFN_vkGetInstanceProcAddr icd_gpa;
static VkInstance instance;
static PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
#define IPROC(name) (PFN_##name)icd_gpa(instance, #name)
#define DPROC(dev, name) (PFN_##name)GetDeviceProcAddr(dev, #name)

int main(void) {
    Display *xdpy = XOpenDisplay(":99");
    if (!xdpy) { printf("XOpenDisplay failed\n"); return 1; }
    printf("XOpenDisplay ok\n");

    int screen = DefaultScreen(xdpy);
    Window win = XCreateSimpleWindow(xdpy, RootWindow(xdpy, screen),
                                      0, 0, 320, 240, 1,
                                      BlackPixel(xdpy, screen), WhitePixel(xdpy, screen));
    XMapWindow(xdpy, win);
    XFlush(xdpy);
    printf("X11 window created and mapped\n");

    void *lib = dlopen("/data/data/com.termux/files/home/funnymdzz-mesa/build/src/panfrost/vulkan/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed\n"); return 1; }
    icd_gpa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");

    PFN_vkCreateInstance CreateInstance = (PFN_vkCreateInstance)icd_gpa(NULL, "vkCreateInstance");
    const char *ext_names[] = { "VK_KHR_surface", "VK_KHR_xlib_surface" };
    VkApplicationInfo app_info = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_0 };
    VkInstanceCreateInfo inst_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = ext_names,
    };
    CHECK(CreateInstance(&inst_info, NULL, &instance), "vkCreateInstance");

    PFN_vkCreateXlibSurfaceKHR CreateXlibSurfaceKHR = IPROC(vkCreateXlibSurfaceKHR);
    VkXlibSurfaceCreateInfoKHR surf_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = xdpy,
        .window = win,
    };
    VkSurfaceKHR surface;
    CHECK(CreateXlibSurfaceKHR(instance, &surf_info, NULL, &surface), "vkCreateXlibSurfaceKHR");

    GetDeviceProcAddr = IPROC(vkGetDeviceProcAddr);
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = IPROC(vkEnumeratePhysicalDevices);
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetSurfaceSupportKHR = IPROC(vkGetPhysicalDeviceSurfaceSupportKHR);
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetSurfaceCapabilitiesKHR = IPROC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetSurfaceFormatsKHR = IPROC(vkGetPhysicalDeviceSurfaceFormatsKHR);
    PFN_vkCreateDevice CreateDevice = IPROC(vkCreateDevice);

    uint32_t pcount = 1;
    VkPhysicalDevice pdev;
    EnumeratePhysicalDevices(instance, &pcount, &pdev);

    VkBool32 supported = VK_FALSE;
    CHECK(GetSurfaceSupportKHR(pdev, 0, surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
    printf("queue family 0 supports presentation: %s\n", supported ? "YES" : "NO");
    if (!supported) { printf("cannot present, aborting\n"); return 1; }

    VkSurfaceCapabilitiesKHR caps;
    CHECK(GetSurfaceCapabilitiesKHR(pdev, surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    printf("surface caps: minImageCount=%u maxImageCount=%u currentExtent=%ux%u\n",
           caps.minImageCount, caps.maxImageCount, caps.currentExtent.width, caps.currentExtent.height);

    uint32_t fmt_count = 0;
    GetSurfaceFormatsKHR(pdev, surface, &fmt_count, NULL);
    VkSurfaceFormatKHR formats[16];
    if (fmt_count > 16) fmt_count = 16;
    GetSurfaceFormatsKHR(pdev, surface, &fmt_count, formats);
    printf("surface format count: %u, first format=%d colorspace=%d\n",
           fmt_count, fmt_count ? formats[0].format : -1, fmt_count ? formats[0].colorSpace : -1);

    const char *dev_ext_names[] = { "VK_KHR_swapchain" };
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qinfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0, .queueCount = 1, .pQueuePriorities = &priority };
    VkDeviceCreateInfo dinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &qinfo,
        .enabledExtensionCount = 1, .ppEnabledExtensionNames = dev_ext_names,
    };
    VkDevice device;
    CHECK(CreateDevice(pdev, &dinfo, NULL, &device), "vkCreateDevice");

    printf("ALL SURFACE/SWAPCHAIN PREREQS PASSED\n");
    return 0;
}
