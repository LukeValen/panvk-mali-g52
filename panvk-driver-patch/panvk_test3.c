#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

int main(void) {
    void *lib = dlopen("/data/data/com.termux/files/home/funnymdzz-mesa/build/src/panfrost/vulkan/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed: %s\n", dlerror()); return 1; }

    PFN_vkGetInstanceProcAddr icd_gpa =
        (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
    PFN_vkCreateInstance CreateInstance =
        (PFN_vkCreateInstance)icd_gpa(NULL, "vkCreateInstance");

    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "panvk_test3";
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    VkInstance instance;
    if (CreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
        printf("vkCreateInstance failed\n"); return 1;
    }
    printf("vkCreateInstance ok\n");

    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)icd_gpa(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetQueueFamilyProperties =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)icd_gpa(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    PFN_vkCreateDevice CreateDevice =
        (PFN_vkCreateDevice)icd_gpa(instance, "vkCreateDevice");
    PFN_vkDestroyDevice DestroyDevice =
        (PFN_vkDestroyDevice)icd_gpa(instance, "vkDestroyDevice");
    PFN_vkDeviceWaitIdle DeviceWaitIdle =
        (PFN_vkDeviceWaitIdle)icd_gpa(instance, "vkDeviceWaitIdle");

    uint32_t count = 0;
    EnumeratePhysicalDevices(instance, &count, NULL);
    if (count == 0) { printf("no physical devices\n"); return 1; }
    VkPhysicalDevice pdev;
    count = 1;
    EnumeratePhysicalDevices(instance, &count, &pdev);
    printf("physical device found\n");

    uint32_t qf_count = 0;
    GetQueueFamilyProperties(pdev, &qf_count, NULL);
    printf("queue families: %u\n", qf_count);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {0};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = 0;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;

    VkDeviceCreateInfo device_info = {0};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;

    VkDevice device;
    VkResult res = CreateDevice(pdev, &device_info, NULL, &device);
    if (res != VK_SUCCESS) {
        printf("vkCreateDevice FAILED: VkResult=%d\n", res);
        return 1;
    }
    printf("vkCreateDevice SUCCESS!\n");

    printf("calling vkDeviceWaitIdle...\n");
    res = DeviceWaitIdle(device);
    printf("vkDeviceWaitIdle result=%d\n", res);

    DestroyDevice(device, NULL);
    printf("vkDestroyDevice ok - ALL PASSED\n");

    return 0;
}
